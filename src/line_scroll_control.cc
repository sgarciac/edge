#include "src/line_scroll_control.h"

#include <glog/logging.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>

#include "src/buffer.h"
#include "src/buffer_output_producer.h"
#include "src/buffer_variables.h"
#include "src/buffer_widget.h"
#include "src/vertical_split_output_producer.h"
#include "src/widget.h"
#include "src/wstring.h"

namespace afc {
namespace editor {
LineScrollControl::Reader::Reader(ConstructorAccessTag,
                                  std::shared_ptr<LineScrollControl> parent)
    : parent_(std::move(parent)) {
  CHECK(parent_ != nullptr);
}

LineScrollControl::LineScrollControl(ConstructorAccessTag, Options options)
    : options_(std::move(options)),
      cursors_([=]() {
        std::map<size_t, std::set<size_t>> cursors;
        for (auto cursor : *options_.buffer->active_cursors()) {
          cursors[cursor.line].insert(cursor.column);
        }
        return cursors;
      }()),
      range_(GetRange(options_.begin)) {}

std::unique_ptr<LineScrollControl::Reader> LineScrollControl::NewReader() {
  auto output = std::make_unique<LineScrollControl::Reader>(
      Reader::ConstructorAccessTag(), shared_from_this());
  readers_.push_back(output.get());
  return output;
}

std::optional<Range> LineScrollControl::Reader::GetRange() const {
  switch (state_) {
    case State::kDone:
      return std::nullopt;
    case State::kProcessing:
      return parent_->range_;
  }
  LOG(FATAL) << "GetRange didn't handle all cases.";
  return std::nullopt;
}

bool LineScrollControl::Reader::HasActiveCursor() const {
  CHECK(state_ == State::kProcessing);
  return parent_->range_.Contains(parent_->options_.buffer->position().line);
}

std::set<size_t> LineScrollControl::Reader::GetCurrentCursors() const {
  CHECK(state_ == State::kProcessing);
  size_t line = parent_->range_.begin.line;
  auto it = parent_->cursors_.find(line);
  if (it == parent_->cursors_.end()) {
    return {};
  }
  std::set<size_t> output;
  for (auto& column : it->second) {
    if (parent_->range_.Contains(LineColumn(line, column))) {
      output.insert(column - parent_->range_.begin.column);
    }
  }
  return output;
}

void LineScrollControl::SignalReaderDone() {
  if (++readers_done_ < readers_.size()) {
    return;
  }
  readers_done_ = 0;
  VLOG(6) << "Advancing, finished range: " << range_;
  range_ = GetRange(range_.end);
  VLOG(7) << "Next range: " << range_;

  for (auto& c : readers_) {
    c->state_ = Reader::State::kProcessing;
  }
}

Range LineScrollControl::GetRange(LineColumn begin) {
  // TODO: This is wrong: it doesn't account for multi-width characters.
  // TODO: This is wrong: it doesn't take into account line filters.
  if (begin.line >= options_.buffer->lines_size()) {
    return Range(begin, LineColumn::Max());
  }

  auto line = options_.buffer->LineAt(begin.line);
  if (options_.buffer->Read(buffer_variables::wrap_from_content)) {
    LOG(INFO) << "Skipping spaces (from " << begin << ").";
    while (begin.column < line->size() && line->get(begin.column) == L' ') {
      begin.column++;
    }
  }

  LineColumn end(begin.line, begin.column + options_.columns_shown);
  if (end.column < options_.buffer->LineAt(end.line)->size() &&
      options_.buffer->Read(buffer_variables::wrap_long_lines)) {
    if (options_.buffer->Read(buffer_variables::wrap_from_content)) {
      auto symbols = options_.buffer->Read(buffer_variables::symbol_characters);
      auto line = options_.buffer->LineAt(end.line);
      auto read = [&](size_t column) { return line->get(column); };
      bool moved = false;
      while (end > begin && symbols.find(read(end.column)) != symbols.npos) {
        end.column--;
        moved = true;
      }
      if (moved) {
        end.column++;
      }
      if (end.column <= begin.column + 1) {
        LOG(INFO) << "Giving up, line exceeds width.";
        end.column = begin.column + options_.columns_shown;
      }
    }
    return Range(begin, end);
  }
  end.line++;
  end.column = options_.initial_column;
  if (end.line >= options_.buffer->lines_size()) {
    end = LineColumn(std::numeric_limits<size_t>::max());
  }
  return Range(begin, end);
}

}  // namespace editor
}  // namespace afc
