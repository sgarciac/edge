#ifndef __AFC_EDITOR_TRANSFORMATION_COMPOSITE_H__
#define __AFC_EDITOR_TRANSFORMATION_COMPOSITE_H__

#include <memory>

#include "src/buffer_contents.h"
#include "src/futures/futures.h"
#include "src/transformation.h"
#include "src/transformation/stack.h"
#include "src/vm/public/environment.h"

namespace afc::editor {
class CompositeTransformation;

class CompositeTransformationAdapter : public Transformation {
 public:
  CompositeTransformationAdapter(
      Modifiers modifiers,
      std::unique_ptr<CompositeTransformation> composite_transformation);

  futures::Value<Result> Apply(
      const Input& transformation_input) const override;

  std::unique_ptr<Transformation> Clone() const override;

 private:
  const Modifiers modifiers_;
  const std::unique_ptr<CompositeTransformation> composite_transformation_;
};

// A particular type of transformation that doesn't directly modify the buffer
// but only does so indirectly, through other transformations (that it passes to
// Input::push).
//
// Ideally, most transformations will be expressed through this, so that we can
// isolate the lower-level primitive transformations.
class CompositeTransformation {
 public:
  virtual std::wstring Serialize() const = 0;

  struct Input {
    LineColumn original_position;
    // Adjusted to ensure that it is within the length of the current line.
    LineColumn position;
    Range range;
    EditorState* editor;
    const OpenBuffer* buffer;
    Modifiers modifiers;
    Transformation::Input::Mode mode;
  };

  class Output {
   public:
    static Output SetPosition(LineColumn position);
    static Output SetColumn(ColumnNumber column);
    Output();
    Output(Output&&) = default;
    Output(std::unique_ptr<Transformation> transformation);
    void Push(std::unique_ptr<Transformation> transformation);

   private:
    friend CompositeTransformationAdapter;
    std::unique_ptr<TransformationStack> transformations_;
  };
  virtual futures::Value<Output> Apply(Input input) const = 0;
  virtual std::unique_ptr<CompositeTransformation> Clone() const = 0;
};

std::unique_ptr<Transformation> NewTransformation(
    Modifiers modifiers, std::unique_ptr<CompositeTransformation> composite);

void RegisterCompositeTransformation(vm::Environment* environment);
}  // namespace afc::editor
namespace afc::vm {
template <>
struct VMTypeMapper<std::shared_ptr<editor::CompositeTransformation::Output>> {
  static std::shared_ptr<editor::CompositeTransformation::Output> get(
      Value* value);
  static Value::Ptr New(
      std::shared_ptr<editor::CompositeTransformation::Output> value);
  static const VMType vmtype;
};
template <>
struct VMTypeMapper<std::shared_ptr<editor::CompositeTransformation::Input>> {
  static std::shared_ptr<editor::CompositeTransformation::Input> get(
      Value* value);
  static Value::Ptr New(
      std::shared_ptr<editor::CompositeTransformation::Input> value);
  static const VMType vmtype;
};
}  // namespace afc::vm
#endif  // __AFC_EDITOR_TRANSFORMATION_COMPOSITE_H__
