#ifndef __AFC_EDITOR_LINE_H__
#define __AFC_EDITOR_LINE_H__

#include <glog/logging.h>

#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "src/lazy_string.h"
#include "src/line_column.h"
#include "src/output_producer.h"
#include "src/parse_tree.h"
#include "src/vm/public/environment.h"

namespace afc {
namespace editor {

class EditorMode;
class EditorState;
class LazyString;
class OpenBuffer;
class LineWithCursor;

using std::hash;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::unordered_set;
using std::vector;
using std::wstring;

// This class is thread-safe.
class Line {
 public:
  // TODO: Turn this into a class.
  struct Options {
    Options() : contents(EmptyString()) {}
    Options(shared_ptr<LazyString> input_contents)
        : contents(std::move(input_contents)), modifiers(contents->size()) {}
    Options(Line line);

    ColumnNumber EndColumn() const;

    void AppendCharacter(wchar_t c, LineModifierSet modifier);
    void AppendString(std::shared_ptr<LazyString> suffix);
    void AppendString(std::shared_ptr<LazyString> suffix,
                      LineModifierSet modifier);
    void AppendString(std::wstring contents, LineModifierSet modifier);
    void Append(Line line);

    std::shared_ptr<LazyString> contents;
    std::vector<LineModifierSet> modifiers;
    LineModifierSet end_of_line_modifiers;
    std::shared_ptr<vm::Environment> environment;

   private:
    void ValidateInvariants();
  };

  Line() : Line(Options()) {}
  explicit Line(const Options& options);
  explicit Line(wstring text);
  Line(const Line& line);

  shared_ptr<LazyString> contents() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return contents_;
  }
  // TODO: Change to return a ColumnNumberDelta.
  size_t size() const {
    CHECK(contents() != nullptr);
    return contents()->size();
  }
  ColumnNumber EndColumn() const;
  bool empty() const {
    CHECK(contents() != nullptr);
    return size() == 0;
  }
  wint_t get(size_t column) const {
    CHECK_LT(column, contents()->size());
    return contents()->get(column);
  }
  wint_t get(ColumnNumber column) const;
  shared_ptr<LazyString> Substring(ColumnNumber column,
                                   ColumnNumberDelta length) const;

  // Returns the substring from pos to the end of the string.
  shared_ptr<LazyString> Substring(ColumnNumber column) const;

  wstring ToString() const { return contents()->ToString(); }
  // Delete characters in [position, position + amount).
  void DeleteCharacters(ColumnNumber position, ColumnNumberDelta amount);

  // Delete characters from position until the end.
  void DeleteCharacters(ColumnNumber position);
  void InsertCharacterAtPosition(ColumnNumber position);

  // Sets the character at the position given.
  //
  // `position` may be greater than size(), in which case the character will
  // just get appended (extending the line by exactly one character).
  void SetCharacter(ColumnNumber position, int c,
                    const LineModifierSet& modifiers);

  void SetAllModifiers(const LineModifierSet& modifiers);
  const vector<LineModifierSet> modifiers() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return options_.modifiers;
  }
  vector<LineModifierSet>& modifiers() {
    std::unique_lock<std::mutex> lock(mutex_);
    return options_.modifiers;
  }
  const LineModifierSet& end_of_line_modifiers() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return options_.end_of_line_modifiers;
  }

  bool modified() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return modified_;
  }
  void set_modified(bool modified) {
    std::unique_lock<std::mutex> lock(mutex_);
    modified_ = modified;
  }

  void Append(const Line& line);

  std::shared_ptr<vm::Environment> environment() const;

  bool filtered() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return filtered_;
  }
  bool filter_version() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return filter_version_;
  }
  void set_filtered(bool filtered, size_t filter_version) {
    std::unique_lock<std::mutex> lock(mutex_);
    filtered_ = filtered;
    filter_version_ = filter_version;
  }

  struct OutputOptions {
    ColumnNumber initial_column;
    ColumnNumberDelta width;
    std::optional<ColumnNumber> active_cursor_column;
    std::set<ColumnNumber> inactive_cursor_columns;
    LineModifierSet modifiers_inactive_cursors;
  };
  OutputProducer::LineWithCursor Output(const OutputOptions& options) const;

 private:
  ColumnNumber EndColumnWithLock() const;
  wint_t GetWithLock(ColumnNumber column) const;

  mutable std::mutex mutex_;
  std::shared_ptr<vm::Environment> environment_;
  // TODO: Remove contents_ and modifiers_ and just use options_ instead.
  shared_ptr<LazyString> contents_;
  Options options_;
  bool modified_ = false;
  bool filtered_ = true;
  size_t filter_version_ = 0;
};

}  // namespace editor
}  // namespace afc

#endif  // __AFC_EDITOR_LINE_H__
