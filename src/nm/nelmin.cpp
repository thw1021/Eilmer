// nm/nelmin.d -> src/nm/nelmin.cpp
//
// nelmin.d is a template module: `minimize` and `NMMinimizer` are templated on the
// objective function and the scalar type, and `Vertex`/`less` are templates too, so
// the definitions live in nelmin.hpp. This TU exists because the CMake fragment
// lists it, and it verifies the header compiles on its own. It also forces the
// instantiation the trunk build needs -- a plain `double f(Slice<double>)` callable
// over nm/number.d's `number`, which is what nelmin.d's own unittest blocks and
// geom/grid/sgrid.d:1411 use -- so a compile error in any member shows up here
// rather than at a distant call site.

#include "nm/nelmin.hpp"

#include "nm/number.hpp"

namespace eil::nm::nelmin {

using TrunkObjective = double (*)(eil::Slice<eil::nm::number>);

template struct Vertex<eil::nm::number>;
template class NMMinimizer<TrunkObjective, eil::nm::number>;

} // namespace eil::nm::nelmin
