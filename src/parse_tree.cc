#include "src/parse_tree.h"

extern "C" {
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
}

#include <glog/logging.h>

#include "src/buffer.h"

namespace std {
template <>
struct hash<afc::editor::LineModifierSet> {
  std::size_t operator()(const afc::editor::LineModifierSet& modifiers) const {
    size_t output;
    std::hash<size_t> hasher;
    for (auto& m : modifiers) {
      output ^= hasher(static_cast<size_t>(m)) + 0x9e3779b9 + (output << 6) +
                (output >> 2);
    }
    return output;
  }
};
}  // namespace std
namespace afc {
namespace editor {
namespace {
template <class T>
inline void hash_combine(std::size_t& seed, const T& v) {
  std::hash<T> hasher;
  seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}
}  // namespace

std::ostream& operator<<(std::ostream& os, const ParseTree& t) {
  os << "[ParseTree: " << t.range() << ", children: ";
  for (auto& c : t.children()) {
    os << c;
  }
  os << "]";
  return os;
}

Range ParseTree::range() const { return range_; }
void ParseTree::set_range(Range range) {
  range_ = range;
  RecomputeHashExcludingChildren();
}

size_t ParseTree::depth() const { return depth_; }

const LineModifierSet& ParseTree::modifiers() const { return modifiers_; }

void ParseTree::set_modifiers(LineModifierSet modifiers) {
  modifiers_ = std::move(modifiers);
  RecomputeHashExcludingChildren();
}

void ParseTree::InsertModifier(LineModifier modifier) {
  modifiers_.insert(modifier);
  RecomputeHashExcludingChildren();
}

const Tree<ParseTree>& ParseTree::children() const { return children_; }

std::unique_ptr<ParseTree, std::function<void(ParseTree*)>>
ParseTree::MutableChildren(size_t i) {
  CHECK_LT(i, children_.size());
  XorChildHash(i);  // Remove its old hash.
  return std::unique_ptr<ParseTree, std::function<void(ParseTree*)>>(
      &children_[i], [this, i](ParseTree* child) {
        depth_ = max(depth(), child->depth() + 1);
        XorChildHash(i);  // Add its new hash.
      });
}

void ParseTree::XorChildHash(size_t position) {
  size_t hash_for_child = position;
  hash_combine(hash_for_child, children_[position].hash());
  children_hashes_ ^= hash_for_child;
}

void ParseTree::Reset() {
  children_.clear();
  depth_ = 0;
  set_modifiers(LineModifierSet());
  children_hashes_ = 0;
}

std::unique_ptr<ParseTree, std::function<void(ParseTree*)>>
ParseTree::PushChild() {
  children_.emplace_back();
  XorChildHash(children_.size() - 1);  // Add its new hash.
  return MutableChildren(children_.size() - 1);
}

size_t ParseTree::hash() const {
  size_t output = hash_;
  hash_combine(output, children_hashes_);
  return output;
}

void ParseTree::RecomputeHashExcludingChildren() {
  hash_ = 0;
  hash_combine(hash_, range_);
  hash_combine(hash_, modifiers_);
  // No need to include depth_, that will come through children_hash_.
}

void SimplifyTree(const ParseTree& tree, ParseTree* output) {
  output->set_range(tree.range());
  for (const auto& child : tree.children()) {
    if (child.range().begin.line != child.range().end.line) {
      SimplifyTree(child, output->PushChild().get());
    }
  }
}

namespace {
void ZoomOutTree(const ParseTree& input, double ratio, ParseTree* parent) {
  Range range = input.range();
  range.begin.line.line *= ratio;
  range.end.line.line *= ratio;
  if (range.begin.line == range.end.line) {
    return;
  }
  auto output = parent->PushChild();
  output->set_range(range);
  for (const auto& child : input.children()) {
    ZoomOutTree(child, ratio, output.get());
  }
}
}  // namespace

ParseTree ZoomOutTree(const ParseTree& input, LineNumberDelta input_lines,
                      LineNumberDelta output_lines) {
  LOG(INFO) << "Zooming out: " << input_lines << " to " << output_lines;
  ParseTree output;
  ZoomOutTree(
      input,
      static_cast<double>(output_lines.line_delta) / input_lines.line_delta,
      &output);
  if (output.children().empty()) {
    return ParseTree();
  }

  CHECK_EQ(output.children().size(), 1ul);
  return std::move(output.children().at(0));
}

// Returns the first children of tree that ends after a given position.
size_t FindChildrenForPosition(const ParseTree* tree,
                               const LineColumn& position) {
  for (size_t i = 0; i < tree->children().size(); i++) {
    if (tree->children().at(i).range().Contains(position)) {
      return i;
    }
  }
  return tree->children().size();
}

ParseTree::Route FindRouteToPosition(const ParseTree& root,
                                     const LineColumn& position) {
  ParseTree::Route output;
  auto tree = &root;
  for (;;) {
    size_t index = FindChildrenForPosition(tree, position);
    if (index == tree->children().size()) {
      return output;
    }
    output.push_back(index);
    tree = &tree->children().at(index);
  }
}

std::vector<const ParseTree*> MapRoute(const ParseTree& root,
                                       const ParseTree::Route& route) {
  std::vector<const ParseTree*> output = {&root};
  for (auto& index : route) {
    output.push_back(&output.back()->children().at(index));
  }
  return output;
}

const ParseTree* FollowRoute(const ParseTree& root,
                             const ParseTree::Route& route) {
  auto tree = &root;
  for (auto& index : route) {
    CHECK_LT(index, tree->children().size());
    tree = &tree->children().at(index);
  }
  return tree;
}

namespace {
class NullTreeParser : public TreeParser {
 public:
  void FindChildren(const BufferContents&, ParseTree* root) override {
    CHECK(root != nullptr);
    root->Reset();
  }
};

class CharTreeParser : public TreeParser {
 public:
  void FindChildren(const BufferContents& buffer, ParseTree* root) override {
    CHECK(root != nullptr);
    root->Reset();
    for (auto line = root->range().begin.line; line <= root->range().end.line;
         ++line) {
      CHECK_LE(line, buffer.EndLine());
      auto contents = buffer.at(line);
      ColumnNumber end_column =
          line == root->range().end.line
              ? root->range().end.column
              : contents->EndColumn() + ColumnNumberDelta(1);
      for (ColumnNumber i = line == root->range().begin.line
                                ? root->range().begin.column
                                : ColumnNumber(0);
           i < end_column; ++i) {
        auto new_children = root->PushChild();
        new_children->set_range(Range::InLine(line, i, ColumnNumberDelta(1)));
        DVLOG(7) << "Adding char: " << *new_children;
      }
    }
  }
};

class WordsTreeParser : public TreeParser {
 public:
  WordsTreeParser(std::wstring symbol_characters,
                  std::unordered_set<wstring> typos,
                  std::unique_ptr<TreeParser> delegate)
      : symbol_characters_(symbol_characters),
        typos_(typos),
        delegate_(std::move(delegate)) {}

  void FindChildren(const BufferContents& buffer, ParseTree* root) override {
    CHECK(root != nullptr);
    root->Reset();
    for (auto line = root->range().begin.line; line <= root->range().end.line;
         line++) {
      const auto& contents = *buffer.at(line);

      ColumnNumber line_end = contents.EndColumn();
      if (line == root->range().end.line) {
        line_end = min(line_end, root->range().end.column);
      }

      ColumnNumber column = line == root->range().begin.line
                                ? root->range().begin.column
                                : ColumnNumber(0);
      while (column < line_end) {
        auto new_children = root->PushChild();

        Range range;
        while (column < line_end && IsSpace(contents, column)) {
          column++;
        }
        range.begin = LineColumn(line, column);

        while (column < line_end && !IsSpace(contents, column)) {
          column++;
        }
        range.end = LineColumn(line, column);

        if (range.IsEmpty()) {
          return;
        }

        CHECK_GT(range.end.column, range.begin.column);
        auto keyword = Substring(contents.contents(), range.begin.column,
                                 range.end.column - range.begin.column)
                           ->ToString();
        if (typos_.find(keyword) != typos_.end()) {
          new_children->InsertModifier(LineModifier::RED);
        }
        new_children->set_range(range);

        DVLOG(6) << "Adding word: " << *new_children;
        delegate_->FindChildren(buffer, new_children.get());
      }
    }
  }

 private:
  bool IsSpace(const Line& line, ColumnNumber column) {
    return symbol_characters_.find(line.get(column)) == symbol_characters_.npos;
  }

  const std::wstring symbol_characters_;
  const std::unordered_set<wstring> typos_;
  const std::unique_ptr<TreeParser> delegate_;
};

class LineTreeParser : public TreeParser {
 public:
  LineTreeParser(std::unique_ptr<TreeParser> delegate)
      : delegate_(std::move(delegate)) {}

  void FindChildren(const BufferContents& buffer, ParseTree* root) override {
    CHECK(root != nullptr);
    root->Reset();
    DVLOG(5) << "Finding lines: " << *root;
    for (auto line = root->range().begin.line; line <= root->range().end.line;
         line++) {
      auto contents = buffer.at(line);
      if (contents->empty()) {
        continue;
      }

      auto new_children = root->PushChild();
      new_children->set_range(Range(
          LineColumn(line),
          min(LineColumn(line, contents->EndColumn()), root->range().end)));
      DVLOG(5) << "Adding line: " << *new_children;
      delegate_->FindChildren(buffer, new_children.get());
    }
  }

 private:
  const std::unique_ptr<TreeParser> delegate_;
};

}  // namespace

/* static */ bool TreeParser::IsNull(TreeParser* parser) {
  return dynamic_cast<NullTreeParser*>(parser) != nullptr;
}

std::unique_ptr<TreeParser> NewNullTreeParser() {
  return std::make_unique<NullTreeParser>();
}

std::unique_ptr<TreeParser> NewCharTreeParser() {
  return std::make_unique<CharTreeParser>();
}

std::unique_ptr<TreeParser> NewWordsTreeParser(
    wstring symbol_characters, std::unordered_set<wstring> typos,
    std::unique_ptr<TreeParser> delegate) {
  return std::make_unique<WordsTreeParser>(
      std::move(symbol_characters), std::move(typos), std::move(delegate));
}

std::unique_ptr<TreeParser> NewLineTreeParser(
    std::unique_ptr<TreeParser> delegate) {
  return std::make_unique<LineTreeParser>(std::move(delegate));
}

}  // namespace editor
}  // namespace afc
