// geom/volume/subrangedvolume.d -> src/geom/volume/subrangedvolume.cpp
// (module geom.volume.subrangedvolume) — see subrangedvolume.hpp for the notes.

#include "geom/volume/subrangedvolume.hpp"

#include <memory>
#include <string>

#include "eil/format.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/volume/parametricvolume.hpp"

namespace {

// D's `to!string(classHandle)` renders through the object's virtual toString(), and
// a null handle prints "null" (measured: `ParametricVolume pv; to!string(pv)` gives
// "null" under ldc2 1.40). Same spelling as eil::geom::d_tostr(const PathPtr&) at
// src/geom/gpath/path.hpp:127, kept file-local so this module does not depend on
// gpath.
std::string tostr(const eil::geom::ParametricVolume::Ptr& v) {
    return v ? v->toString() : std::string("null");
}

} // namespace

namespace eil {
namespace geom {

// D:22-34. The parameters shadow the members exactly as in D, so the assignments
// that mean the members are written through `this.`, as D does.
SubRangedVolume::SubRangedVolume(const ParametricVolume::Ptr& pvolume, double r0, double r1,
                                 double s0, double s1, double t0, double t1) {
    underlying_volume = pvolume->dup(); // D:27 — stores a copy, not the caller's handle
    this->r0 = r0;                      // D:28
    this->r1 = r1;                      // D:29
    this->s0 = s0;                      // D:30
    this->s1 = s1;                      // D:31
    this->t0 = t0;                      // D:32
    this->t1 = t1;                      // D:33
}

// D:35-41
Vector3 SubRangedVolume::operator()(double r, double s, double t) const {
    r = r0 + (r1 - r0) * r; // subrange the parameter
    s = s0 + (s1 - s0) * s;
    t = t0 + (t1 - t0) * t;
    return underlying_volume->operator()(r, s, t);
}

// D:42-46. Back through the constructor, which dups the underlying volume again.
std::shared_ptr<ParametricVolume> SubRangedVolume::dup() const {
    return std::make_shared<SubRangedVolume>(underlying_volume, r0, r1, s0, s1, t0, t1);
}

// D:47-55
std::string SubRangedVolume::toString() const {
    return "SubRangedVolume(underlying_volume=" + tostr(underlying_volume) +
        ", r0=" + eil::to_string(r0) + ", r1=" + eil::to_string(r1) +
        ", s0=" + eil::to_string(s0) + ", s1=" + eil::to_string(s1) +
        ", t0=" + eil::to_string(t0) + ", t1=" + eil::to_string(t1) + ")";
}

} // namespace geom
} // namespace eil
