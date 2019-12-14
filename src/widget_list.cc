#include "src/widget_list.h"

#include <glog/logging.h>

#include <algorithm>
#include <cctype>
#include <ctgmath>
#include <iostream>
#include <numeric>

#include "src/buffer.h"
#include "src/buffer_output_producer.h"
#include "src/buffer_variables.h"
#include "src/buffer_widget.h"
#include "src/buffers_list.h"
#include "src/frame_output_producer.h"
#include "src/horizontal_split_output_producer.h"
#include "src/vertical_split_output_producer.h"
#include "src/widget.h"
#include "src/wstring.h"

namespace afc {
namespace editor {

BufferWidget* WidgetList::GetActiveLeaf() {
  return const_cast<BufferWidget*>(
      const_cast<const WidgetList*>(this)->GetActiveLeaf());
}

const BufferWidget* WidgetList::GetActiveLeaf() const {
  CHECK(!children_.empty());
  CHECK_LT(active_, children_.size());
  CHECK(children_[active_] != nullptr);
  return children_[active_]->GetActiveLeaf();
}

void WidgetList::ForEachBufferWidget(
    std::function<void(BufferWidget*)> callback) {
  for (auto& widget : children_) {
    widget->ForEachBufferWidget(callback);
  }
}
void WidgetList::ForEachBufferWidgetConst(
    std::function<void(const BufferWidget*)> callback) const {
  for (const auto& widget : children_) {
    widget->ForEachBufferWidgetConst(callback);
  }
}

void WidgetList::SetSize(LineColumnDelta size) { size_ = size; }

void WidgetList::RemoveBuffer(OpenBuffer* buffer) {
  for (auto& child : children_) {
    child->RemoveBuffer(buffer);
  }
}

size_t WidgetList::count() const { return children_.size(); }

size_t WidgetList::index() const { return active_; }

void WidgetList::set_index(size_t position) {
  CHECK(!children_.empty());
  active_ = position % children_.size();
}

void WidgetList::AddChild(std::unique_ptr<Widget> widget) {
  children_.push_back(std::move(widget));
  set_index(children_.size() - 1);
  SetSize(size_);
}

Widget* WidgetList::Child() { return children_[active_].get(); }

void WidgetList::SetChild(std::unique_ptr<Widget> widget) {
  children_[active_] = std::move(widget);
}

void WidgetList::WrapChild(
    std::function<std::unique_ptr<Widget>(std::unique_ptr<Widget>)> callback) {
  children_[active_] = callback(std::move(children_[active_]));
}

size_t WidgetList::CountLeaves() const {
  int count = 0;
  for (const auto& child : children_) {
    count += child->CountLeaves();
  }
  return count;
}

int WidgetList::AdvanceActiveLeafWithoutWrapping(int delta) {
  LOG(INFO) << "WidgetList advances leaf: " << delta;
  while (delta > 0) {
    delta = children_[active_]->AdvanceActiveLeafWithoutWrapping(delta);
    if (active_ == children_.size() - 1) {
      return delta;
    } else if (delta > 0) {
      delta--;
      active_++;
    }
  }
  return delta;
}

void WidgetList::SetActiveLeavesAtStart() {
  active_ = 0;
  children_[active_]->SetActiveLeavesAtStart();
}

void WidgetList::RemoveActiveLeaf() {
  CHECK_LT(active_, children_.size());
  if (children_.size() == 1) {
    children_[0] = BufferWidget::New(std::weak_ptr<OpenBuffer>());
  } else {
    children_.erase(children_.begin() + active_);
    active_ %= children_.size();
  }
  CHECK_LT(active_, children_.size());
  SetSize(size_);
}

WidgetList::WidgetList(std::vector<std::unique_ptr<Widget>> children,
                       size_t active)
    : children_(std::move(children)), active_(active) {}

WidgetList::WidgetList(std::unique_ptr<Widget> children)
    : WidgetList(
          [&]() {
            std::vector<std::unique_ptr<Widget>> output;
            output.push_back(std::move(children));
            return output;
          }(),
          0) {}

WidgetListHorizontal::WidgetListHorizontal(std::unique_ptr<Widget> children)
    : WidgetList(
          [&]() {
            std::vector<std::unique_ptr<Widget>> output;
            output.push_back(std::move(children));
            return output;
          }(),
          0) {}

WidgetListHorizontal::WidgetListHorizontal(
    std::vector<std::unique_ptr<Widget>> children, size_t active)
    : WidgetList(std::move(children), active) {}

wstring WidgetListHorizontal::Name() const { return L""; }

wstring WidgetListHorizontal::ToString() const {
  wstring output = L"[buffer tree horizontal, children: " +
                   std::to_wstring(children_.size()) + L", active: " +
                   std::to_wstring(active_) + L"]";
  return output;
}

std::unique_ptr<OutputProducer> WidgetListHorizontal::CreateOutputProducer() {
  std::vector<HorizontalSplitOutputProducer::Row> rows;
  CHECK_EQ(children_.size(), lines_per_child_.size());
  for (size_t index = 0; index < children_.size(); index++) {
    auto child_producer = children_[index]->CreateOutputProducer();
    CHECK(child_producer != nullptr);
    std::shared_ptr<const OpenBuffer> buffer =
        children_[index]->GetActiveLeaf()->Lock();
    if (children_.size() > 1) {
      VLOG(5) << "Producing row with frame.";
      std::vector<HorizontalSplitOutputProducer::Row> nested_rows;
      FrameOutputProducer::FrameOptions frame_options;
      frame_options.title = children_[index]->Name();
      frame_options.position_in_parent = index;
      if (index == active_) {
        frame_options.active_state =
            FrameOutputProducer::FrameOptions::ActiveState::kActive;
      }
      if (buffer != nullptr) {
        frame_options.extra_information =
            OpenBuffer::FlagsToString(buffer->Flags());
      }
      nested_rows.push_back(
          {std::make_unique<FrameOutputProducer>(std::move(frame_options)),
           LineNumberDelta(1)});
      nested_rows.push_back({std::move(child_producer),
                             lines_per_child_[index] - LineNumberDelta(1)});
      child_producer = std::make_unique<HorizontalSplitOutputProducer>(
          std::move(nested_rows), 1);
    }
    CHECK(child_producer != nullptr);
    rows.push_back({std::move(child_producer), lines_per_child_[index]});
  }

  return std::make_unique<HorizontalSplitOutputProducer>(std::move(rows),
                                                         active_);
}

void WidgetListHorizontal::SetSize(LineColumnDelta size) {
  WidgetList::SetSize(size);
  lines_per_child_.clear();
  if (size.line == LineNumberDelta(0)) return;
  for (auto& child : children_) {
    lines_per_child_.push_back(child->MinimumLines());
  }

  if (children_.size() > 1) {
    LOG(INFO) << "Adding lines for frames.";
    for (auto& lines : lines_per_child_) {
      if (lines > LineNumberDelta(0)) {
        static const LineNumberDelta kFrameLines(1);
        lines += kFrameLines;
      }
    }
  }

  LineNumberDelta lines_given = std::accumulate(
      lines_per_child_.begin(), lines_per_child_.end(), LineNumberDelta(0));

  // TODO: this could be done way faster (sort + single pass over all
  // buffers).
  while (lines_given > size_.line) {
    LOG(INFO) << "Ensuring that lines given (" << lines_given
              << ") doesn't exceed lines available (" << size_.line << ").";
    std::vector<size_t> indices_maximal_producers = {0};
    for (size_t i = 1; i < lines_per_child_.size(); i++) {
      LineNumberDelta maximum =
          lines_per_child_[indices_maximal_producers.front()];
      if (maximum < lines_per_child_[i]) {
        indices_maximal_producers = {i};
      } else if (maximum == lines_per_child_[i]) {
        indices_maximal_producers.push_back(i);
      }
    }
    CHECK(!indices_maximal_producers.empty());
    CHECK_GT(lines_per_child_[indices_maximal_producers[0]],
             LineNumberDelta(0));
    for (auto& i : indices_maximal_producers) {
      if (lines_given > size_.line) {
        lines_given--;
        lines_per_child_[i]--;
      }
    }
  }

  CHECK_EQ(lines_given,
           std::accumulate(lines_per_child_.begin(), lines_per_child_.end(),
                           LineNumberDelta(0)));

  if (size_.line > lines_given) {
    LineNumberDelta lines_each =
        (size_.line - lines_given) / lines_per_child_.size();
    lines_given += lines_per_child_.size() * lines_each;
    for (auto& l : lines_per_child_) {
      LineNumberDelta extra_line =
          lines_given < size_.line ? LineNumberDelta(1) : LineNumberDelta(0);
      l += lines_each + extra_line;
      lines_given += extra_line;
    }
  }

  CHECK_EQ(size_.line,
           std::accumulate(lines_per_child_.begin(), lines_per_child_.end(),
                           LineNumberDelta(0)));

  for (size_t i = 0; i < lines_per_child_.size(); i++) {
    children_[i]->SetSize(LineColumnDelta(
        lines_per_child_[i] -
            (children_.size() > 1 ? LineNumberDelta(1) : LineNumberDelta(0)),
        size_.column));
  }
}

LineNumberDelta WidgetListHorizontal::MinimumLines() {
  LineNumberDelta count;
  for (auto& child : children_) {
    static const LineNumberDelta kFrameLines = LineNumberDelta(1);
    count += child->MinimumLines() + kFrameLines;
  }
  return count;
}

WidgetListVertical::WidgetListVertical(std::unique_ptr<Widget> children)
    : WidgetList(std::move(children)) {}

WidgetListVertical::WidgetListVertical(
    std::vector<std::unique_ptr<Widget>> children, size_t active)
    : WidgetList(std::move(children), active) {}

wstring WidgetListVertical::Name() const { return L""; }

wstring WidgetListVertical::ToString() const {
  wstring output = L"[buffer tree vertical, children: " +
                   std::to_wstring(children_.size()) + L", active: " +
                   std::to_wstring(active_) + L"]";
  return output;
}

std::unique_ptr<OutputProducer> WidgetListVertical::CreateOutputProducer() {
  std::vector<VerticalSplitOutputProducer::Column> columns;
  CHECK_EQ(children_.size(), columns_per_child_.size());

  for (size_t index = 0; index < children_.size(); index++) {
    auto child_producer = children_[index]->CreateOutputProducer();
    CHECK(child_producer != nullptr);
    std::shared_ptr<const OpenBuffer> buffer =
        children_[index]->GetActiveLeaf()->Lock();
    columns.push_back({std::move(child_producer), columns_per_child_[index]});
  }

  return std::make_unique<VerticalSplitOutputProducer>(std::move(columns),
                                                       active_);
}

void WidgetListVertical::SetSize(LineColumnDelta size) {
  WidgetList::SetSize(size);
  columns_per_child_.clear();

  if (size.line == LineNumberDelta()) return;

  ColumnNumberDelta base_columns = size_.column / children_.size();
  ColumnNumberDelta columns_left =
      size_.column - base_columns * children_.size();
  CHECK_LT(columns_left, ColumnNumberDelta(children_.size()));
  for (auto& unused __attribute__((unused)) : children_) {
    columns_per_child_.push_back(base_columns);
    if (columns_left > ColumnNumberDelta(0)) {
      ++columns_per_child_.back();
      --columns_left;
    }
  }
  CHECK_EQ(columns_left, ColumnNumberDelta(0));
  for (size_t i = 0; i < columns_per_child_.size(); i++) {
    children_[i]->SetSize(LineColumnDelta(size_.line, columns_per_child_[i]));
  }
}

LineNumberDelta WidgetListVertical::MinimumLines() {
  LineNumberDelta output = children_[0]->MinimumLines();
  for (auto& child : children_) {
    output = max(child->MinimumLines(), output);
  }
  static const LineNumberDelta kFrameLines = LineNumberDelta(1);
  return output + kFrameLines;
}

}  // namespace editor
}  // namespace afc
