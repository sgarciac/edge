#include "terminal.h"

#include <cassert>
#include <iostream>

extern "C" {
#include <curses.h>
#include <term.h>
}

namespace afc {
namespace editor {

using std::cerr;

constexpr int Terminal::DOWN_ARROW;
constexpr int Terminal::UP_ARROW;
constexpr int Terminal::LEFT_ARROW;
constexpr int Terminal::RIGHT_ARROW;
constexpr int Terminal::BACKSPACE;

Terminal::Terminal() {
  initscr();
  noecho();
  keypad(stdscr, false);
  SetStatus("Initializing...");
}

Terminal::~Terminal() {
  endwin();
}

void Terminal::Display(EditorState* editor_state) {
  if (editor_state->current_buffer == editor_state->buffers.end()) {
    if (editor_state->screen_needs_redraw) {
      editor_state->screen_needs_redraw = false;
      clear();
      ShowStatus(editor_state->status);
      refresh();
    }
    return;
  }
  auto const& buffer = editor_state->get_current_buffer();
  if (buffer->view_start_line() > buffer->current_position_line()) {
    buffer->set_view_start_line(buffer->current_position_line());
    editor_state->screen_needs_redraw = true;
  } else if (buffer->view_start_line() + LINES - 1 <= buffer->current_position_line()) {
    buffer->set_view_start_line(buffer->current_position_line() - LINES + 2);
    editor_state->screen_needs_redraw = true;
  }

  if (editor_state->screen_needs_redraw) {
    ShowBuffer(editor_state);
    editor_state->screen_needs_redraw = false;
  }
  ShowStatus(editor_state->status);
  AdjustPosition(buffer);
  refresh();
}

void Terminal::ShowStatus(const string& status) {
  if (status.empty()) {
    return;
  }
  move(LINES - 1, 0);
  if (status.size() < static_cast<size_t>(COLS)) {
    addstr(status.c_str());
    for (int i = status.size(); i < COLS; i++) {
      addch(' ');
    }
  } else {
    addstr(status.substr(0, COLS).c_str());
  }
}

void Terminal::ShowBuffer(const EditorState* editor_state) {
  const shared_ptr<OpenBuffer> buffer = editor_state->get_current_buffer();
  const vector<shared_ptr<Line>>& contents(*buffer->contents());

  clear();

  size_t last_line_to_show =
      buffer->view_start_line() + static_cast<size_t>(LINES)
      - (editor_state->status.empty() ? 1 : 2);
  if (last_line_to_show >= contents.size()) {
    last_line_to_show = contents.size() - 1;
  }
  for (size_t current_line = buffer->view_start_line();
       current_line <= last_line_to_show; current_line++) {
    const shared_ptr<LazyString> line(contents[current_line]->contents);
    size_t size = std::min(static_cast<size_t>(COLS), line->size());
    for (size_t pos = 0; pos < size; pos++) {
      int c = line->get(pos);
      assert(c != '\n');
      addch(c);
    }
    if (size < static_cast<size_t>(COLS)) {
      addch('\n');
    }
  }
}

void Terminal::AdjustPosition(const shared_ptr<OpenBuffer> buffer) {
  const vector<shared_ptr<Line>>& contents(*buffer->contents());
  size_t pos_x = buffer->current_position_col();
  if (pos_x > contents[buffer->current_position_line()]->contents->size()) {
    pos_x = contents[buffer->current_position_line()]->contents->size();
  }

  move(buffer->current_position_line() - buffer->view_start_line(), pos_x);
}

int Terminal::Read() {
  int c = getch();
  //cerr << "Read: " << c << "\n";
  switch (c) {
    case 127:
      return BACKSPACE;
    case 27:
      {
        nodelay(stdscr, true);
        int next = getch();
        //cerr << "Read next: " << next << "\n";
        nodelay(stdscr, false);
        switch (next) {
          case -1:
            return -1;

          case 91:
            {
              int next2 = getch();
              //cerr << "Read next2: " << next2 << "\n";
              switch (next2) {
                case 65:
                  return UP_ARROW;
                case 66:
                  return DOWN_ARROW;
                case 67:
                  return RIGHT_ARROW;
                case 68:
                  return LEFT_ARROW;
              }
            }
            return -1;
        }
        //cerr << "Unget: " << next << "\n";
        ungetch(next);
      }
      return -1;
    default:
      return c;
  }
}

void Terminal::SetStatus(const std::string& status) {
  status_ = status;

  size_t height = LINES;
  size_t width = COLS;
  move(height - 1, 0);
  std::string output_status =
      status_.length() > width
      ? status_.substr(0, width)
      : (status_ + std::string(width - status_.length(), ' '));
  addstr(output_status.c_str());
  move(0, 0);
  refresh();
}

}  // namespace afc
}  // namespace editor
