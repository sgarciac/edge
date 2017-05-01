#include "line_column.h"

namespace afc {
namespace editor {

std::ostream& operator<<(std::ostream& os, const LineColumn& lc) {
    os << "["
       << (lc.line == std::numeric_limits<size_t>::max()
               ? "inf" : std::to_string(lc.line))
       << ":"
       << (lc.column == std::numeric_limits<size_t>::max()
               ? "inf" : std::to_string(lc.column))
       << "]";
    return os;
}

bool LineColumn::operator!=(const LineColumn& other) const {
  return line != other.line || column != other.column;
}

std::ostream& operator<<(std::ostream& os, const Range& range) {
  os << "[" << range.begin << ", " << range.end << ")";
  return os;
}

std::wstring LineColumn::ToCppString() const {
  return L"LineColumn(" + std::to_wstring(line) + L", " +
      std::to_wstring(column) + L")";
}

} // namespace editor
} // namespace afc
