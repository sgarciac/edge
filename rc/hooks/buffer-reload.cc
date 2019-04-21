// buffer-reload.cc: Prepare for a buffer being reload.
//
// This program mainly sets several buffer variables depending on properties
// of the buffer (such as the extension of the file being loaded).

#include "../editor_commands/cpp-mode"
#include "../editor_commands/java-mode"
#include "../editor_commands/lib/clang-format"
#include "../editor_commands/lib/paths"
#include "../editor_commands/lib/strings"

// Optimizes the buffer for visualizing a patch (output of a `diff` command).
void DiffMode() { buffer.set_tree_parser("diff"); }

void GoToBeginningOfLine() {
  buffer.set_position(LineColumn(buffer.position().line(), 0));
}

void GoToEndOfLine() {
  int current_line = buffer.position().line();
  buffer.set_position(
      LineColumn(buffer.position().line(), buffer.line(current_line).size()));
}

void DeleteCurrentLine() {
  buffer.PushTransformationStack();
  buffer.ApplyTransformation(TransformationGoToColumn(0));

  Modifiers modifiers = Modifiers();
  modifiers.set_line();
  modifiers.set_repetitions(repetitions());
  modifiers.set_boundary_end_neighbor();
  buffer.ApplyTransformation(TransformationDelete(modifiers));

  buffer.PopTransformationStack();
  set_repetitions(1);
}

void CenterScreenAroundCurrentLine() {
  int size = screen.lines();
  size--;  // The status line doesn't count.
  int line = buffer.position().line();
  int start_line = line - size / 2;
  if (start_line < 0) {
    SetStatus("Near beginning of file.");
    start_line = 0;
  } else if (start_line + size > buffer.line_count()) {
    SetStatus("Near end of file.");
    start_line = (buffer.line_count() > size ? buffer.line_count() - size : 0);
  }
  buffer.set_view_start_line(start_line);
}

buffer.set_editor_commands_path("~/.edge/editor_commands/");

void HandleFileTypes(string basename, string extension) {
  if (extension == "cc" || extension == "h" || extension == "c") {
    CppMode();
    buffer.AddBindingToFile("sh", buffer.editor_commands_path() + "header");
    buffer.AddBindingToFile("sI", buffer.editor_commands_path() + "include");
    buffer.AddBindingToFile("si", buffer.editor_commands_path() + "indent");
    SetStatus("🔡 C++ file (" + extension + ")");
    return;
  }

  if (extension == "sh") {
    buffer.set_paragraph_line_prefix_characters(" #");
    buffer.set_line_prefix_characters(" #");
    SetStatus("🔡 Shell script (" + extension + ")");
  }

  if (extension == "java") {
    JavaMode();
    buffer.AddBindingToFile("si", buffer.editor_commands_path() + "indent");
    buffer.AddBindingToFile("sR", buffer.editor_commands_path() + "reflow");
    SetStatus("🔡 Java file (" + extension + ")");
    return;
  }

  if (basename == "COMMIT_EDITMSG") {
    buffer.set_paragraph_line_prefix_characters(" #");
    buffer.set_line_prefix_characters(" #");
    buffer.AddBindingToFile("sR", buffer.editor_commands_path() + "reflow");
    SetStatus("🔡 Git commit message");
    return;
  }

  if (extension == "py") {
    buffer.set_paragraph_line_prefix_characters(" #");
    buffer.set_line_prefix_characters(" #");
    buffer.AddBindingToFile("si", buffer.editor_commands_path() + "indent");
    SetStatus("🔡 Python file (" + extension + ")");
    return;
  }

  if (extension == "md") {
    buffer.set_tree_parser("md");
    buffer.AddBindingToFile("si", buffer.editor_commands_path() + "indent");
    buffer.AddBindingToFile("sR", buffer.editor_commands_path() + "reflow");
    buffer.set_paragraph_line_prefix_characters("*-# ");
    buffer.set_line_prefix_characters(" ");
    SetStatus("🔡 Markdown file (" + extension + ")");
  }
}

string path = buffer.path();
if (path == "") {
  // If path is empty, this buffer is running a command.
  string command = buffer.command();
  if (command != "") {
    buffer.set_paste_mode(true);
  }

  command = SkipInitialSpaces(command);
  string base_command = BaseCommand(command);
  if (base_command != "") {
    if (base_command == "bash" || base_command == "python" ||
        base_command == "sh" || base_command == "gdb") {
      // These are interactive commands, that get a full pts.
      buffer.set_pts(true);
      buffer.set_follow_end_of_file(true);
      buffer.set_buffer_list_context_lines(5);
    } else if (base_command == "make") {
      buffer.set_contains_line_marks(true);
      buffer.set_reload_on_buffer_write(true);
      buffer.set_follow_end_of_file(true);
      buffer.set_buffer_list_context_lines(5);
    } else if (base_command == "grep") {
      buffer.set_contains_line_marks(true);
      buffer.set_allow_dirty_delete(true);
    } else if (base_command == "clang-format") {
      buffer.set_show_in_buffers_list(true);
      buffer.set_close_after_clean_exit(true);
      buffer.set_allow_dirty_delete(true);
    } else if (base_command == "diff") {
      DiffMode();
    } else if (base_command == "git") {
      string next = BaseCommand(SkipInitialSpaces(command.substr(
          base_command.size(), command.size() - base_command.size())));
      if (next == "diff") {
        DiffMode();
      }
    } else {
      buffer.set_follow_end_of_file(buffer.pts());
    }
    buffer.set_atomic_lines(false);
    buffer.set_reload_on_enter(false);
  }
} else {
  // If path is non-empty, this buffer is loading a file.
  int dot = path.find_last_of(".", path.size());
  string extension =
      dot == -1 ? "" : path.substr(dot + 1, path.size() - dot - 1);
  string basename = Basename(path);

  buffer.AddBinding("^", "Go to the beginning of the current line",
                    GoToBeginningOfLine);
  buffer.AddBinding("$", "Go to the end of the current line", GoToEndOfLine);
  buffer.AddBindingToFile("J",
                          buffer.editor_commands_path() + "fold-next-line");
  buffer.AddBinding("K", "Delete the current line", DeleteCurrentLine);
  buffer.AddBindingToFile("#", buffer.editor_commands_path() + "reflow");

  buffer.set_typos("overriden");

  HandleFileTypes(basename, extension);
}

if (!buffer.pts()) {
  buffer.AddBinding("M", "Center the screen around the current line.",
                    CenterScreenAroundCurrentLine);
}

void ZoomToLeaf() { editor.ZoomToLeaf(); }
void AddNewLeaf() { editor.AddVerticalPane(); }
void RewindActiveLeaf() { editor.AdvanceActiveLeaf(-1); }
void AdvanceActiveLeaf() { editor.AdvanceActiveLeaf(1); }
buffer.AddBinding("st=", "Frames: Zoom to the current leaf", ZoomToLeaf);
buffer.AddBinding("st+", "Frames: Add a new leaf", AddNewLeaf);
buffer.AddBinding("stk", "Frames: Move to the previous leaf", RewindActiveLeaf);
buffer.AddBinding("stj", "Frames: Move to the next leaf", AdvanceActiveLeaf);
