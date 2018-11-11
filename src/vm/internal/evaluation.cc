#include "evaluation.h"

#include <cassert>

#include "../public/value.h"
#include "../public/vm.h"

namespace afc {
namespace vm {

namespace {

void Crash(Trampoline*) { assert(false); }

}

}  // namespace vm
}  // namespace afc
