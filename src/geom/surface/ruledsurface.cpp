// geom/surface/ruledsurface.d -> src/geom/surface/ruledsurface.cpp
// (module geom.surface.ruledsurface).
//
// Definitions in D source order: the validating constructor (D:21-45), the copy
// constructor (D:47-57), dup (D:59-62), opCall (D:64-81), toString (D:83-87). The
// evaluation order of the blend in opCall is D's: (1.0-r) is formed before it
// multiplies edge0(s), r multiplies edge1(s), and the two Vector3 products are added
// left to right — no factoring into edge0 + r*(edge1-edge0), which would round
// differently.
//
// See the header for the non-1:1 notes (string switch -> if/else chain, the two
// Exception guards, the unvalidated copy constructor, the p.z clear after the blend).
#include "geom/surface/ruledsurface.hpp"

#include <memory>
#include <string>

#include "eil/error.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"
#include "geom/surface/parametricsurface.hpp"

namespace eil {
namespace geom {

// D:21-45.
RuledSurface::RuledSurface(const PathPtr& edge0, const PathPtr& edge1,
                           const std::string& ruled_direction, bool pure2D) {
    this->edge0 = edge0->dup(); // D:23
    this->edge1 = edge1->dup(); // D:24
    this->ruled_direction = ruled_direction;
    this->pure2D = pure2D;
    if (ruled_direction == "r" || ruled_direction == "i") {
        // D:28-34: edge0 is the west boundary, edge1 the east one.
        p00 = (*this->edge0)(0.0);
        p01 = (*this->edge0)(1.0);
        p10 = (*this->edge1)(0.0);
        p11 = (*this->edge1)(1.0);
    } else if (ruled_direction == "s" || ruled_direction == "j") {
        // D:35-41: edge0 is the south boundary, edge1 the north one.
        p00 = (*this->edge0)(0.0);
        p10 = (*this->edge0)(1.0);
        p01 = (*this->edge1)(0.0);
        p11 = (*this->edge1)(1.0);
    } else {
        // D:42-43.
        throw eil::ExceptionBase("Invalid string for ruled_direction: " + ruled_direction,
                                 __FILE__, __LINE__);
    }
} // end RuledSurface()

// D:47-57. As in D this re-dups the edges but runs none of the switch, so an invalid
// ruled_direction copies straight through.
RuledSurface::RuledSurface(const RuledSurface& other) {
    edge0 = other.edge0->dup();
    edge1 = other.edge1->dup();
    ruled_direction = other.ruled_direction;
    pure2D = other.pure2D;
    p00 = other.p00;
    p10 = other.p10;
    p01 = other.p01;
    p11 = other.p11;
}

// D:59-62. Through the validating constructor, which dups both edges again.
std::shared_ptr<ParametricSurface> RuledSurface::dup() const {
    return std::make_shared<RuledSurface>(edge0, edge1, ruled_direction, pure2D);
}

// D:64-81.
Vector3 RuledSurface::operator()(double r, double s) const {
    Vector3 p; // D:66 — Vector3.init (NaN), overwritten by the blend below.
    if (ruled_direction == "r" || ruled_direction == "i") {
        p = (1.0 - r) * (*edge0)(s) + r * (*edge1)(s); // D:70
    } else if (ruled_direction == "s" || ruled_direction == "j") {
        p = (1.0 - s) * (*edge0)(r) + s * (*edge1)(r); // D:74
    } else {
        // D:76-77. Reachable: ruled_direction is a public field (D:17).
        throw eil::ExceptionBase("Invalid string for ruled_direction: " + ruled_direction,
                                 __FILE__, __LINE__);
    }
    if (pure2D) { p.z = 0.0; } // D:79
    return p;
} // end opCall()

// D:83-87. d_tostr on a Path handle is D's to!string of a class: the virtual
// toString(), or "null" for a null handle (path.hpp:130).
std::string RuledSurface::toString() const {
    return "RuledSurface(edge0=" + d_tostr(edge0) + ", edge1=" + d_tostr(edge1) +
           ", ruled_direction=" + ruled_direction + ", pure2D=" + eil::to_string(pure2D) + ")";
}

} // namespace geom
} // namespace eil
