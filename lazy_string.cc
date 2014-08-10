#include "lazy_string.h"

#include <cassert>

namespace {

using namespace afc::editor;

class EmptyStringImpl : public LazyString {
 public:
  char get(size_t pos) const { assert(false); }
  size_t size() const { return 0; }
};

}  // namespace

namespace afc {
namespace editor {

shared_ptr<LazyString> EmptyString() {
  return shared_ptr<LazyString>(new EmptyStringImpl());
}

}  // namespace editor
}  // namespace afc
