#ifndef __AFC_EDITOR_MEMORY_MAPPED_FILE_H__
#define __AFC_EDITOR_MEMORY_MAPPED_FILE_H__

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
}

#include <functional>
#include <string>

#include "lazy_string.h"

namespace afc {
namespace editor {

using std::string;
using std::function;

class MemoryMappedFile : public LazyString {
 public:
  MemoryMappedFile(const string& path);
  ~MemoryMappedFile();

  char get(size_t pos) const {
    assert(buffer_ != nullptr);
    assert(pos < size());
    return buffer_[pos];
  }
  size_t size() const {
    return static_cast<size_t>(stat_buffer_.st_size);
  }

 private:
  const string path_;
  int fd_;
  struct stat stat_buffer_;
  char* buffer_;
};

class EditorState;
class OpenBuffer;

void LoadMemoryMappedFile(
    EditorState* editor_state, const string& path, OpenBuffer* buffer);

}  // namespace editor
}  // namespace afc

#endif