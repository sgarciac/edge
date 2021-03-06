#include "../public/callbacks.h"

#include "../public/types.h"

namespace afc {
namespace vm {

const VMType VMTypeMapper<void>::vmtype = VMType(VMType::VM_VOID);
const VMType VMTypeMapper<bool>::vmtype = VMType(VMType::VM_BOOLEAN);
const VMType VMTypeMapper<int>::vmtype = VMType(VMType::VM_INTEGER);
const VMType VMTypeMapper<double>::vmtype = VMType(VMType::VM_DOUBLE);
const VMType VMTypeMapper<wstring>::vmtype = VMType(VMType::VM_STRING);

}  // namespace vm
}  // namespace afc
