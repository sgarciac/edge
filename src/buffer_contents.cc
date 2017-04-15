#include "buffer_contents.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <unordered_set>

#include "char_buffer.h"
#include "editor.h"
#include "lazy_string_append.h"
#include "substring.h"
#include "wstring.h"

namespace afc {
namespace editor {

wstring BufferContents::ToString() const {
  wstring output;
  output.reserve(CountCharacters());
  ForEach([&output](size_t position, const Line& line) {
            output.append((position == 0 ? L"" : L"\n") + line.ToString());
            return true;
          });
  return output;
}

void BufferContents::insert(size_t position, const BufferContents& source,
                            size_t first_line, size_t last_line) {
  CHECK_LT(position, size());
  CHECK_LT(first_line, source.size());
  CHECK_LE(first_line, last_line);
  CHECK_LE(last_line, source.size());
  lines_.insert(lines_.begin() + position, source.lines_.begin() + first_line,
                source.lines_.begin() + last_line);
  NotifyUpdateListeners(nullptr);
}

bool BufferContents::ForEach(
    const std::function<bool(size_t, const Line&)>& callback) const {
  size_t position = 0;
  for (const auto& line : lines_) {
    if (!callback(position++, *line)) { return false; }
  }
  return true;
}

void BufferContents::ForEach(
    const std::function<void(const Line&)>& callback) const {
  ForEach([callback](size_t, const Line& line) {
            callback(line);
            return true;
          });
}

void BufferContents::ForEach(const std::function<void(wstring)>& callback)
    const {
  ForEach([callback](const Line& line) { callback(line.ToString()); });
}

size_t BufferContents::CountCharacters() const {
  size_t output = 0;
  ForEach([&output](const Line& line) {
    output += line.size() + 1;  // \n.
  });
  if (output > 0) {
    output--;  // Last line has no \n.
  }
  return output;
}

void BufferContents::DeleteCharactersFromLine(
    size_t line, size_t column, size_t amount) {
  if (amount == 0) { return; }
  CHECK_LE(column + amount, at(line)->size());

  auto new_line = std::make_shared<Line>(*at(line));
  new_line->DeleteCharacters(column, amount);
  set_line(line, new_line);

  NotifyUpdateListeners(
      [line, column, amount](LineColumn position) {
        if (position.line == line) {
          if (position.column > column + amount) {
            position.column -= amount;
          } else if (position.column > column) {
            position.column = column;
          }
        }
        return position;
      });
}

void BufferContents::DeleteCharactersFromLine(size_t line, size_t column) {
  return DeleteCharactersFromLine(line, column, at(line)->size() - column);
}

void BufferContents::SetCharacter(size_t line, size_t column, int c,
    std::unordered_set<Line::Modifier, hash<int>> modifiers) {
  auto new_line = std::make_shared<Line>(*at(line));
  new_line->SetCharacter(column, c, modifiers);
  set_line(line, new_line);
  NotifyUpdateListeners(nullptr);
}

void BufferContents::InsertCharacter(size_t line, size_t column) {
  auto new_line = std::make_shared<Line>(*at(line));
  new_line->InsertCharacterAtPosition(column);
  set_line(line, new_line);
  NotifyUpdateListeners(nullptr);
}

void BufferContents::AppendToLine(
    size_t position, const Line& line_to_append) {
  if (lines_.empty()) {
    push_back(std::make_shared<Line>());
  }
  CHECK(!lines_.empty());
  position = min(position, size() - 1);
  auto line = std::make_shared<Line>(*at(position));
  line->Append(line_to_append);
  set_line(position, line);
  NotifyUpdateListeners(nullptr);
}

void BufferContents::EraseLines(size_t first, size_t last) {
  if (first == last) {
    return;  // Optimization to avoid notifying listeners.
  }
  CHECK_LE(first, last);
  CHECK_LE(last, size());
  LOG(INFO) << "Erasing lines in range [" << first << ", " << last << ").";
  lines_.erase(lines_.begin() + first, lines_.begin() + last);
  NotifyUpdateListeners(
      [last, first](LineColumn position) {
        if (position.line >= last) {
          position.line -= last - first;
        } else if (position.line >= first) {
          position.line = first;
        }
        return position;
      });
}

void BufferContents::SplitLine(size_t line, size_t column) {
  auto tail = std::make_shared<Line>(*at(line));
  tail->DeleteCharacters(0, column);
  insert_line(line + 1, tail);
  NotifyUpdateListeners(
      [line, column](LineColumn position) {
        if (position.line > line) {
          position.line++;
        } else if (position.line == line && position.column >= column) {
          position.line++;
          position.column -= column;
        }
        return position;
      });
  DeleteCharactersFromLine(line, column);
}

void BufferContents::FoldNextLine(size_t position) {
  if (position + 1 >= size()) {
    return;
  }
  size_t initial_size = at(position)->size();
  AppendToLine(position, *at(position + 1));
  NotifyUpdateListeners(
      [position, initial_size](LineColumn cursor) {
        if (cursor.line == position + 1) {
          cursor.line--;
          cursor.column += initial_size;
        }
        return cursor;
      });
  EraseLines(position + 1, position + 2);
}

void BufferContents::AddUpdateListener(
    std::function<void(const CursorAdjuster&)> listener) {
  CHECK(listener);
  update_listeners_.push_back(listener);
}

void BufferContents::NotifyUpdateListeners(
    const CursorAdjuster& cursor_adjuster) {
  for (auto& l : update_listeners_) {
    l(cursor_adjuster);
  }
}

}  // namespace editor
}  // namespace afc