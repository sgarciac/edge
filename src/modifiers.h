#ifndef __AFC_EDITOR_MODIFIERS_H__
#define __AFC_EDITOR_MODIFIERS_H__

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "src/direction.h"
#include "src/line_column.h"
#include "src/structure.h"
#include "src/vm/public/environment.h"

namespace afc {
namespace editor {

using std::ostream;

struct BufferPosition {
  // The name of the buffer.
  std::wstring buffer_name;
  // The position within the buffer.
  LineColumn position;
};

std::ostream& operator<<(std::ostream& os, const BufferPosition& bp);

struct Modifiers {
  static void Register(vm::Environment* environment);

  std::wstring Serialize() const;

  enum class Strength {
    kNormal,
    kStrong,
  };

  // Specifies what happens to characters near the cursor when a modification
  // is applied.
  enum class ModifyMode {
    // Default.  Characters move. In an insertion, they just move to the right,
    // to make space (in the file) for the newly inserted contents. In a
    // deletion, they get "consumed" (destroyed).
    kShift,
    // Characters never move. Characters at the right of an insertion will get
    // overwritten. For a deletion, characters just get blanked (set to space),
    // but not actually deleted.
    kOverwrite
  };

  // Sets the modifiers to their default values, including resetting any form
  // of stickyness.
  void ResetHard() {
    structure = StructureChar();
    default_direction = Direction::kForwards;
    default_insertion = ModifyMode::kShift;
    ResetSoft();
  }

  // After executing a command, sets modifiers to their default values, but,
  // unline ResetHard, abides by stickyness.
  void ResetSoft() {
    ResetStructure();
    ResetDirection();
    strength = Strength::kNormal;
    ResetInsertion();
    ResetRepetitions();
  }

  void ResetStructure() {
    if (!sticky_structure) {
      structure = StructureChar();
    }
  }

  void ResetDirection() { direction = default_direction; }

  void ResetInsertion() { insertion = default_insertion; }

  void ResetRepetitions() { repetitions = std::nullopt; }

  // Fields follow.
  Structure* structure = StructureChar();
  bool sticky_structure = false;

  Strength strength = Strength::kNormal;

  Direction direction = Direction::kForwards;
  Direction default_direction = Direction::kForwards;

  ModifyMode insertion = ModifyMode::kShift;
  ModifyMode default_insertion = ModifyMode::kShift;

  std::optional<size_t> repetitions = std::nullopt;

  enum class DeleteBehavior { kDeleteText, kDoNothing };
  DeleteBehavior delete_behavior = DeleteBehavior::kDeleteText;

  enum class PasteBufferBehavior { kDeleteInto, kDoNothing };
  PasteBufferBehavior paste_buffer_behavior = PasteBufferBehavior::kDeleteInto;

  enum Boundary {
    // At the current cursor position.
    CURRENT_POSITION,

    // Strictly at the start/end of the current region.
    LIMIT_CURRENT,

    // At the start/end of the next region.
    LIMIT_NEIGHBOR,
  };

  Boundary boundary_begin = CURRENT_POSITION;
  Boundary boundary_end = LIMIT_CURRENT;

  enum class CursorsAffected {
    // The transformation only affects the current cursor.
    kOnlyCurrent,

    // The transformation affects all cursors.
    kAll,
  };

  static constexpr CursorsAffected kDefaultCursorsAffected =
      CursorsAffected::kOnlyCurrent;
  std::optional<CursorsAffected> cursors_affected = std::nullopt;

  // The currently active cursors.
  std::wstring active_cursors;
};

Modifiers::Boundary IncrementBoundary(Modifiers::Boundary boundary);

ostream& operator<<(ostream& os, const Modifiers& m);

}  // namespace editor
namespace vm {
template <>
struct VMTypeMapper<std::shared_ptr<editor::Modifiers>> {
  static std::shared_ptr<editor::Modifiers> get(Value* value);
  static Value::Ptr New(std::shared_ptr<editor::Modifiers> value);
  static const VMType vmtype;
};
}  // namespace vm
}  // namespace afc

#endif  // __AFC_EDITOR_MODIFIERS_H__
