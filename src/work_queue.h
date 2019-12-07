#ifndef __AFC_EDITOR_WORK_QUEUE_H__
#define __AFC_EDITOR_WORK_QUEUE_H__

#include <glog/logging.h>

#include <condition_variable>
#include <functional>
#include <mutex>
#include <vector>

namespace afc::editor {
// Contains a list of callbacks that will be executed later, at some point
// shortly before the Editor attempts to sleep waiting for IO (in the main
// loop). If this isn't empty, the main loop will actually skip the sleep and
// continue running.
//
// One of the uses of this is for long running operations that can't be executed
// in background threads. They periodically interrupt themselves and insert
// their continuations here. Edge flushes this to advance their work. This
// allows them to run without preventing Edge from handling input from the user.
//
// Another use is to ensure that a given execution (such as updating the syntax
// tree) only happens in "batches", after a set of operations has been applied
// to the buffer (rather than having to schedule many redundant runs, e.g., when
// input is being gradually read from a file).
class WorkQueue {
 public:
  void Schedule(std::function<void()> callback);

  enum class State { kIdle, kScheduled };
  void Execute();

  State state() const;

 private:
  mutable std::mutex mutex_;
  std::vector<std::function<void()>> callbacks_;
};
}  // namespace afc::editor
#endif  // __AFC_EDITOR_WORK_QUEUE_H__