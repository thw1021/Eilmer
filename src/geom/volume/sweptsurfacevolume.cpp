// geom/volume/sweptsurfacevolume.d -> src/geom/volume/sweptsurfacevolume.cpp
// (module geom.volume.sweptsurfacevolume) — see sweptsurfacevolume.hpp for the notes.

#include "geom/volume/sweptsurfacevolume.hpp"

#include <memory>
#include <string>

#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"
#include "geom/surface/parametricsurface.hpp"
#include "geom/volume/parametricvolume.hpp"

namespace {

using eil::geom::ParametricSurface;
using eil::geom::PathPtr;

// D's `to!string(classHandle)` renders through the object's virtual toString(), and a
// null handle prints "null" (measured here: ssv.nullface.str). Same spelling as
// src/geom/volume/twosurfacevolume.cpp:22-24 and eil::geom::d_tostr(const PathPtr&) at
// src/geom/gpath/path.hpp:130.
std::string tostr(const ParametricSurface::Ptr& s) {
    return s ? s->toString() : std::string("null");
}
std::string tostr(const PathPtr& p) {
    return p ? p->toString() : std::string("null");
}

} // namespace

namespace eil {
namespace geom {

// D:20-32. The parameters shadow the members exactly as in D, so the assignments that
// mean the members go through `this->`. Each corner re-evaluates edge04 and the face
// just as D's own lines do — D calls edge04(0.0) four times and face0123(0,0) four
// times here, and that call pattern is preserved (it matters for a face or path with
// side effects, and for call-count parity with the reference).
SweptSurfaceVolume::SweptSurfaceVolume(const ParametricSurface::Ptr& face0123,
                                       const PathPtr& edge04) {
    this->face0123 = face0123->dup(); // D:22 stores a copy, not the caller's handle
    this->edge04 = edge04->dup();     // D:23
    this->p[0] = (*this->edge04)(0.0); // D:24 — no face subtraction here, see the header
    this->p[1] = (*this->edge04)(0.0) + (*this->face0123)(1.0, 0.0)
                 - (*this->face0123)(0.0, 0.0); // D:25
    this->p[2] = (*this->edge04)(0.0) + (*this->face0123)(1.0, 1.0)
                 - (*this->face0123)(0.0, 0.0); // D:26
    this->p[3] = (*this->edge04)(0.0) + (*this->face0123)(0.0, 1.0)
                 - (*this->face0123)(0.0, 0.0); // D:27
    this->p[4] = (*this->edge04)(1.0);          // D:28
    this->p[5] = (*this->edge04)(1.0) + (*this->face0123)(1.0, 0.0)
                 - (*this->face0123)(0.0, 0.0); // D:29
    this->p[6] = (*this->edge04)(1.0) + (*this->face0123)(1.0, 1.0)
                 - (*this->face0123)(0.0, 0.0); // D:30
    this->p[7] = (*this->edge04)(1.0) + (*this->face0123)(0.0, 1.0)
                 - (*this->face0123)(0.0, 0.0); // D:31
} // end SweptSurfaceVolume()

// D:34-39. Copies the STORED corners (D:38 `this.p[i] = other.p[i].dup()`, a value
// copy of a Vector3 struct) rather than re-deriving them, which is why it and dup()
// differ once p[] is written to — measured in the corpus.
SweptSurfaceVolume::SweptSurfaceVolume(const SweptSurfaceVolume& other) {
    face0123 = other.face0123->dup(); // D:36
    edge04 = other.edge04->dup();     // D:37
    for (size_t i = 0; i < 8; ++i) {  // D:38 foreach(i; 0 .. 8)
        this->p[i] = other.p[i].dup();
    }
} // end SweptSurfaceVolume()

// D:41-44. Back through the two-argument constructor, which dups both members again
// and re-derives the eight corners.
std::shared_ptr<ParametricVolume> SweptSurfaceVolume::dup() const {
    return std::make_shared<SweptSurfaceVolume>(face0123, edge04);
}

// D:46-57. Locate a point within the volume by translating the bottom face along the
// sweep path. The order of the three terms is D's, left to right.
Vector3 SweptSurfaceVolume::operator()(double r, double s, double t) const {
    Vector3 p_rst = (*edge04)(t) + (*face0123)(r, s) - (*face0123)(0.0, 0.0); // D:55
    return p_rst;                                                            // D:56
} // end opCall

// D:59-64. Both members go through to!string (their own virtual toString()).
std::string SweptSurfaceVolume::toString() const {
    std::string repr = "SweptSurfaceVolume(face0123=" + tostr(face0123); // D:61
    repr += ", edge04=" + tostr(edge04) + ")";                           // D:62
    return repr;
} // end toString

} // namespace geom
} // namespace eil
