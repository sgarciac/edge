#include "src/line.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <unordered_set>

#include "src/buffer_variables.h"
#include "src/char_buffer.h"
#include "src/editor.h"
#include "src/hash.h"
#include "src/lazy_string_append.h"
#include "src/lazy_string_functional.h"
#include "src/substring.h"
#include "src/tracker.h"
#include "src/wstring.h"

namespace afc {
namespace editor {

using std::hash;
using std::unordered_set;
using std::wstring;

Line::Options::Options(Line line)
    : contents(std::move(line.contents())),
      modifiers(std::move(line.modifiers())),
      end_of_line_modifiers(line.end_of_line_modifiers()),
      environment(line.environment()) {}

ColumnNumber Line::Options::EndColumn() const {
  // TODO: Compute this separately, taking the width of characters into account.
  return ColumnNumber(0) + contents->size();
}

void Line::Options::SetCharacter(ColumnNumber column, int c,
                                 const LineModifierSet& c_modifiers) {
  ValidateInvariants();
  auto str = NewLazyString(wstring(1, c));
  if (column >= EndColumn()) {
    column = EndColumn();
    contents = StringAppend(std::move(contents), std::move(str));
  } else {
    contents = StringAppend(
        StringAppend(afc::editor::Substring(std::move(contents),
                                            ColumnNumber(0), column.ToDelta()),
                     std::move(str)),
        afc::editor::Substring(contents, column + ColumnNumberDelta(1)));
  }

  LineModifierSet previous_modifiers;
  if (!modifiers.empty() && modifiers.begin()->first <= column) {
    auto it = modifiers.lower_bound(column);
    previous_modifiers =
        (it == modifiers.end() ? modifiers.begin() : --it)->second;
  }
  if (c_modifiers != previous_modifiers) {
    modifiers[column] = c_modifiers;
    if (column + ColumnNumberDelta(1) < EndColumn()) {
      modifiers[column + ColumnNumberDelta(1)] = previous_modifiers;
    }
    ValidateInvariants();
  }
}

void Line::Options::InsertCharacterAtPosition(ColumnNumber column) {
  ValidateInvariants();
  contents = StringAppend(
      StringAppend(
          afc::editor::Substring(contents, ColumnNumber(0), column.ToDelta()),
          NewLazyString(L" ")),
      afc::editor::Substring(contents, column));

  std::map<ColumnNumber, LineModifierSet> new_modifiers;
  for (auto& m : modifiers) {
    new_modifiers[m.first + (m.first < column ? ColumnNumberDelta(0)
                                              : ColumnNumberDelta(1))] =
        std::move(m.second);
  }
  modifiers = std::move(new_modifiers);

  ValidateInvariants();
}

void Line::Options::AppendCharacter(wchar_t c, LineModifierSet modifier) {
  ValidateInvariants();
  CHECK(modifier.find(LineModifier::RESET) == modifier.end());
  modifiers[ColumnNumber(0) + contents->size()] = modifier;
  end_of_line_modifiers = std::move(modifier);
  contents = StringAppend(std::move(contents), NewLazyString(wstring(1, c)));
  ValidateInvariants();
}

void Line::Options::AppendString(std::shared_ptr<LazyString> suffix) {
  AppendString(std::move(suffix), std::nullopt);
}

void Line::Options::AppendString(
    std::shared_ptr<LazyString> suffix,
    std::optional<LineModifierSet> suffix_modifiers) {
  ValidateInvariants();
  Line::Options suffix_line;
  suffix_line.contents = std::move(suffix);
  if (suffix_modifiers.has_value()) {
    if (suffix_line.contents->size() > ColumnNumberDelta(0)) {
      suffix_line.modifiers[ColumnNumber(0)] = suffix_modifiers.value();
    }
    suffix_line.end_of_line_modifiers = suffix_modifiers.value();
  }
  Append(Line(std::move(suffix_line)));
  ValidateInvariants();
}

void Line::Options::AppendString(std::wstring suffix,
                                 std::optional<LineModifierSet> modifiers) {
  AppendString(NewLazyString(std::move(suffix)), std::move(modifiers));
}

void Line::Options::Append(Line line) {
  ValidateInvariants();
  if (line.empty()) return;
  auto original_length = EndColumn().ToDelta();
  contents = StringAppend(std::move(contents), std::move(line.contents()));

  LineModifierSet current_modifiers;
  if (!modifiers.empty()) {
    current_modifiers = std::move(end_of_line_modifiers);
  }

  if (!current_modifiers.empty() &&
      line.modifiers().find(ColumnNumber(0)) == line.modifiers().end()) {
    modifiers[ColumnNumber(0) + original_length] = {};  // Reset.
    current_modifiers.clear();
  }

  for (auto& m : line.modifiers()) {
    if (m.second != current_modifiers) {
      current_modifiers = m.second;
      modifiers[m.first + original_length] = std::move(m.second);
    }
  }

  end_of_line_modifiers = std::move(line.end_of_line_modifiers());
  ValidateInvariants();
}

Line::Options& Line::Options::DeleteCharacters(ColumnNumber column,
                                               ColumnNumberDelta delta) {
  ValidateInvariants();
  CHECK_GE(delta, ColumnNumberDelta(0));
  CHECK_LE(column, EndColumn());
  CHECK_LE(column + delta, EndColumn());

  contents = StringAppend(
      afc::editor::Substring(contents, ColumnNumber(0), column.ToDelta()),
      afc::editor::Substring(contents, column + delta));

  std::map<ColumnNumber, LineModifierSet> new_modifiers;
  // TODO: We could optimize this to only set it once (rather than for every
  // modifier before the deleted range).
  std::optional<LineModifierSet> last_modifiers_before_gap;
  std::optional<LineModifierSet> modifiers_continuation;
  for (auto& m : modifiers) {
    if (m.first < column) {
      last_modifiers_before_gap = m.second;
      new_modifiers[m.first] = std::move(m.second);
    } else if (m.first < column + delta) {
      modifiers_continuation = std::move(m.second);
    } else {
      new_modifiers[m.first - delta] = std::move(m.second);
    }
  }
  if (modifiers_continuation.has_value() &&
      new_modifiers.find(column) == new_modifiers.end() &&
      last_modifiers_before_gap != modifiers_continuation &&
      column + delta < EndColumn()) {
    new_modifiers[column] = modifiers_continuation.value();
  }
  modifiers = std::move(new_modifiers);

  ValidateInvariants();
  return *this;
}

Line::Options& Line::Options::DeleteSuffix(ColumnNumber column) {
  return DeleteCharacters(column, EndColumn() - column);
}

void Line::Options::ValidateInvariants() { CHECK(contents != nullptr); }

/* static */ std::shared_ptr<Line> Line::New(Options options) {
  return std::make_shared<Line>(std::move(options));
}

Line::Line(wstring x) : Line(Line::Options(NewLazyString(std::move(x)))) {}

Line::Line(Options options)
    : environment_(options.environment == nullptr
                       ? std::make_shared<Environment>()
                       : std::move(options.environment)),
      options_(std::move(options)) {
  ValidateInvariants();
}

Line::Line(const Line& line) {
  std::unique_lock<std::mutex> lock(line.mutex_);
  environment_ = line.environment_;
  options_ = line.options_;
  hash_ = line.hash_;
}

std::shared_ptr<LazyString> Line::contents() const {
  std::unique_lock<std::mutex> lock(mutex_);
  return options_.contents;
}

ColumnNumber Line::EndColumn() const {
  std::unique_lock<std::mutex> lock(mutex_);
  return EndColumnWithLock();
}

bool Line::empty() const {
  CHECK(contents() != nullptr);
  return EndColumn().IsZero();
}

wint_t Line::get(ColumnNumber column) const {
  std::unique_lock<std::mutex> lock(mutex_);
  return GetWithLock(column);
}

shared_ptr<LazyString> Line::Substring(ColumnNumber column,
                                       ColumnNumberDelta delta) const {
  return afc::editor::Substring(contents(), column, delta);
}

shared_ptr<LazyString> Line::Substring(ColumnNumber column) const {
  return afc::editor::Substring(contents(), column);
}

void Line::SetAllModifiers(const LineModifierSet& modifiers) {
  std::unique_lock<std::mutex> lock(mutex_);
  ValidateInvariants();
  options_.modifiers.clear();
  options_.modifiers[ColumnNumber(0)] = modifiers;
  options_.end_of_line_modifiers = std::move(modifiers);
  hash_ = std::nullopt;
  ValidateInvariants();
}

void Line::Append(const Line& line) {
  if (line.empty()) return;
  std::unique_lock<std::mutex> lock(mutex_);
  ValidateInvariants();
  line.ValidateInvariants();
  CHECK(this != &line);
  hash_ = std::nullopt;
  auto original_length = EndColumnWithLock().ToDelta();
  options_.contents = StringAppend(options_.contents, line.options_.contents);
  for (auto& m : line.options_.modifiers) {
    options_.modifiers[m.first + original_length] = m.second;
  }
  options_.end_of_line_modifiers = line.options_.end_of_line_modifiers;
  ValidateInvariants();
}

std::shared_ptr<vm::Environment> Line::environment() const {
  std::unique_lock<std::mutex> lock(mutex_);
  CHECK(environment_ != nullptr);
  return environment_;
}

OutputProducer::LineWithCursor Line::Output(
    const OutputOptions& options) const {
  static Tracker tracker(L"Line::Output");
  auto tracker_call = tracker.Call();

  std::unique_lock<std::mutex> lock(mutex_);

  CHECK(environment_ != nullptr);
  VLOG(5) << "Producing output of line: " << ToString();
  Line::Options line_output;
  ColumnNumber output_column;
  ColumnNumber input_column = options.initial_column;
  OutputProducer::LineWithCursor line_with_cursor;
  auto modifiers_it = options_.modifiers.lower_bound(input_column);
  if (!options_.modifiers.empty() &&
      modifiers_it != options_.modifiers.begin()) {
    line_output.modifiers[output_column] = std::prev(modifiers_it)->second;
  }

  for (; input_column < EndColumnWithLock() &&
         output_column < ColumnNumber(0) + options.width;
       ++input_column) {
    wint_t c = GetWithLock(input_column);
    CHECK(c != '\n');

    if (modifiers_it != options_.modifiers.end()) {
      CHECK_GE(modifiers_it->first, input_column);
      if (modifiers_it->first == input_column) {
        line_output.modifiers[output_column] = modifiers_it->second;
        ++modifiers_it;
      }
    }

    if (options.active_cursor_column.has_value() &&
        options.active_cursor_column.value() == input_column) {
      line_with_cursor.cursor = output_column;
      if (!options.modifiers_main_cursor.empty()) {
        line_output.modifiers[output_column + ColumnNumberDelta(1)] =
            line_output.modifiers.empty()
                ? LineModifierSet()
                : line_output.modifiers.rbegin()->second;
        line_output.modifiers[output_column].insert(
            options.modifiers_main_cursor.begin(),
            options.modifiers_main_cursor.end());
      }
    } else if (options.inactive_cursor_columns.find(input_column) !=
               options.inactive_cursor_columns.end()) {
      line_output.modifiers[output_column + ColumnNumberDelta(1)] =
          line_output.modifiers.empty()
              ? LineModifierSet()
              : line_output.modifiers.rbegin()->second;
      line_output.modifiers[output_column].insert(
          options.modifiers_inactive_cursors.begin(),
          options.modifiers_inactive_cursors.end());
    }

    switch (c) {
      case L'\r':
        break;

      case L'\t': {
        ColumnNumber target =
            ColumnNumber(0) +
            ((output_column.ToDelta() / 8) + ColumnNumberDelta(1)) * 8;
        line_output.AppendString(
            ColumnNumberDelta::PaddingString(target - output_column, L' '),
            std::nullopt);
        output_column = target;
        break;
      }

      default:
        VLOG(8) << "Print character: " << c;
        line_output.contents = StringAppend(std::move(line_output.contents),
                                            NewLazyString(wstring(1, c)));
        output_column += ColumnNumberDelta(wcwidth(c));
    }
  }
  line_output.end_of_line_modifiers =
      input_column == EndColumnWithLock()
          ? options_.end_of_line_modifiers
          : (line_output.modifiers.empty()
                 ? LineModifierSet()
                 : line_output.modifiers.rbegin()->second);
  line_with_cursor.line = std::make_shared<Line>(std::move(line_output));
  if (!line_with_cursor.cursor.has_value() &&
      options.active_cursor_column.has_value()) {
    line_with_cursor.cursor = output_column;
  }
  return line_with_cursor;
}

size_t Line::GetHash() const {
  std::unique_lock<std::mutex> lock(mutex_);
  if (hash_.has_value()) return hash_.value();
  size_t value = 0;
  for (auto& modifiers : options_.modifiers) {
    value = hash_combine(value, std::hash<ColumnNumber>{}(modifiers.first),
                         std::hash<LineModifierSet>{}(modifiers.second));
  }
  ForEachColumn(*options_.contents, [&](ColumnNumber, wchar_t c) {
    value = hash_combine(value, c);
  });
  hash_ = value;
  return value;
}

void Line::ValidateInvariants() const {
  CHECK(options_.contents != nullptr);
  for (auto& m : options_.modifiers) {
    CHECK_LE(m.first, EndColumnWithLock());
    CHECK(m.second.find(LineModifier::RESET) == m.second.end());
  }
}

ColumnNumber Line::EndColumnWithLock() const {
  return ColumnNumber(0) + options_.contents->size();
}

wint_t Line::GetWithLock(ColumnNumber column) const {
  CHECK_LT(column, EndColumnWithLock());
  return options_.contents->get(column);
}

}  // namespace editor
}  // namespace afc
