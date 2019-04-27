#include "src/cursors_highlighter.h"

#include <glog/logging.h>

#include <set>

#include "src/delegating_output_receiver_with_internal_modifiers.h"
#include "src/output_receiver.h"

namespace afc {
namespace editor {
namespace {
class CursorsHighlighter
    : public DelegatingOutputReceiverWithInternalModifiers {
 public:
  explicit CursorsHighlighter(CursorsHighlighterOptions options)
      : DelegatingOutputReceiverWithInternalModifiers(
            std::move(options.delegate),
            DelegatingOutputReceiverWithInternalModifiers::Preference::
                kInternal),
        options_(std::move(options)),
        next_cursor_(options_.columns.begin()) {
    CheckInvariants();
    UpdateColumnRead(0);
  }

  void AddCharacter(wchar_t c) override {
    CheckInvariants();
    switch (cursor_state_) {
      case CursorState::kNone:
        break;
      case CursorState::kActive:
        ++next_cursor_;
        AddInternalModifier(LineModifier::CYAN);
        break;
      case CursorState::kInactive:
        ++next_cursor_;
        AddInternalModifier(LineModifier::REVERSE);
        AddInternalModifier(options_.multiple_cursors ? LineModifier::CYAN
                                                      : LineModifier::BLUE);
        break;
    }

    DelegatingOutputReceiver::AddCharacter(c);
    if (cursor_state_ != CursorState::kNone) {
      AddInternalModifier(LineModifier::RESET);
    }
    UpdateColumnRead(1);
    CheckInvariants();
  }

  void AddString(const wstring& str) override {
    size_t str_pos = 0;
    while (str_pos < str.size()) {
      CheckInvariants();

      // Compute the position of the next cursor relative to the start of this
      // string.
      size_t next_column = (next_cursor_ == options_.columns.end())
                               ? str.size()
                               : *next_cursor_ + str_pos - column_read_;
      if (next_column > str_pos) {
        size_t len = next_column - str_pos;
        DelegatingOutputReceiver::AddString(str.substr(str_pos, len));
        column_read_ += len;
        str_pos += len;
      }

      CheckInvariants();

      if (str_pos < str.size()) {
        CHECK(next_cursor_ != options_.columns.end());
        CHECK_EQ(*next_cursor_, column_read_);
        AddCharacter(str[str_pos]);
        str_pos++;
      }
      CheckInvariants();
    }
  }

 private:
  enum class CursorState {
    kNone,
    kInactive,
    kActive,
  };

  void UpdateColumnRead(size_t delta) {
    column_read_ += delta;
    if (next_cursor_ == options_.columns.end() ||
        *next_cursor_ != column_read_) {
      cursor_state_ = CursorState::kNone;
    } else if (!options_.active_cursor_input.has_value() ||
               options_.active_cursor_input.value() != column_read_) {
      cursor_state_ = CursorState::kInactive;
    } else {
      cursor_state_ = CursorState::kActive;
      if (options_.active_cursor_output != nullptr) {
        *options_.active_cursor_output =
            DelegatingOutputReceiverWithInternalModifiers::column();
      }
    }
  }

  void CheckInvariants() {
    if (next_cursor_ != options_.columns.end()) {
      CHECK_GE(*next_cursor_, column_read_);
    }
  }

  const CursorsHighlighterOptions options_;

  // Points to the first element in columns_ that is greater than or equal to
  // the current position.
  std::set<size_t>::const_iterator next_cursor_;
  size_t column_read_ = 0;
  CursorState cursor_state_;
};
}  // namespace
std::unique_ptr<OutputReceiver> NewCursorsHighlighter(
    CursorsHighlighterOptions options) {
  return std::make_unique<CursorsHighlighter>(std::move(options));
}
}  // namespace editor
}  // namespace afc