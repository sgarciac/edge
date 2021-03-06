#ifndef __AFC_EDITOR_EDITOR_VARIABLES_H__
#define __AFC_EDITOR_EDITOR_VARIABLES_H__

#include "src/variables.h"
#include "vm/public/value.h"

namespace afc::editor::editor_variables {

EdgeStruct<bool>* BoolStruct();
extern EdgeVariable<bool>* const multiple_buffers;

}  // namespace afc::editor::editor_variables

#endif  // __AFC_EDITOR_EDITOR_VARIABLES_H__
