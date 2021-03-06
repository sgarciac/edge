#include "src/terminal.h"

#include <glog/logging.h>

#include <algorithm>
#include <cctype>
#include <iostream>

#include "src/buffer_output_producer.h"
#include "src/buffer_variables.h"
#include "src/dirname.h"
#include "src/frame_output_producer.h"
#include "src/horizontal_split_output_producer.h"
#include "src/line_marks.h"
#include "src/parse_tree.h"
#include "src/status_output_producer.h"

namespace afc {
namespace editor {

using std::cerr;
using std::set;
using std::to_wstring;

constexpr int Terminal::DOWN_ARROW;
constexpr int Terminal::UP_ARROW;
constexpr int Terminal::LEFT_ARROW;
constexpr int Terminal::RIGHT_ARROW;
constexpr int Terminal::BACKSPACE;
constexpr int Terminal::PAGE_UP;
constexpr int Terminal::PAGE_DOWN;
constexpr int Terminal::ESCAPE;
constexpr int Terminal::CTRL_A;
constexpr int Terminal::CTRL_D;
constexpr int Terminal::CTRL_E;
constexpr int Terminal::CTRL_L;
constexpr int Terminal::CTRL_U;
constexpr int Terminal::CTRL_K;

Terminal::Terminal() : lines_cache_(1024) {}

void Terminal::Display(EditorState* editor_state, Screen* screen,
                       const EditorState::ScreenState& screen_state) {
  if (screen_state.needs_hard_redraw) {
    screen->HardRefresh();
    hashes_current_lines_.clear();
    lines_cache_.Clear();
  }
  screen->Move(LineNumber(0), ColumnNumber(0));

  StatusOutputProducerSupplier status_supplier(editor_state->status(), nullptr,
                                               editor_state->modifiers());
  std::vector<HorizontalSplitOutputProducer::Row> rows(2);
  rows[1].lines = status_supplier.lines();
  rows[0].lines = screen->lines() - rows[1].lines;

  auto buffer = editor_state->current_buffer();
  rows[0].producer = editor_state->buffer_tree()->CreateOutputProducer(
      {.size = LineColumnDelta(rows[0].lines, screen->columns()),
       .main_cursor_behavior =
           (editor_state->status()->GetType() == Status::Type::kPrompt ||
            (buffer != nullptr &&
             buffer->status()->GetType() == Status::Type::kPrompt))
               ? Widget::OutputProducerOptions::MainCursorBehavior::kHighlight
               : Widget::OutputProducerOptions::MainCursorBehavior::kIgnore});

  rows[1].producer = status_supplier.CreateOutputProducer(
      LineColumnDelta(rows[1].lines, screen->columns()));

  HorizontalSplitOutputProducer producer(
      std::move(rows),
      editor_state->status()->GetType() == Status::Type::kPrompt ? 1 : 0);

  for (auto line = LineNumber(); line.ToDelta() < screen->lines(); ++line) {
    WriteLine(screen, line, producer.Next());
  }

  if (editor_state->status()->GetType() == Status::Type::kPrompt ||
      (buffer != nullptr &&
       buffer->status()->GetType() == Status::Type::kPrompt) ||
      (buffer != nullptr && !buffer->Read(buffer_variables::atomic_lines) &&
       cursor_position_.has_value())) {
    screen->SetCursorVisibility(Screen::NORMAL);
    AdjustPosition(screen);
  } else {
    screen->SetCursorVisibility(Screen::INVISIBLE);
  }
  screen->Refresh();
  screen->Flush();
}

// Adjust the name of a buffer to a string suitable to be shown in the Status
// with progress indicators surrounding it.
//
// Empty strings -> "…"
// "$ xyz" -> "xyz"
// "$ abc/def/ghi" -> "ghi"
//
// The thinking is to return at most a single-character, and pick the most
// meaningful.
wstring TransformCommandNameForStatus(wstring name) {
  static const wstring kDefaultName = L"…";
  static const size_t kMaxLength = 5;

  size_t index = 0;
  if (name.size() > 2 && name[0] == L'$' && name[1] == L' ') {
    index = 2;
  }

  index = name.find_first_not_of(L' ', index);  // Skip spaces.
  if (index == string::npos) {
    return kDefaultName;
  }
  size_t end = name.find_first_of(L' ', index);
  wstring output = Basename(
      name.substr(index, end == string::npos ? string::npos : end - index));

  if (output.size() > kMaxLength) {
    output = output.substr(0, kMaxLength - kDefaultName.size()) + kDefaultName;
  }
  return output;
}

void FlushModifiers(Screen* screen, const LineModifierSet& modifiers) {
  screen->SetModifier(LineModifier::RESET);
  for (const auto& m : modifiers) {
    screen->SetModifier(m);
  }
}

void Terminal::WriteLine(Screen* screen, LineNumber line,
                         OutputProducer::Generator generator) {
  if (hashes_current_lines_.size() <= line.line) {
    CHECK_LT(line.ToDelta(), screen->lines());
    hashes_current_lines_.resize(screen->lines().line_delta * 2 + 50);
  }

  auto factory = [&] {
    return GetLineDrawer(generator.generate(), screen->columns());
  };

  LineDrawer no_hash_drawer;
  LineDrawer* drawer;
  if (generator.inputs_hash.has_value()) {
    if (hashes_current_lines_[line.line] == generator.inputs_hash.value()) {
      return;
    }
    drawer = lines_cache_.Get(generator.inputs_hash.value(), factory);
  } else {
    no_hash_drawer = factory();
    drawer = &no_hash_drawer;
  }

  VLOG(8) << "Generating line for screen: " << line;
  screen->Move(line, ColumnNumber(0));
  drawer->draw_callback(screen);
  hashes_current_lines_[line.line] = generator.inputs_hash;
  if (drawer->cursor.has_value()) {
    cursor_position_ = LineColumn(line, drawer->cursor.value());
  }
}

Terminal::LineDrawer Terminal::GetLineDrawer(
    OutputProducer::LineWithCursor line_with_cursor, ColumnNumberDelta width) {
  Terminal::LineDrawer output;
  std::vector<decltype(LineDrawer::draw_callback)> functions;

  CHECK(line_with_cursor.line != nullptr);
  VLOG(6) << "Writing line of length: "
          << line_with_cursor.line->EndColumn().ToDelta();
  ColumnNumber input_column;
  ColumnNumber output_column;

  functions.push_back(
      [](Screen* screen) { screen->SetModifier(LineModifier::RESET); });

  auto modifiers_it =
      line_with_cursor.line->modifiers().lower_bound(input_column);

  while (input_column < line_with_cursor.line->EndColumn() &&
         output_column < ColumnNumber(0) + width) {
    if (line_with_cursor.cursor.has_value() &&
        input_column == line_with_cursor.cursor.value()) {
      output.cursor = output_column;
    }

    // Each iteration will advance input_column and then print between start
    // and input_column.
    auto start = input_column;
    while ((input_column < line_with_cursor.line->EndColumn() &&
            output_column < ColumnNumber(0) + width &&
            (!line_with_cursor.cursor.has_value() ||
             input_column != line_with_cursor.cursor.value() ||
             output.cursor == output_column) &&
            (modifiers_it == line_with_cursor.line->modifiers().end() ||
             modifiers_it->first > input_column))) {
      output_column += ColumnNumberDelta(
          wcwidth(line_with_cursor.line->contents()->get(input_column)));
      ++input_column;
    }

    // TODO: Have screen receive the LazyString directly.
    if (start != input_column) {
      auto str = Substring(line_with_cursor.line->contents(), start,
                           input_column - start)
                     ->ToString();
      functions.push_back([str](Screen* screen) { screen->WriteString(str); });
    }

    if (modifiers_it != line_with_cursor.line->modifiers().end()) {
      CHECK_GE(modifiers_it->first, input_column);
      if (modifiers_it->first == input_column) {
        functions.push_back([modifiers = modifiers_it->second](Screen* screen) {
          FlushModifiers(screen, modifiers);
        });
        ++modifiers_it;
      }
    }
  }

  if (line_with_cursor.cursor.has_value() && !output.cursor.has_value()) {
    output.cursor = output_column;
  }

  if (output_column < ColumnNumber(0) + width) {
    functions.push_back([](Screen* screen) { screen->WriteString(L"\n"); });
  }
  output.draw_callback = [functions = std::move(functions)](Screen* screen) {
    for (auto& f : functions) {
      f(screen);
    }
  };
  return output;
}

void Terminal::AdjustPosition(Screen* screen) {
  CHECK(cursor_position_.has_value());
  VLOG(5) << "Setting cursor position: " << cursor_position_.value();
  screen->Move(cursor_position_.value().line, cursor_position_.value().column);
}

}  // namespace editor
}  // namespace afc
