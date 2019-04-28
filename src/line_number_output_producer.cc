#include "src/line_number_output_producer.h"

#include <glog/logging.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>

#include "src/buffer.h"
#include "src/buffer_output_producer.h"
#include "src/buffer_variables.h"
#include "src/buffer_widget.h"
#include "src/line_scroll_control.h"
#include "src/vertical_split_output_producer.h"
#include "src/widget.h"
#include "src/wstring.h"

namespace afc {
namespace editor {

/* static */ size_t LineNumberOutputProducer::PrefixWidth(size_t lines_size) {
  return 1 + std::to_wstring(lines_size).size();
}

LineNumberOutputProducer::LineNumberOutputProducer(
    std::shared_ptr<OpenBuffer> buffer,
    std::unique_ptr<LineScrollControl::Reader> line_scroll_control_reader)
    : width_(PrefixWidth(buffer->lines_size())),
      buffer_(std::move(buffer)),
      line_scroll_control_reader_(std::move(line_scroll_control_reader)) {}

void LineNumberOutputProducer::WriteLine(Options options) {
  auto range = line_scroll_control_reader_->GetRange();
  if (range.has_value() && range.value().begin.line >= buffer_->lines_size()) {
    return;  // Happens when the buffer is smaller than the screen.
  }

  std::wstring number =
      range.has_value() ? std::to_wstring(range.value().begin.line + 1) : L"↪";
  CHECK_LE(number.size(), width_ - 1);
  std::wstring padding(width_ - number.size() - 1, L' ');
  if (!range.has_value() ||
      line_scroll_control_reader_->GetCurrentCursors().empty()) {
    options.receiver->AddModifier(LineModifier::DIM);
  } else if (line_scroll_control_reader_->HasActiveCursor() ||
             buffer_->Read(buffer_variables::multiple_cursors)) {
    options.receiver->AddModifier(LineModifier::CYAN);
    options.receiver->AddModifier(LineModifier::BOLD);
  } else {
    options.receiver->AddModifier(LineModifier::BLUE);
  }
  options.receiver->AddString(padding + number + L':');

  if (range.has_value()) {
    line_scroll_control_reader_->RangeDone();
  }
}

size_t LineNumberOutputProducer::width() const { return width_; }

}  // namespace editor
}  // namespace afc