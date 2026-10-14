// geom/volume/wedgevolume.d -> src/geom/volume/wedgevolume.cpp
// (module geom.volume.wedgevolume) — see wedgevolume.hpp for the notes.

#include "geom/volume/wedgevolume.hpp"

#include <cmath>
#include <memory>
#include <string>

#include "eil/format.hpp"
#include "eil/math.hpp"
#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/surface/parametricsurface.hpp"
#include "geom/volume/parametricvolume.hpp"

namespace {

using eil::geom::ParametricSurface;

// D's `to!string(classHandle)` renders through the object's virtual toString(), and a
// null handle prints "null" (measured here: wv.nullface.str). Same spelling as
// src/geom/volume/twosurfacevolume.cpp:22-24.
std::string tostr(const ParametricSurface::Ptr& s) {
    return s ? s->toString() : std::string("null");
}

} // namespace

namespace eil {
namespace geom {

// D:23-35. The parameters shadow the members exactly as in D, so the member
// assignments go through `this->` and, as in D, the four face evaluations below call
// the ARGUMENT handle (D:27-30 write `face0123(0.0,0.0)` with the parameter in scope),
// not the stored dup.
WedgeVolume::WedgeVolume(const ParametricSurface::Ptr& face0123, number dtheta) {
    this->face0123 = face0123->dup(); // D:25 stores a copy, not the caller's handle
    this->dtheta = dtheta;            // D:26
    this->p[0] = (*face0123)(0.0, 0.0);   // D:27
    this->p[1] = (*face0123)(1.0, 0.0);   // D:28
    this->p[2] = (*face0123)(1.0, 1.0);   // D:29
    this->p[3] = (*face0123)(0.0, 1.0);   // D:30
    this->p[4] = sweep_through_arc(this->p[0], dtheta); // D:31
    this->p[5] = sweep_through_arc(this->p[1], dtheta); // D:32
    this->p[6] = sweep_through_arc(this->p[2], dtheta); // D:33
    this->p[7] = sweep_through_arc(this->p[3], dtheta); // D:34
} // end WedgeVolume()

// D:37-42. Copies the STORED corners (D:41 `this.p[i] = other.p[i].dup()`, a value
// copy of a Vector3 struct) rather than re-deriving them, so it and dup() differ once
// dtheta or p[] is written through the public fields.
WedgeVolume::WedgeVolume(const WedgeVolume& other) {
    face0123 = other.face0123->dup();            // D:39
    dtheta = other.dtheta;                       // D:40
    for (size_t i = 0; i < 8; ++i) {             // D:41 foreach(i; 0 .. 8)
        this->p[i] = other.p[i].dup();
    }
} // end WedgeVolume()

// D:44-47. Back through the constructor, which re-derives the eight corners from the
// (re-duped) face and the current dtheta.
std::shared_ptr<ParametricVolume> WedgeVolume::dup() const {
    return std::make_shared<WedgeVolume>(face0123, dtheta);
}

// D:49-61. Interpolate on the bottom surface, then sweep about the x-axis by t*dtheta.
Vector3 WedgeVolume::operator()(double r, double s, double t) const {
    Vector3 p_rs = (*face0123)(r, s); // D:59
    return sweep_through_arc(p_rs, t * dtheta); // D:60
} // end opCall

// D:63-68. dtheta prints through to!string, i.e. %.6g (wave-02-brief section 5).
std::string WedgeVolume::toString() const {
    std::string repr = "WedgeVolume(face0123=" + tostr(face0123); // D:65
    repr += ", dtheta=" + eil::to_string(dtheta) + ")";           // D:66
    return repr;
} // end toString

// D:71-80, private. Rotate p0 about the x-axis by theta, right-handed, angles measured
// from the +y axis toward +z (D:73-75). Statement order is D's: r first, then theta0,
// then theta1, then the two products. sqrt is eil::sqrt (positive NaN on a domain
// error), atan2 is eil::atan2 (Phobos' own implementation — see the header note; using
// std::atan2 here is the T-910 mistake), `^^2` is eil::pow, and cos/sin are the library
// ones because D's sin(double)/cos(double) were measured bit-identical to them.
Vector3 WedgeVolume::sweep_through_arc(const Vector3& p0, number theta) const {
    number r = eil::sqrt(eil::pow(p0.y, 2) + eil::pow(p0.z, 2)); // D:76
    number theta0 = eil::atan2(p0.z, p0.y);                       // D:77
    number theta1 = theta0 + theta;                               // D:78
    return Vector3(p0.x, r * std::cos(theta1), r * std::sin(theta1)); // D:79
} // end sweep_through_arc()

} // namespace geom
} // namespace eil
