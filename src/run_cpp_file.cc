#include "src/run_cpp_file.h"

#include <memory>

#include "src/buffer.h"
#include "src/buffer_variables.h"
#include "src/command.h"
#include "src/editor.h"
#include "src/file_link_mode.h"
#include "src/line_prompt_mode.h"

namespace afc {
namespace editor {

namespace {
class RunCppFileCommand : public Command {
 public:
  wstring Description() const override { return L"runs a command from a file"; }
  wstring Category() const override { return L"Extensions"; }

  void ProcessInput(wint_t, EditorState* editor_state) override {
    if (!editor_state->has_current_buffer()) {
      return;
    }
    auto buffer = editor_state->current_buffer();
    PromptOptions options;
    options.editor_state = editor_state;
    options.prompt = L"cmd ";
    options.history_file = L"editor_commands";
    options.initial_value =
        buffer->Read(buffer_variables::editor_commands_path);
    options.handler = RunCppFileHandler;
    options.cancel_handler = [](EditorState*) { /* Nothing. */ };
    options.predictor = FilePredictor;
    Prompt(std::move(options));
  }
};
}  // namespace

futures::Value<EmptyValue> RunCppFileHandler(const wstring& input,
                                             EditorState* editor_state) {
  // TODO(easy): Honor `multiple_buffers`.
  auto buffer = editor_state->current_buffer();
  if (buffer == nullptr) {
    return futures::Past(EmptyValue());
  }
  if (editor_state->structure() == StructureLine()) {
    auto target = buffer->GetBufferFromCurrentLine();
    if (target != nullptr) {
      buffer = target;
    }
    editor_state->ResetModifiers();
  }

  buffer->ResetMode();
  auto options = ResolvePathOptions::New(editor_state);
  options.path = input;
  auto resolved_path = ResolvePath(std::move(options));
  if (!resolved_path.has_value()) {
    buffer->status()->SetWarningText(L"🗱  File not found: " + input);
    return futures::Past(EmptyValue());
  }

  using futures::IterationControlCommand;
  auto index = std::make_shared<size_t>(0);
  return futures::Transform(
      futures::While([buffer, total = editor_state->repetitions(),
                      adjusted_input = resolved_path->path, index]() {
        if (*index >= total)
          return futures::Past(IterationControlCommand::kStop);
        auto evaluation = buffer->EvaluateFile(adjusted_input);
        if (!evaluation.has_value())
          return futures::Past(IterationControlCommand::kStop);
        ++*index;
        return futures::Transform(
            evaluation.value(), [](const std::unique_ptr<Value>&) {
              return futures::Past(IterationControlCommand::kContinue);
            });
      }),
      [editor_state](IterationControlCommand) {
        editor_state->ResetRepetitions();
        return futures::Past(EmptyValue());
      });
}

std::unique_ptr<Command> NewRunCppFileCommand() {
  return std::make_unique<RunCppFileCommand>();
}

}  // namespace editor
}  // namespace afc
