// geom/volume/twosurfacevolume.d -> src/geom/volume/twosurfacevolume.cpp
// (module geom.volume.twosurfacevolume) — see twosurfacevolume.hpp for the notes.

#include "geom/volume/twosurfacevolume.hpp"

#include <memory>
#include <string>

#include "eil/error.hpp"
#include "eil/format.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/surface/parametricsurface.hpp"
#include "geom/volume/parametricvolume.hpp"

namespace {

using eil::geom::ParametricSurface;

// D's `to!string(classHandle)` renders through the object's virtual toString(), and a
// null handle prints "null" (measured with ldc2 1.40). Same spelling as
// eil::geom::d_tostr(const PathPtr&) at src/geom/gpath/path.hpp:127, kept file-local.
std::string tostr(const ParametricSurface::Ptr& s) {
    return s ? s->toString() : std::string("null");
}

} // namespace

namespace eil {
namespace geom {

// D:51-81. The parameters shadow the members exactly as in D, so the assignments that
// mean the members go through `this->`.
TwoSurfaceVolume::TwoSurfaceVolume(const ParametricSurface::Ptr& face0,
                                   const ParametricSurface::Ptr& face1,
                                   const std::string& ruled_direction) {
    this->face0 = face0->dup();            // D:53 — stores a copy, not the caller's handle
    this->face1 = face1->dup();            // D:54
    this->ruled_direction = ruled_direction; // D:55

    // D:56-80. A string switch in D; an if-chain in exactly the same case order here.
    if (ruled_direction == "r" || ruled_direction == "i") {
        // D:57-63: face0 is the west face.
        this->p[0] = (*this->face0)(0.0, 0.0);
        this->p[3] = (*this->face0)(1.0, 0.0);
        this->p[7] = (*this->face0)(1.0, 1.0);
        this->p[4] = (*this->face0)(0.0, 1.0);
        // face1 is the east face (D:62)
        this->p[1] = (*this->face1)(0.0, 0.0);
        this->p[2] = (*this->face1)(1.0, 0.0);
        this->p[6] = (*this->face1)(1.0, 1.0);
        this->p[5] = (*this->face1)(0.0, 1.0);
    } else if (ruled_direction == "s" || ruled_direction == "j") {
        // D:64-70: face0 is the south face.
        this->p[0] = (*this->face0)(0.0, 0.0);
        this->p[1] = (*this->face0)(1.0, 0.0);
        this->p[5] = (*this->face0)(1.0, 1.0);
        this->p[4] = (*this->face0)(0.0, 1.0);
        // face1 is the north face (D:69)
        this->p[3] = (*this->face1)(0.0, 0.0);
        this->p[2] = (*this->face1)(1.0, 0.0);
        this->p[6] = (*this->face1)(1.0, 1.0);
        this->p[7] = (*this->face1)(0.0, 1.0);
    } else if (ruled_direction == "t" || ruled_direction == "k") {
        // D:71-77: face0 is the bottom face.
        this->p[0] = (*this->face0)(0.0, 0.0);
        this->p[1] = (*this->face0)(1.0, 0.0);
        this->p[2] = (*this->face0)(1.0, 1.0);
        this->p[3] = (*this->face0)(0.0, 1.0);
        // face1 is the top face (D:76)
        this->p[4] = (*this->face1)(0.0, 0.0);
        this->p[5] = (*this->face1)(1.0, 0.0);
        this->p[6] = (*this->face1)(1.0, 1.0);
        this->p[7] = (*this->face1)(0.0, 1.0);
    } else {
        // D:78-79.
        throw eil::ExceptionBase("Invalid string for ruled_direction: " + ruled_direction,
                                 __FILE__, __LINE__);
    }
} // end TwoSurfaceVolume()

// D:83-89. Note this copies the stored corners rather than re-deriving them from the
// (re-duped) faces, so it and dup() differ once the faces move.
TwoSurfaceVolume::TwoSurfaceVolume(const TwoSurfaceVolume& other) {
    face0 = other.face0->dup(); // D:85
    face1 = other.face1->dup(); // D:86
    ruled_direction = other.ruled_direction; // D:87
    for (size_t i = 0; i < 8; ++i) {         // D:88 foreach(i; 0 .. 8)
        this->p[i] = other.p[i].dup();
    }
}

// D:91-94. Back through the validating constructor, which dups both faces again and
// re-derives the eight corners.
std::shared_ptr<ParametricVolume> TwoSurfaceVolume::dup() const {
    return std::make_shared<TwoSurfaceVolume>(face0, face1, ruled_direction);
}

// D:96-124. Locate a point within the volume by linear interpolation between
// respective points on bottom and top surface.
Vector3 TwoSurfaceVolume::operator()(double r, double s, double t) const {
    Vector3 p_rst; // D:106 — Vector3.init (NaN) until the branch assigns it
    // D:107-122, the same if-chain in the same case order as the constructor.
    if (ruled_direction == "r" || ruled_direction == "i") {
        // D:110 VERBATIM, including `1.0-1`: the scalar on face0 is the literal
        // expression 1.0-1, i.e. exactly 0.0, NOT (1.0-r). Measured in the corpus
        // (tsv.i.discards_face0 / tsv.i.face0_unused are both true), so this branch
        // discards face0; see the header note.
        p_rst = (*face0)(s, t).scale(1.0 - 1) + (*face1)(s, t).scale(r);
    } else if (ruled_direction == "s" || ruled_direction == "j") {
        p_rst = (*face0)(r, t).scale(1.0 - s) + (*face1)(r, t).scale(s); // D:114
    } else if (ruled_direction == "t" || ruled_direction == "k") {
        p_rst = (*face0)(r, s).scale(1.0 - t) + (*face1)(r, s).scale(t); // D:118
    } else {
        // D:120-121. Reachable, because ruled_direction is a public field (D:48).
        throw eil::ExceptionBase("Invalid string for ruled_direction: " + ruled_direction,
                                 __FILE__, __LINE__);
    }
    return p_rst;
} // end opCall

// D:126-133. ruled_direction is appended RAW (D:130), so the text carries the
// spelling the caller used and no quotes.
std::string TwoSurfaceVolume::toString() const {
    std::string repr = "TwoSurfaceVolume(face0=" + tostr(face0); // D:128
    repr += ", face1=" + tostr(face1);                           // D:129
    repr += ", ruled_direction=" + ruled_direction;              // D:130
    repr += ")";                                                 // D:131
    return repr;
} // end toString

} // namespace geom
} // namespace eil
