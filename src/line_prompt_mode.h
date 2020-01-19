#ifndef __AFC_EDITOR_LINE_PROMPT_MODE_H__
#define __AFC_EDITOR_LINE_PROMPT_MODE_H__

#include <memory>

#include "command.h"
#include "editor.h"
#include "predictor.h"

namespace afc {
namespace editor {

using std::unique_ptr;

struct PromptOptions {
  EditorState* editor_state = nullptr;

  // Text to show in the prompt.
  wstring prompt;

  std::wstring prompt_contents_type;

  // Optional. Name of the file with the history for this type of prompt.
  // Defaults to no history.
  wstring history_file;

  // Optional. Initial value for the prompt. Defaults to empty.
  wstring initial_value;

  // Run any time the text in the prompt changes.
  //
  // The prompt buffer is passed as an argument. To get the current text in the
  // prompt, use:
  //
  // auto line = buffer->LineAt(LineNumber(0));
  //
  // TODO(easy): Turn this into an options structure.
  std::function<void(const std::shared_ptr<OpenBuffer>&)> change_handler =
      [](const std::shared_ptr<OpenBuffer>&) {};

  // Function to run when the prompt receives the final input.
  std::function<void(const wstring& input, EditorState* editor)> handler;

  // Optional. Function to run when the prompt is cancelled (because ESCAPE was
  // pressed). If empty, handler will be run with an empty input.
  std::function<void(EditorState* editor)> cancel_handler;

  // Optional. Useful for automatic completion.
  Predictor predictor = EmptyPredictor;

  // Source buffer to give to the predictor. See
  // `PredictorInput::source_buffer`.
  std::shared_ptr<OpenBuffer> source_buffer;

  enum class Status { kEditor, kBuffer };
  Status status = Status::kEditor;
};

// TODO: Move editor_state to PromptOptions.
void Prompt(PromptOptions options);

unique_ptr<Command> NewLinePromptCommand(
    wstring description, std::function<PromptOptions(EditorState*)> options);

}  // namespace editor
}  // namespace afc

#endif
