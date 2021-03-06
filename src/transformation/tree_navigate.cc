#include "src/transformation/tree_navigate.h"

#include "src/buffer.h"
#include "src/futures/futures.h"
#include "src/parse_tree.h"
#include "src/seek.h"
#include "src/transformation/composite.h"
#include "src/transformation/set_position.h"
#include "src/vm_transformation.h"

namespace afc::editor {
namespace {
class TreeNavigate : public CompositeTransformation {
  std::wstring Serialize() const override { return L"TreeNavigate()"; }
  futures::Value<Output> Apply(Input input) const override {
    auto root = input.buffer->parse_tree();
    if (root == nullptr) return futures::Past(Output());
    const ParseTree* tree = root.get();
    auto next_position = input.position;
    Seek(*input.buffer->contents(), &next_position).Once();

    while (true) {
      // Find the first relevant child at the current level.
      size_t child = 0;
      while (child < tree->children().size() &&
             (tree->children()[child].range().end <= input.position ||
              tree->children()[child].children().empty())) {
        child++;
      }

      if (child >= tree->children().size()) {
        break;
      }

      auto candidate = &tree->children()[child];
      if (tree->range().begin >= input.position &&
          (tree->range().end != next_position ||
           candidate->range().end != next_position)) {
        break;
      }
      tree = candidate;
    }

    auto last_position = tree->range().end;
    Seek(*input.buffer->contents(), &last_position).Backwards().Once();
    return futures::Past(Output::SetPosition(
        input.position == last_position ? tree->range().begin : last_position));
  }

  std::unique_ptr<CompositeTransformation> Clone() const override {
    return std::make_unique<TreeNavigate>();
  }
};
}  // namespace

// TODO(easy): Change return type to Composite.
transformation::Variant NewTreeNavigateTransformation() {
  return std::make_unique<TreeNavigate>();
}
}  // namespace afc::editor
