#include "buffers_list.h"

#include <algorithm>
#include <ctgmath>

#include "src/buffer.h"
#include "src/buffer_variables.h"
#include "src/buffer_widget.h"
#include "src/dirname.h"
#include "src/horizontal_split_output_producer.h"
#include "src/status.h"
#include "src/widget.h"

namespace afc {
namespace editor {

namespace {
class BuffersListProducer : public OutputProducer {
 public:
  BuffersListProducer(std::map<wstring, std::shared_ptr<OpenBuffer>>* buffers,
                      std::shared_ptr<OpenBuffer> active_buffer_,
                      size_t buffers_per_line)
      : buffers_(buffers),
        active_buffer_(std::move(active_buffer_)),
        buffers_per_line_(buffers_per_line),
        prefix_width_(std::to_wstring(buffers_->size()).size() + 2),
        buffers_iterator_(buffers->begin()) {
    VLOG(1) << "BuffersList created. Buffers per line: " << buffers_per_line
            << ", prefix width: " << prefix_width_
            << ", count: " << buffers->size();
  }

  void WriteLine(Options options) override {
    VLOG(2) << "BuffersListProducer::WriteLine start at "
            << options.receiver->column() << " and output "
            << options.receiver->width();
    // Excluding prefixes and separators.
    //
    // TODO: Move this to constructor? Our customer can tell us the width in
    // advance?
    const ColumnNumberDelta columns_per_buffer =
        (options.receiver->width() -
         std::min(options.receiver->width(),
                  (prefix_width_ * buffers_per_line_))) /
        buffers_per_line_;
    VLOG(2) << "Columns per buffer: " << columns_per_buffer;
    for (size_t i = 0;
         i < buffers_per_line_ && buffers_iterator_ != buffers_->end();
         i++, buffers_iterator_++, index_++) {
      auto buffer = buffers_iterator_->second;
      options.receiver->AddModifier(LineModifier::RESET);
      auto name = buffers_iterator_->first;
      auto number_prefix = std::to_wstring(index_ + 1);
      ColumnNumber start =
          ColumnNumber(0) + (columns_per_buffer + prefix_width_) * i;
      if (options.receiver->column() < start) {
        options.receiver->AddString(ColumnNumberDelta::PaddingString(
            start - options.receiver->column(), L' '));
      }

      options.receiver->AddModifier(LineModifier::CYAN);
      if (buffer == active_buffer_) {
        options.receiver->AddModifier(LineModifier::BOLD);
        options.receiver->AddModifier(LineModifier::REVERSE);
      }
      start += prefix_width_ - ColumnNumberDelta(number_prefix.size() + 2);
      if (options.receiver->column() < start) {
        options.receiver->AddString(ColumnNumberDelta::PaddingString(
            start - options.receiver->column(), L' '));
      }
      options.receiver->AddString(number_prefix);
      options.receiver->AddModifier(LineModifier::RESET);

      std::list<std::wstring> output_components;
      std::list<std::wstring> components;
      if (buffer != nullptr && buffer->Read(buffer_variables::path) == name &&
          DirectorySplit(name, &components) && !components.empty()) {
        name.clear();
        output_components.push_front(components.back());
        if (ColumnNumberDelta(output_components.front().size()) >
            columns_per_buffer) {
          output_components.front() = output_components.front().substr(
              output_components.front().size() -
              columns_per_buffer.column_delta);
        } else {
          size_t consumed = output_components.front().size();
          components.pop_back();

          static const size_t kSizeOfSlash = 1;
          while (!components.empty()) {
            if (columns_per_buffer >
                ColumnNumberDelta(components.size() * 2 +
                                  components.back().size() + consumed)) {
              output_components.push_front(components.back());
              consumed += components.back().size() + kSizeOfSlash;
            } else if (columns_per_buffer >
                       ColumnNumberDelta(1 + kSizeOfSlash + consumed)) {
              output_components.push_front(
                  std::wstring(1, components.back()[0]));
              consumed += 1 + kSizeOfSlash;
            } else {
              break;
            }
            components.pop_back();
          }
        }
      }

      wstring progress;
      std::optional<LineModifier> progress_modifier;
      if (!buffer->GetLineMarks()->empty()) {
        progress = L"!";
        progress_modifier = LineModifier::RED;
      } else if (buffer->ShouldDisplayProgress()) {
        progress = ProgressString(buffer->Read(buffer_variables::progress),
                                  OverflowBehavior::kModulo);
      } else {
        progress = ProgressStringFillUp(buffer->lines_size().line_delta,
                                        OverflowBehavior::kModulo);
        progress_modifier = LineModifier::DIM;
      }
      // If we ever make ProgressString return more than a single character,
      // we'll have to adjust this.
      CHECK_LE(progress.size(), 1ul);

      if (progress_modifier.has_value()) {
        options.receiver->AddModifier(progress_modifier.value());
      }
      options.receiver->AddString(progress);
      options.receiver->AddModifier(LineModifier::RESET);

      if (!name.empty()) {
        options.receiver->AddString(name);
        continue;
      }

      auto last = output_components.end();
      --last;
      for (auto it = output_components.begin(); it != output_components.end();
           ++it) {
        if (it != output_components.begin()) {
          options.receiver->AddModifier(LineModifier::DIM);
          options.receiver->AddCharacter(L'/');
          options.receiver->AddModifier(LineModifier::RESET);
        }
        if (it == last) {
          options.receiver->AddModifier(LineModifier::BOLD);
        }
        options.receiver->AddString(*it);
      }
      options.receiver->AddModifier(LineModifier::RESET);
    }
  }

 private:
  const std::map<wstring, std::shared_ptr<OpenBuffer>>* const buffers_;
  const std::shared_ptr<OpenBuffer> active_buffer_;
  const size_t buffers_per_line_;
  const ColumnNumberDelta prefix_width_;

  int line_ = 0;
  std::map<wstring, std::shared_ptr<OpenBuffer>>::iterator buffers_iterator_;
  size_t index_ = 0;
};
}  // namespace

BuffersList::BuffersList(std::unique_ptr<Widget> widget)
    : widget_(std::move(widget)) {
  CHECK(widget_ != nullptr);
}

void BuffersList::AddBuffer(std::shared_ptr<OpenBuffer> buffer,
                            AddBufferType add_buffer_type) {
  CHECK(buffer != nullptr);
  switch (add_buffer_type) {
    case AddBufferType::kVisit:
      buffers_[buffer->Read(buffer_variables::name)] = buffer;
      GetActiveLeaf()->SetBuffer(buffer);
      break;

    case AddBufferType::kOnlyList:
      buffers_[buffer->Read(buffer_variables::name)] = buffer;
      break;

    case AddBufferType::kIgnore:
      break;
  }
}

void BuffersList::RemoveBuffer(OpenBuffer* buffer) {
  CHECK(widget_ != nullptr);
  CHECK(buffer != nullptr);
  buffers_.erase(buffer->Read(buffer_variables::name));
  CHECK(widget_ != nullptr);
  widget_->RemoveBuffer(buffer);
}

size_t BuffersList::CountLeaves() const { return widget_->CountLeaves(); }

int BuffersList::AdvanceActiveLeafWithoutWrapping(int delta) {
  CHECK(widget_ != nullptr);
  return widget_->AdvanceActiveLeafWithoutWrapping(delta);
}

void BuffersList::SetActiveLeavesAtStart() {
  CHECK(widget_ != nullptr);
  widget_->SetActiveLeavesAtStart();
}

std::shared_ptr<OpenBuffer> BuffersList::GetBuffer(size_t index) {
  CHECK_LT(index, buffers_.size());
  auto it = buffers_.begin();
  std::advance(it, index);
  return it->second;
}

size_t BuffersList::GetCurrentIndex() {
  auto buffer = GetActiveLeaf()->Lock();
  if (buffer == nullptr) {
    return 0;
  }
  auto it = buffers_.find(buffer->Read(buffer_variables::name));
  if (it == buffers_.end()) {
    return 0;
  }
  return std::distance(buffers_.begin(), it);
}

size_t BuffersList::BuffersCount() const { return buffers_.size(); }

wstring BuffersList::Name() const {
  CHECK(widget_ != nullptr);
  return widget_->Name();
}

wstring BuffersList::ToString() const {
  return L"BuffersList: " + widget_->Name();
}

BufferWidget* BuffersList::GetActiveLeaf() {
  return const_cast<BufferWidget*>(
      const_cast<const BuffersList*>(this)->GetActiveLeaf());
}

const BufferWidget* BuffersList::GetActiveLeaf() const {
  CHECK(widget_ != nullptr);
  return widget_->GetActiveLeaf();
}

std::unique_ptr<OutputProducer> BuffersList::CreateOutputProducer() {
  CHECK(widget_ != nullptr);
  auto output = widget_->CreateOutputProducer();
  if (buffers_list_lines_ == LineNumberDelta(0)) {
    return output;
  }

  std::vector<HorizontalSplitOutputProducer::Row> rows;
  rows.push_back({std::move(output), lines_ - buffers_list_lines_});

  if (buffers_list_lines_ > LineNumberDelta(0)) {
    rows.push_back(
        {std::make_unique<BuffersListProducer>(
             &buffers_, widget_->GetActiveLeaf()->Lock(), buffers_per_line_),
         buffers_list_lines_});
  }

  return std::make_unique<HorizontalSplitOutputProducer>(std::move(rows), 0);
}

void BuffersList::SetSize(LineNumberDelta lines, ColumnNumberDelta columns) {
  lines_ = lines;
  columns_ = columns;

  static const auto kMinimumColumnsPerBuffer = ColumnNumberDelta(20);

  buffers_list_lines_ = LineNumberDelta(
      ceil(static_cast<double>(
               (buffers_.size() * kMinimumColumnsPerBuffer).column_delta) /
           columns_.column_delta));
  buffers_per_line_ = ceil(static_cast<double>(buffers_.size()) /
                           buffers_list_lines_.line_delta);

  widget_->SetSize(lines_ - buffers_list_lines_, columns_);
}

LineNumberDelta BuffersList::lines() const { return lines_; }
ColumnNumberDelta BuffersList::columns() const { return columns_; }
LineNumberDelta BuffersList::MinimumLines() { return LineNumberDelta(0); }

Widget* BuffersList::Child() { return widget_.get(); }

void BuffersList::SetChild(std::unique_ptr<Widget> widget) {
  CHECK(widget != nullptr);
  widget_ = std::move(widget);
}

void BuffersList::WrapChild(
    std::function<std::unique_ptr<Widget>(std::unique_ptr<Widget>)> callback) {
  widget_ = callback(std::move(widget_));
  CHECK(widget_ != nullptr);
}

}  // namespace editor
}  // namespace afc
