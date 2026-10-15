// geom/gpath/modifiedpath.d -> src/geom/gpath/modifiedpath.cpp
// (module geom.gpath.modifiedpath).

#include "geom/gpath/modifiedpath.hpp"

#include <cmath>
#include <cstddef>
#include <memory>
#include <string>

#include "eil/conv.hpp"
#include "eil/math.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"

namespace eil {
namespace geom {

// ---------------------------------------------------------------------------
// ReParameterizedPath
// ---------------------------------------------------------------------------

// D:19-22
ReParameterizedPath::ReParameterizedPath(const PathPtr& other)
    : underlying_path(other->dup()) {}

// D:23-27
Vector3 ReParameterizedPath::operator()(double t) const {
    double tdsh = underlying_t(t);
    return (*underlying_path)(tdsh);
}

// D:28-32
Vector3 ReParameterizedPath::dpdt(double t) const {
    double tdsh = underlying_t(t);
    return underlying_path->dpdt(tdsh) * d_underlying_t_dt(tdsh);
}

// D:33-38. D writes std.math.pow(d_underlying_t_dt(tdsh), 2) — eil::pow, not a
// multiplication (brief section 5).
Vector3 ReParameterizedPath::d2pdt2(double t) const {
    double tdsh = underlying_t(t);
    return underlying_path->d2pdt2(tdsh) * eil::sqr(d_underlying_t_dt(tdsh)) +
        underlying_path->dpdt(tdsh) * d2_underlying_t_dt2(tdsh);
}

// D:39-42, the body of the abstract toString()
std::string ReParameterizedPath::toString() const {
    return "ReParameterizedPath(underlying_path=" + d_tostr(underlying_path) + ")";
}

// D:43-46
std::string ReParameterizedPath::classString() const { return "ReParameterizedPath"; }

// ---------------------------------------------------------------------------
// ArcLengthParameterizedPath
// ---------------------------------------------------------------------------

// D:57-61
ArcLengthParameterizedPath::ArcLengthParameterizedPath(const PathPtr& other)
    : ReParameterizedPath(other) {
    set_arc_length_vector(100);
}

// D:62-65, `this(ref const(ArcLengthParameterizedPath) other) { this(other.underlying_path); }`
ArcLengthParameterizedPath::ArcLengthParameterizedPath(const ArcLengthParameterizedPath& other)
    : ArcLengthParameterizedPath(other.underlying_path) {}

// D:66-69
std::shared_ptr<Path> ArcLengthParameterizedPath::dup() const {
    return std::make_shared<ArcLengthParameterizedPath>(this->underlying_path);
}

// D:70-73
std::string ArcLengthParameterizedPath::toString() const {
    return "ArcLengthParameterizedPath(underlying_path=" + d_tostr(underlying_path) + ")";
}

// D:74-77. "ArcLengthParametrizedPath" is how D spells it (missing 'e'); kept.
std::string ArcLengthParameterizedPath::classString() const {
    return "ArcLengthParametrizedPath";
}

// D:81-100
void ArcLengthParameterizedPath::set_arc_length_vector(int N) {
    // Compute the arc_lengths for a number of sample points
    // so that these can later be used to do a reverse interpolation
    // on the evaluation parameter.
    arc_length_vector.set_length(0);
    if (N == 0) return;
    double dt = 1.0 / N;
    double L = 0.0;
    arc_length_vector.append(L);
    Vector3 p0 = (*underlying_path)(0.0);
    Vector3 p1;
    for (int i = 1; i <= N; ++i) { // D: foreach (i; 1 .. N+1)
        p1 = (*underlying_path)(dt * i);
        Vector3 dp = p1 - p0;
        L += eil::re(eil::geom::abs(dp));
        arc_length_vector.append(L);
        p0 = p1;
    }
} // end set_arc_length_vector()

// D:101-119
double ArcLengthParameterizedPath::underlying_t(double t) const {
    // The incoming parameter value, t, is proportional to arc_length fraction.
    if (t <= 0.0) { return 0.0; }
    if (t >= 1.0) { return 1.0; }
    // Do a reverse look-up from the arc_length fraction to the original t parameter
    // of the underlying Path.
    double L_target = t * arc_length_vector[arc_length_vector.length() - 1];
    // Starting from the right-hand end,
    // let's try to find a point to the left of L_target.
    // If the value is out of range, this should just result in
    // us extrapolating one of the end segments -- that's OK.
    int i = static_cast<int>(arc_length_vector.length()) - 1;  // D: to!int(length)
    double dt = 1.0 / (arc_length_vector.length() - 1);
    while (L_target < arc_length_vector[i] && i > 0) i--;
    double frac = (L_target - arc_length_vector[i]) /
        (arc_length_vector[i + 1] - arc_length_vector[i]);
    return (1.0 - frac) * dt * i + frac * dt * (i + 1);
}

// D:120-125. The argument is named tdsh because D documents it as the *underlying*
// parameter ("input t is underlying_t"), unlike the base-class virtual's t.
double ArcLengthParameterizedPath::d_underlying_t_dt(double tdsh) const {
    // derivative of inverse fn
    return 1.0 / d_arc_f_dt(tdsh);
}

// D:126-131
double ArcLengthParameterizedPath::d2_underlying_t_dt2(double tdsh) const {
    // derivative of inverse fn
    return -d2_arc_f_dt2(tdsh) / eil::pow(d_arc_f_dt(tdsh), 3.0);
}

// D:133-138
double ArcLengthParameterizedPath::d_arc_f_dt(double t) const {
    // "t" is underlying_t
    Vector3 dpdt = underlying_path->dpdt(t);
    return eil::re(eil::geom::abs(dpdt)) / arc_length_vector[arc_length_vector.length() - 1];
}

// D:139-147
double ArcLengthParameterizedPath::d2_arc_f_dt2(double t) const {
    // "t" is underlying_t
    //chain rule on d_arc_f_dt
    Vector3 dpdt = underlying_path->dpdt(t);
    dpdt.normalize(); // direction only
    Vector3 d2pdt2 = underlying_path->d2pdt2(t);
    return eil::re(dot(dpdt, d2pdt2)) / arc_length_vector[arc_length_vector.length() - 1];
}

// ---------------------------------------------------------------------------
// SubRangedPath
// ---------------------------------------------------------------------------

// D:155-160
SubRangedPath::SubRangedPath(const PathPtr& other, double newt0, double newt1)
    : ReParameterizedPath(other) {
    t0 = newt0;
    t1 = newt1;
}

// D:161-164
SubRangedPath::SubRangedPath(const SubRangedPath& other)
    : SubRangedPath(other.underlying_path, other.t0, other.t1) {}

// D:165-168
std::shared_ptr<Path> SubRangedPath::dup() const {
    return std::make_shared<SubRangedPath>(this->underlying_path, this->t0, this->t1);
}

// D:169-173. Note D's separator between t0 and t1 is a space, not ", " — kept,
// because toString() bytes go into logs (modifiedpath.d:171-172).
std::string SubRangedPath::toString() const {
    return "SubRangedPath(underlying_path=" + d_tostr(underlying_path) +
        ", t0=" + d_tostr(t0) + " t1=" + d_tostr(t1) + ")";
}

// D:174-177
std::string SubRangedPath::classString() const { return "SubRangedPath"; }

// D:179-182
double SubRangedPath::underlying_t(double t) const { return t0 + (t1 - t0) * t; }

// D:183-186
double SubRangedPath::d_underlying_t_dt(double /*t*/) const { return t1 - t0; }

// D:187-190
double SubRangedPath::d2_underlying_t_dt2(double /*t*/) const { return 0.0; }

// ---------------------------------------------------------------------------
// ReversedPath
// ---------------------------------------------------------------------------

// D:196-199
ReversedPath::ReversedPath(const PathPtr& other) : SubRangedPath(other, 1.0, 0.0) {}

// D:200-203
std::shared_ptr<Path> ReversedPath::dup() const {
    return std::make_shared<ReversedPath>(this->underlying_path);
}

// ---------------------------------------------------------------------------
// TransformedPath
// ---------------------------------------------------------------------------

// D:210-213
TransformedPath::TransformedPath(const PathPtr& other) : original_path(other->dup()) {}

// D:214-218
Vector3 TransformedPath::operator()(double t) const {
    Vector3 p = (*original_path)(t);
    return apply_transform(p);
}

// D:219-222, the body of the abstract toString()
std::string TransformedPath::toString() const {
    return "TransformedPath(original_path=" + d_tostr(original_path) + ")";
}

// D:223-226
std::string TransformedPath::classString() const { return "TransformedPath"; }

// ---------------------------------------------------------------------------
// TranslatedPath
// ---------------------------------------------------------------------------

// D:236-240
TranslatedPath::TranslatedPath(const PathPtr& other, const Vector3& vshift)
    : TransformedPath(other), shift(vshift) {}

// D:241-244
std::shared_ptr<Path> TranslatedPath::dup() const {
    return std::make_shared<TranslatedPath>(this->original_path, this->shift);
}

// D:245-249
std::string TranslatedPath::toString() const {
    return "TranslatedPath(original_path=" + d_tostr(original_path) +
        ", shift=" + d_tostr(shift) + ")";
}

// D:250-253
std::string TranslatedPath::classString() const { return "TranslatedPath"; }

// D:255-259
Vector3 TranslatedPath::apply_transform(Vector3& p) const { return p + shift; }

// ---------------------------------------------------------------------------
// MirrorImagePath
// ---------------------------------------------------------------------------

// D:266-270
MirrorImagePath::MirrorImagePath(const PathPtr& other, const Vector3& vpoint,
                                 const Vector3& vnormal)
    : TransformedPath(other), point(vpoint), normal(vnormal) {}

// D:271-274
std::shared_ptr<Path> MirrorImagePath::dup() const {
    return std::make_shared<MirrorImagePath>(this->original_path, this->point, this->normal);
}

// D:275-280
std::string MirrorImagePath::toString() const {
    return "MirrorImagePath(original_path=" + d_tostr(original_path) +
        ", point=" + d_tostr(point) + ", normal=" + d_tostr(normal) + ")";
}

// D:281-284
std::string MirrorImagePath::classString() const { return "MirrorImagePath"; }

// D:286-290. mirror_image() mutates p in place and returns it, as in D.
Vector3 MirrorImagePath::apply_transform(Vector3& p) const { return p.mirror_image(point, normal); }

// ---------------------------------------------------------------------------
// RotatedAboutZAxisPath
// ---------------------------------------------------------------------------

// D:296-299
RotatedAboutZAxisPath::RotatedAboutZAxisPath(const PathPtr& other, double angle)
    : TransformedPath(other) {
    dtheta = angle;
}

// D:300-303
std::shared_ptr<Path> RotatedAboutZAxisPath::dup() const {
    return std::make_shared<RotatedAboutZAxisPath>(this->original_path, this->dtheta);
}

// D:304-308
std::string RotatedAboutZAxisPath::toString() const {
    return "RotatedAboutZAxisPath(original_path=" + d_tostr(original_path) +
        ", angle=" + d_tostr(dtheta) + ")";
}

// D:309-312
std::string RotatedAboutZAxisPath::classString() const { return "RotatedAboutZAxisPath"; }

// D:314-319
Vector3 RotatedAboutZAxisPath::apply_transform(Vector3& p) const {
    return p.rotate_about_zaxis(dtheta);
}

} // namespace geom
} // namespace eil
