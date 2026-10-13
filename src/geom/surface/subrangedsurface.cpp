// geom/surface/subrangedsurface.d -> src/geom/surface/subrangedsurface.cpp
// (module geom.surface.subrangedsurface).
//
// Definitions in D source order: the constructor (D:18-26), opCall (D:27-32), dup
// (D:33-36), toString (D:37-44). The arithmetic order inside opCall is D's: r is
// remapped first (D:29), then s (D:30), then the pair is forwarded (D:31); the two
// multiplications ` (r1-r0)*r ` are not rearranged into r*r1 - r*r0, which would not
// round the same way.
//
// See the header for the non-1:1 notes (adapted-handles, the absent classString, the
// dup-in-the-constructor aliasing, and the measured SIGSEGV of a null underlying
// surface).
#include "geom/surface/subrangedsurface.hpp"

#include <memory>
#include <string>

#include "eil/format.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/surface/parametricsurface.hpp"

namespace eil {
namespace geom {

// D:18-26. The member is the *dup* of the argument (D:21), so this adapter owns its
// underlying surface.
SubRangedSurface::SubRangedSurface(const ParametricSurface::Ptr& psurf, double r0, double r1,
                                   double s0, double s1)
    : underlying_surface(psurf->dup()), r0(r0), r1(r1), s0(s0), s1(s1) {}

// D:27-32.
Vector3 SubRangedSurface::operator()(double r, double s) const {
    r = r0 + (r1 - r0) * r; // D:29 subrange the parameter
    s = s0 + (s1 - s0) * s; // D:30
    return (*underlying_surface)(r, s); // D:31
}

// D:33-36. Back through the constructor, which dups the member again — so a dup of a
// chain keeps the chain depth instead of sharing the inner surface.
std::shared_ptr<ParametricSurface> SubRangedSurface::dup() const {
    return std::make_shared<SubRangedSurface>(underlying_surface, r0, r1, s0, s1);
}

// D:37-44. to!string of the class handle renders through its virtual toString()
// ("null" for a null handle, which is unreachable — see the header).
std::string SubRangedSurface::toString() const {
    return "SubRangedSurface(underlying_surface=" +
           (underlying_surface ? underlying_surface->toString() : std::string("null")) +
           ", r0=" + eil::to_string(r0) + ", r1=" + eil::to_string(r1) +
           ", s0=" + eil::to_string(s0) + ", s1=" + eil::to_string(s1) + ")";
}

} // namespace geom
} // namespace eil
