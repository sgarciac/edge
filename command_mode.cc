#include <memory>
#include <map>
#include <string>

#include "advanced_mode.h"
#include "command.h"
#include "command_mode.h"
#include "find_mode.h"
#include "help_command.h"
#include "map_mode.h"

namespace afc {
namespace editor {

using std::make_pair;
using std::map;
using std::unique_ptr;

class Quit : public Command {
 public:
  const string Description() {
    return "quits";
  }

  void ProcessInput(int c, EditorState* editor_state) {
    editor_state->terminate = true;
  }
};

class LineUp : public Command {
 public:
  const string Description() {
    return "moves up one line";
  }

  void ProcessInput(int c, EditorState* editor_state) {
    shared_ptr<OpenBuffer> buffer = editor_state->get_current_buffer();
    if (buffer->current_position_line > 0) {
      buffer->current_position_line--;
    }
  }
};

class LineDown : public Command {
 public:
  const string Description() {
    return "moves down one line";
  }

  void ProcessInput(int c, EditorState* editor_state) {
    shared_ptr<OpenBuffer> buffer = editor_state->get_current_buffer();
    if (buffer->current_position_line < buffer->contents.size() - 1) {
      buffer->current_position_line++;
    }
  }
};

class MoveForwards : public Command {
 public:
  const string Description() {
    return "moves forwards";
  }

  void ProcessInput(int c, EditorState* editor_state) {
    shared_ptr<OpenBuffer> buffer = editor_state->get_current_buffer();
    if (buffer->current_position_col < buffer->current_line()->size()) {
      buffer->current_position_col++;
    } else if (buffer->current_position_col > buffer->current_line()->size()) {
      buffer->current_position_col = buffer->current_line()->size();
    }
  }
};

class MoveBackwards : public Command {
 public:
  const string Description() {
    return "moves backwards";
  }

  void ProcessInput(int c, EditorState* editor_state) {
    shared_ptr<OpenBuffer> buffer = editor_state->get_current_buffer();
    if (buffer->current_position_col > buffer->current_line()->size()) {
      buffer->current_position_col = buffer->current_line()->size();
    }
    if (buffer->current_position_col > 0) {
      buffer->current_position_col--;
    }
  }
};

class EnterAdvancedMode : public Command {
 public:
  const string Description() {
    return "enters advanced-command mode (press 'a?' for more)";
  }

  void ProcessInput(int c, EditorState* editor_state) {
    editor_state->mode = std::move(NewAdvancedMode());
  }
};

class EnterFindMode : public Command {
 public:
  const string Description() {
    return "finds occurrences of a character";
  }

  void ProcessInput(int c, EditorState* editor_state) {
    editor_state->mode = std::move(NewFindMode());
  }
};

class ActivateLink : public Command {
 public:
  const string Description() {
    return "activates the current link (if any)";
  }

  void ProcessInput(int c, EditorState* editor_state) {
    shared_ptr<OpenBuffer> buffer = editor_state->get_current_buffer();
    if (buffer->current_line()->activate.get() != nullptr) {
      buffer->current_line()->activate->ProcessInput(c, editor_state);
    }
  }
};

static const map<int, Command*>& GetCommandModeMap() {
  static map<int, Command*> output;
  if (output.empty()) {
    output.insert(make_pair('q', new Quit()));

    output.insert(make_pair('a', new EnterAdvancedMode()));
    output.insert(make_pair('f', new EnterFindMode()));

    output.insert(make_pair('\n', new ActivateLink()));

    output.insert(make_pair('j', new LineDown()));
    output.insert(make_pair('k', new LineUp()));
    output.insert(make_pair('l', new MoveForwards()));
    output.insert(make_pair('h', new MoveBackwards()));

    output.insert(make_pair('?', NewHelpCommand(output, "command mode").release()));
  }
  return output;
}

unique_ptr<EditorMode> NewCommandMode() {
  unique_ptr<MapMode> mode(new MapMode(GetCommandModeMap()));
  return std::move(mode);
}

}  // namespace afc
}  // namespace editor
