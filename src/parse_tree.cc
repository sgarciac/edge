#include "src/parse_tree.h"

extern "C" {
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
}

#include <glog/logging.h>

#include "src/buffer.h"
#include "src/hash.h"

namespace afc {
namespace editor {
std::ostream& operator<<(std::ostream& os, const ParseTree& t) {
  os << "[ParseTree: " << t.range() << ", children: ";
  for (auto& c : t.children()) {
    os << c;
  }
  os << "]";
  return os;
}

ParseTree::ParseTree(Range range) : range_(std::move(range)) {}

ParseTree::ParseTree(const ParseTree& other)
    : children_(other.children()),
      children_hashes_(other.children_hashes_),
      range_(other.range()),
      depth_(other.depth()),
      modifiers_(other.modifiers()) {}

Range ParseTree::range() const { return range_; }
void ParseTree::set_range(Range range) { range_ = range; }

size_t ParseTree::depth() const { return depth_; }

const LineModifierSet& ParseTree::modifiers() const { return modifiers_; }

void ParseTree::set_modifiers(LineModifierSet modifiers) {
  modifiers_ = std::move(modifiers);
}

void ParseTree::InsertModifier(LineModifier modifier) {
  modifiers_.insert(modifier);
}

const std::vector<ParseTree>& ParseTree::children() const { return children_; }

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
  children_hashes_ ^= hash_combine(position, children_[position].hash());
}

void ParseTree::Reset() {
  children_.clear();
  children_hashes_ = 0;
  depth_ = 0;
  set_modifiers(LineModifierSet());
}

void ParseTree::PushChild(ParseTree child) {
  depth_ = max(depth(), child.depth() + 1);
  children_.push_back(std::move(child));
  XorChildHash(children_.size() - 1);
}

size_t ParseTree::hash() const {
  return hash_combine(std::hash<Range>{}(range_),
                      std::hash<LineModifierSet>{}(modifiers_),
                      children_hashes_);
}

ParseTree SimplifyTree(const ParseTree& tree) {
  ParseTree output(tree.range());
  for (const auto& child : tree.children()) {
    if (child.range().begin.line != child.range().end.line) {
      output.PushChild(SimplifyTree(child));
    }
  }
  return output;
}

namespace {
std::optional<ParseTree> ZoomOutTree(const ParseTree& input, double ratio) {
  Range range = input.range();
  range.begin.line.line *= ratio;
  range.end.line.line *= ratio;
  if (range.begin.line == range.end.line) {
    return std::nullopt;
  }
  ParseTree output(range);
  for (const auto& child : input.children()) {
    auto output_child = ZoomOutTree(child, ratio);
    if (output_child.has_value()) {
      output.PushChild(std::move(output_child.value()));
    }
  }
  return output;
}
}  // namespace

ParseTree ZoomOutTree(const ParseTree& input, LineNumberDelta input_lines,
                      LineNumberDelta output_lines) {
  LOG(INFO) << "Zooming out: " << input_lines << " to " << output_lines;
  auto output =
      ZoomOutTree(input, static_cast<double>(output_lines.line_delta) /
                             input_lines.line_delta);
  if (!output.has_value()) {
    return ParseTree();
  }

  return output.value();
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
  ParseTree FindChildren(const BufferContents&, Range range) override {
    return ParseTree(range);
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

  ParseTree FindChildren(const BufferContents& buffer, Range range) override {
    ParseTree output(range);
    range.ForEachLine([&](LineNumber line) {
      const auto& contents = *buffer.at(line);

      ColumnNumber line_end = contents.EndColumn();
      if (line == range.end.line) {
        line_end = min(line_end, range.end.column);
      }

      ColumnNumber column =
          line == range.begin.line ? range.begin.column : ColumnNumber(0);
      while (column < line_end) {
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
        ParseTree child = delegate_->FindChildren(buffer, range);
        if (typos_.find(keyword) != typos_.end()) {
          child.InsertModifier(LineModifier::RED);
        }
        DVLOG(6) << "Adding word: " << child;
        output.PushChild(std::move(child));
      }
    });
    return output;
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

  ParseTree FindChildren(const BufferContents& buffer, Range range) override {
    ParseTree output(range);
    range.ForEachLine([&](LineNumber line) {
      auto contents = buffer.at(line);
      if (contents->empty()) {
        return;
      }

      output.PushChild(delegate_->FindChildren(
          buffer,
          Range(LineColumn(line),
                min(LineColumn(line, contents->EndColumn()), range.end))));
    });
    return output;
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
