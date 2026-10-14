// geom/grid/paver2d.d -> src/geom/grid/paver2d.cpp (module geom.grid.paver2d)
//
// The only non-template part of the module: the rendering of D's compiler-generated
// enum member name for `enum NodeType` (paver2d.d:12). fill_interior itself is a
// template in paver2d.hpp (see the header's MEASURED DEPENDENCY note for why), so
// this translation unit has no dependency on geom/grid/usgrid.d and always compiles.

#include "geom/grid/paver2d.hpp"

#include <string>

#include "eil/format.hpp"

namespace eil {
namespace geom {

std::string nodeTypeName(NodeType nt) {
    // D renders an enum member by name; measured against ldc2 1.40.0 (corpus rows
    // nt.writerside / nt.tostring * / nt.sfmt / nt.oor7 / nt.oorminus1 / nt.oormax).
    switch (nt) {
    case NodeType::side: return std::string("side");
    case NodeType::rowend: return std::string("rowend");
    case NodeType::corner: return std::string("corner");
    case NodeType::reversal: return std::string("reversal");
    }
    // Out of range: ldc2 prints the string form of the cast expression, e.g.
    // "cast(NodeType)7" and "cast(NodeType)-1" — not the bare number and not "nan".
    return std::string("cast(NodeType)") + eil::to_string(static_cast<int>(nt));
}

} // namespace geom
} // namespace eil
