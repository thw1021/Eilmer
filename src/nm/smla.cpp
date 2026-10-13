// nm/smla.d -> src/nm/smla.cpp
//
// smla.d is a template module (SMatrix(T), GMRESWorkSpace(T) and the free
// templates), so the definitions live in smla.hpp. The one non-template item is
// the module-level constant, defined here.

#include "nm/smla.hpp"

namespace eil::nm {
namespace smla {

// smla.d:27 `immutable double ESSENTIALLY_ZERO = 1.0e-50;`
const double ESSENTIALLY_ZERO = 1.0e-50;

// Pin the instantiation the trunk build uses (nm/number.d's number), so a
// compile error in any SMatrix member shows up here rather than at a distant
// call site.
template class SMatrix<eil::nm::number>;

} // namespace smla
} // namespace eil::nm
