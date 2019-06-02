#ifndef __AFC_EDITOR_SRC_TRACKERS_H__
#define __AFC_EDITOR_SRC_TRACKERS_H__

#include <functional>
#include <list>
#include <memory>
#include <string>

namespace afc {
namespace editor {

// Tracks number of times an operation happens, as well as total time spent
// executing it.
class Tracker {
 public:
  struct Data {
    std::wstring name;

    size_t executions = 0;
    double seconds = 0;
  };

  static std::list<Data> GetData();

  Tracker(std::wstring name);
  ~Tracker();

  std::unique_ptr<bool, std::function<void(bool*)>> Call();

 private:
  static std::list<Tracker*> trackers_;

  std::list<Tracker*>::iterator trackers_it_;

  Data data_;
};

}  // namespace editor
}  // namespace afc

#endif  // __AFC_EDITOR_SRC_TRACKERS_H__