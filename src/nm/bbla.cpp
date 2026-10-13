// nm/bbla.d -> src/nm/bbla.cpp
//
// bbla.d is a template module: class Matrix(T) plus template functions, with no
// non-template definitions to move out of the header. This TU exists because
// CMakeLists lists it, and it verifies the header compiles on its own. It also
// pins the instantiation the trunk build needs (nm/number.d's `number`), which is
// what nm/bbla.d's own unittest blocks instantiate.

#include "nm/bbla.hpp"

namespace eil::nm::bbla {

// Forcing an instantiation of the Matrix member set the way the unittests use it,
// so a compile error in any member shows up in this TU rather than only at a
// distant call site.
template class Matrix<eil::nm::number>;

} // namespace eil::nm::bbla
