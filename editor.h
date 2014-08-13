#ifndef __AFC_EDITOR_EDITOR_H__
#define __AFC_EDITOR_EDITOR_H__

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "buffer.h"
#include "command_mode.h"
#include "lazy_string.h"
#include "memory_mapped_file.h"

namespace afc {
namespace editor {

using std::shared_ptr;
using std::unique_ptr;
using std::vector;
using std::map;
using std::max;
using std::min;

struct EditorState {
  EditorState()
      : current_buffer(buffers.end()),
        terminate(false),
        repetitions(1),
        structure(0),
        mode(std::move(NewCommandMode())) {}

  void CheckPosition() {
    get_current_buffer()->CheckPosition();
  }

  shared_ptr<OpenBuffer> get_current_buffer() const {
    return current_buffer->second;
  }

  void MoveBufferForwards(size_t times);
  void MoveBufferBackwards(size_t times);

  map<string, shared_ptr<OpenBuffer>> buffers;
  map<string, shared_ptr<OpenBuffer>>::iterator current_buffer;
  bool terminate;

  size_t repetitions;
  int structure;
  unique_ptr<EditorMode> mode;

  // Set by the terminal handler.
  size_t visible_lines;

  bool screen_needs_redraw;

  string status;
};

}  // namespace editor
}  // namespace afc

#endif
