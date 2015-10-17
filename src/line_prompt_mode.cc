#include "line_prompt_mode.h"

#include <memory>
#include <limits>
#include <string>

#include "buffer.h"
#include "char_buffer.h"
#include "command.h"
#include "command_mode.h"
#include "file_link_mode.h"
#include "editable_string.h"
#include "editor.h"
#include "predictor.h"
#include "terminal.h"
#include "wstring.h"

namespace afc {
namespace editor {
namespace {

using std::make_pair;
using std::numeric_limits;

const wstring kPredictionsBufferName = L"- predictions";

map<wstring, shared_ptr<OpenBuffer>>::iterator
GetHistoryBuffer(EditorState* editor_state, const wstring& name) {
  OpenFileOptions options;
  options.editor_state = editor_state;
  options.name = L"- history: " + name;
  auto it = editor_state->buffers()->find(options.name);
  if (it != editor_state->buffers()->end()) {
    return it;
  }
  options.path =
      (*editor_state->edge_path().begin()) + L"/" + name + L"_history";
  options.make_current_buffer = false;
  it = OpenFile(options);
  assert(it != editor_state->buffers()->end());
  assert(it->second != nullptr);
  it->second->set_bool_variable(
      OpenBuffer::variable_save_on_close(), true);
  if (!editor_state->has_current_buffer()) {
    // Seems lame, but what can we do?
    editor_state->set_current_buffer(it);
    editor_state->ScheduleRedraw();
  }
  return it;
}

map<wstring, shared_ptr<OpenBuffer>>::iterator
GetPredictionsBuffer(
    EditorState* editor_state,
    Predictor predictor,
    wstring input,
    function<void(const wstring&)> consumer) {
  auto it = editor_state->buffers()
      ->insert(make_pair(kPredictionsBufferName, nullptr));
  it.first->second = PredictionsBuffer(
      editor_state, std::move(predictor), std::move(input),
      std::move(consumer));
  it.first->second->Reload(editor_state);
  it.first->second->set_current_position_line(0);
  it.first->second->set_current_position_col(0);
  return it.first;
}

class LinePromptMode : public EditorMode {
 public:
  LinePromptMode(PromptOptions options,
                 map<wstring, shared_ptr<OpenBuffer>>::iterator initial_buffer)
      : options_(std::move(options)),
        initial_buffer_(initial_buffer),
        input_(EditableString::New(options_.initial_value)),
        most_recent_char_(0) {}

  void ProcessInput(wint_t c, EditorState* editor_state) {
    editor_state->set_status_prompt(true);
    if (initial_buffer_ != editor_state->current_buffer()
        && initial_buffer_ != editor_state->buffers()->end()) {
      editor_state->set_current_buffer(initial_buffer_);
      editor_state->ScheduleRedraw();
    }
    int last_char = most_recent_char_;
    most_recent_char_ = c;
    switch (c) {
      case '\n':
        if (input_->size() != 0) {
          auto history =
              GetHistoryBuffer(editor_state, options_.history_file)->second;
          CHECK(history != nullptr);
          if (history->contents()->size() == 0
              || (history->contents()->at(history->contents()->size() - 1)
                      ->ToString()
                  != input_->ToString())) {
            history->AppendLine(editor_state, input_);
          }
        }
        editor_state->set_status_prompt(false);
        editor_state->ResetStatus();
        options_.handler(input_->ToString(), editor_state);
        return;

      case Terminal::ESCAPE:
        editor_state->set_status_prompt(false);
        editor_state->ResetStatus();
        options_.handler(L"", editor_state);
        return;

      case Terminal::BACKSPACE:
        input_->Backspace();
        break;

      case '\t':
        if (last_char == '\t') {
          auto it = editor_state->buffers()->find(kPredictionsBufferName);
          if (it == editor_state->buffers()->end()) {
            editor_state->SetStatus(L"Error: predictions buffer not found.");
            return;
          }
          it->second->set_current_position_line(0);
          editor_state->set_current_buffer(it);
          editor_state->ScheduleRedraw();
        } else {
          LOG(INFO) << "Triggering predictions from: " << input_->ToString();
          GetPredictionsBuffer(
              editor_state, options_.predictor, input_->ToString(),
              [editor_state, this](const wstring& prediction) {
                if (input_->ToString().size() >= prediction.size()) {
                  return;
                }
                LOG(INFO) << "Prediction advanced from " << input_->ToString()
                          << " to " << prediction;
                input_ = EditableString::New(prediction);
                UpdateStatus(editor_state);
                // We do this so that another \t will cause the predictors
                // buffer to be updated (since the input has changed).
                most_recent_char_ = '\n';
              });
        }
        break;

      case Terminal::CTRL_U:
        input_->Clear();
        break;

      case Terminal::UP_ARROW:
        {
          auto buffer =
              GetHistoryBuffer(editor_state, options_.history_file)->second;
          if (buffer == nullptr || buffer->contents()->empty()) { return; }
          LineColumn position = buffer->position();
          if (position.line > 0) {
            position.line --;
            buffer->set_position(position);
          }
          SetInputFromCurrentLine(buffer);
        }
        break;

      case Terminal::DOWN_ARROW:
        {
          auto buffer =
              GetHistoryBuffer(editor_state, options_.history_file)->second;
          if (buffer == nullptr || buffer->contents()->size() == 1) { return; }
          LineColumn position = buffer->position();
          if (position.line + 1 <= buffer->contents()->size()) {
            position.line ++;
            buffer->set_position(position);
          }
          SetInputFromCurrentLine(buffer);
        }
        break;

      default:
        input_->Insert(static_cast<char>(c));
    }
    UpdateStatus(editor_state);
  }

  void UpdateStatus(EditorState* editor_state) {
    editor_state->SetStatus(options_.prompt + input_->ToString());
  }

 private:
  void SetInputFromCurrentLine(const shared_ptr<OpenBuffer>& buffer) {
    if (buffer == nullptr || buffer->line() == buffer->end()) {
      input_ = EditableString::New(L"");
      return;
    }
    input_ = EditableString::New(buffer->current_line()->ToString());
  }

  const PromptOptions options_;
  const map<wstring, shared_ptr<OpenBuffer>>::iterator initial_buffer_;

  shared_ptr<EditableString> input_;
  int most_recent_char_;
};

class LinePromptCommand : public Command {
 public:
  LinePromptCommand(wstring description, PromptOptions options)
      : description_(std::move(description)),
        options_(std::move(options)) {}

  const wstring Description() {
    return description_;
  }

  void ProcessInput(wint_t, EditorState* editor_state) {
    Prompt(editor_state, options_);
  }

 private:
  const wstring description_;
  PromptOptions options_;
};

}  // namespace

using std::unique_ptr;
using std::shared_ptr;

void Prompt(EditorState* editor_state, PromptOptions options) {
  auto history = GetHistoryBuffer(editor_state, options.history_file);
  std::unique_ptr<LinePromptMode> line_prompt_mode(
      new LinePromptMode(std::move(options), editor_state->current_buffer()));
  history->second->set_current_position_line(
      history->second->contents()->size());
  line_prompt_mode->UpdateStatus(editor_state);
  editor_state->set_mode(std::move(line_prompt_mode));
  editor_state->set_status_prompt(true);
}

unique_ptr<Command> NewLinePromptCommand(wstring description,
                                         PromptOptions options) {
  return std::move(unique_ptr<Command>(new LinePromptCommand(
      std::move(description), std::move(options))));
}

}  // namespace afc
}  // namespace editor
