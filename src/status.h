#ifndef __AFC_EDITOR_STATUS_H__
#define __AFC_EDITOR_STATUS_H__

#include <memory>

#include "src/audio.h"
#include "src/line_column.h"

namespace afc {
namespace editor {

class OpenBuffer;

enum class OverflowBehavior { kModulo, kMaximum };
std::wstring ProgressString(size_t counter, OverflowBehavior overflow_behavior);
std::wstring ProgressStringFillUp(size_t counter,
                                  OverflowBehavior overflow_behavior);

// Opaque type returned by `SetExpiringInformationText`.
struct StatusExpirationControl;

class Status {
 public:
  Status(std::shared_ptr<OpenBuffer> console, AudioPlayer* audio_player);
  void CopyFrom(const Status& status);

  enum class Type { kWarning, kInformation, kPrompt };
  Type GetType() const;

  LineNumberDelta DesiredLines() const;

  void set_prompt(std::wstring text, std::shared_ptr<OpenBuffer> buffer);
  // Should only be called if `GetType` returns kPrompt.
  void set_prompt_context(std::shared_ptr<OpenBuffer> prompt_context);
  // May be nullptr.
  const std::shared_ptr<OpenBuffer>& prompt_buffer() const;
  const std::shared_ptr<OpenBuffer>& prompt_context() const;

  void SetInformationText(std::wstring text);
  std::unique_ptr<StatusExpirationControl,
                  std::function<void(StatusExpirationControl*)>>
  SetExpiringInformationText(std::wstring text);
  void SetWarningText(std::wstring text);
  void Reset();

  void Bell();

  const std::wstring& text() const;

 private:
  friend StatusExpirationControl;
  void ValidatePreconditions() const;

  const std::shared_ptr<OpenBuffer> console_;
  AudioPlayer* const audio_player_;

  // We nest our mutable fields in `struct Data`. This allows us to implement
  // `SetExpiringInformationText`, where we can detect if the status hasn't
  // changed (between the call to `SetExpiringInformationText` and the moment
  // when the `StatusExpirationControl` that it returns is deleted).
  struct Data {
    Type type = Type::kInformation;
    std::wstring text;
    std::shared_ptr<OpenBuffer> prompt_buffer = nullptr;

    // When `prompt_buffer` isn't nullptr, `prompt_context` may be set to a
    // buffer that contains either a preview of the results of executing the
    // prompt or possible completions.
    std::shared_ptr<OpenBuffer> prompt_context = nullptr;
  };

  std::shared_ptr<Data> data_ = std::make_shared<Data>();
};

}  // namespace editor
}  // namespace afc

#endif  // __AFC_EDITOR_STATUS_H__
