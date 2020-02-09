#ifndef __AFC_EDITOR_FILE_SYSTEM_DRIVER_H__
#define __AFC_EDITOR_FILE_SYSTEM_DRIVER_H__

extern "C" {
#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
}

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "src/async_processor.h"
#include "src/futures/futures.h"
#include "src/work_queue.h"

namespace afc::editor {

// Class used to interact with the file system. All operations are performed
// asynchronously in a background thread; once their results are available, the
// corresponding future is notified through `work_queue` (to switch back to the
// main thread).
class FileSystemDriver {
 public:
  FileSystemDriver(WorkQueue* work_queue);

  futures::Value<int> Open(std::wstring path, int mode);
  futures::Value<std::optional<struct stat>> Stat(std::wstring path);

 private:
  AsyncEvaluator evaluator_;
};

}  // namespace afc::editor

#endif  // __AFC_EDITOR_FILE_SYSTEM_DRIVER_H__