// geom/volume/slabvolume.d -> src/geom/volume/slabvolume.cpp
// (module geom.volume.slabvolume) — see slabvolume.hpp for the notes.

#include "geom/volume/slabvolume.hpp"

#include <memory>
#include <string>

#include "geom/elements/vector3.hpp"
#include "geom/gpath/line.hpp"
#include "geom/gpath/path.hpp"
#include "geom/surface/parametricsurface.hpp"
#include "geom/volume/parametricvolume.hpp"
#include "geom/volume/sweptsurfacevolume.hpp"

namespace {

using eil::geom::ParametricSurface;
using eil::geom::PathPtr;
using eil::geom::Vector3;

// D:25-26, factored out only because C++ cannot evaluate it inside a derived
// constructor body (the base subobject is already built by then). It performs
// exactly one call of face0123(0.0,0.0), as D's `Vector3 p0 = ...` does, and builds
// the same Line.
PathPtr make_edge04(const ParametricSurface::Ptr& face0123, const Vector3& dz) {
    Vector3 p0 = (*face0123)(0.0, 0.0); // D:25
    return std::make_shared<eil::geom::Line>(p0, p0 + dz); // D:26 new Line(p0, p0+dz)
}

// D's `to!string(classHandle)` renders through the object's virtual toString(), and
// a null handle prints "null" (measured with ldc2 1.40). Same spelling as
// eil::geom::d_tostr(const PathPtr&) at src/geom/gpath/path.hpp:127, kept file-local
// so this module does not depend on gpath's header being included first.
std::string tostr(const ParametricSurface::Ptr& s) {
    return s ? s->toString() : std::string("null");
}

} // namespace

namespace eil {
namespace geom {

// D:17-27. The parameter dz shadows the member exactly as in D (D:17), so the
// mem-initializer `dz(dz)` is D's `this.dz = dz` (D:24).
SlabVolume::SlabVolume(const ParametricSurface::Ptr& face0123, const Vector3& dz)
    : SweptSurfaceVolume(face0123, make_edge04(face0123, dz)), // D:26 super(...)
      dz(dz)                                                   // D:24
{}

// D:36-39. Back through the two-argument constructor, which dups the face (the base
// constructor does that at sweptsurfacevolume.d:22) and rebuilds the Line from the
// stored face and this slab's dz.
std::shared_ptr<ParametricVolume> SlabVolume::dup() const {
    return std::make_shared<SlabVolume>(this->face0123, this->dz);
}

// D:41-46, concatenated in the same two steps D uses.
std::string SlabVolume::toString() const {
    std::string repr = "SlabVolume(face0123=" + tostr(face0123); // D:43
    repr += ", dz=" + dz.toString() + ")";                       // D:44
    return repr;
}

} // namespace geom
} // namespace eil
