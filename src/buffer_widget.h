#ifndef __AFC_EDITOR_BUFFER_LEAF_H__
#define __AFC_EDITOR_BUFFER_LEAF_H__

#include <list>
#include <memory>

#include "src/output_producer.h"
#include "src/parse_tree.h"
#include "src/tree.h"
#include "src/widget.h"

namespace afc {
namespace editor {

class BufferWidget : public Widget {
 private:
  struct ConstructorAccessTag {};

 public:
  static std::unique_ptr<BufferWidget> New(std::weak_ptr<OpenBuffer> buffer);

  BufferWidget(ConstructorAccessTag, std::weak_ptr<OpenBuffer> buffer);

  wstring Name() const override;
  wstring ToString() const override;

  BufferWidget* GetActiveLeaf() override;

  std::unique_ptr<OutputProducer> CreateOutputProducer() override;

  void SetSize(size_t lines, size_t columns) override;
  size_t lines() const override;
  size_t columns() const override;
  size_t MinimumLines() override;
  LineColumn view_start() const;

  // Custom methods.
  std::shared_ptr<OpenBuffer> Lock() const;
  void SetBuffer(std::weak_ptr<OpenBuffer> buffer);

 private:
  // When either leaf_ or lines_ changes, recomputes internal data.
  void RecomputeData();

  std::weak_ptr<OpenBuffer> leaf_;
  size_t lines_ = 0;
  size_t columns_ = 0;

  // The position in the buffer where the view begins.
  LineColumn view_start_;

  std::shared_ptr<const ParseTree> simplified_parse_tree_;
  std::shared_ptr<const ParseTree> zoomed_out_tree_;
};

}  // namespace editor
}  // namespace afc

#endif  // __AFC_EDITOR_BUFFER_LEAF_H__