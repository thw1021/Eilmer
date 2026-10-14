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

// ---------------------------------------------------------------------------
// d_cos / d_sin — why the two calls go through a volatile pointer.
//
// D calls cos() and sin() as two separate libm entries: std/math/trigonometry.d:74 and
// :132 forward to core.math, LDC lowers those to llvm.cos.f64 / llvm.sin.f64, and LLVM
// emits two calls. GCC at -O1 and above recognises a cos(x)/sin(x) pair in one
// expression and rewrites it into a single call to glibc's `sincos` — which is NOT the
// same function. Measured on this machine with the argument this module actually
// produces, x = theta1 = 0x3fcc185df5fdd150 (0.21949362288763511):
//
//   ldc2 binary, cos(x)            0x3fef3b74c62efebb  0.97600783069352515
//   g++ -O0, std::cos(x)           0x3fef3b74c62efebb  (same function, same answer)
//   g++ -O1/-O2/-O3, fused sincos  0x3fef3b74c62efeba  0.97600783069352504  (1 ULP low)
//
// and the wedge's y lands at 0x3fc0d1af56120768 against D's 0x3fc0d1af56120769 — 20 of
// the 4594 corpus rows differ at -O3, 0 of 4594 at -O0 (objdump of the -O1 object shows
// one `sincos@plt` where the -O0 object has none). The volatile pointers keep the two
// calls two calls; nothing else in sweep_through_arc changes. This is the same class of
// compiler substitution as the atan2 one in wave-02-brief section 5 (T-910), so it is
// fixed on the port side, not by widening a tolerance.
// ---------------------------------------------------------------------------
double (*volatile d_cos_ptr)(double) = &std::cos;
double (*volatile d_sin_ptr)(double) = &std::sin;

inline eil::number d_cos(eil::number x) { return d_cos_ptr(x); }
inline eil::number d_sin(eil::number x) { return d_sin_ptr(x); }

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
// ones because D's sin(double)/cos(double) were measured bit-identical to them — as
// long as they stay two separate calls, which is what d_cos/d_sin above arrange.
Vector3 WedgeVolume::sweep_through_arc(const Vector3& p0, number theta) const {
    number r = eil::sqrt(eil::pow(p0.y, 2) + eil::pow(p0.z, 2)); // D:76
    number theta0 = eil::atan2(p0.z, p0.y);                       // D:77
    number theta1 = theta0 + theta;                               // D:78
    return Vector3(p0.x, r * d_cos(theta1), r * d_sin(theta1));     // D:79
} // end sweep_through_arc()

} // namespace geom
} // namespace eil
