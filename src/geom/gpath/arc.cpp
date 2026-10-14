// geom/gpath/arc.d -> src/geom/gpath/arc.cpp (module geom.gpath.arc).

#include "geom/gpath/arc.hpp"

#include <cmath>
#include <limits>
#include <memory>

#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/math.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"
#include "nm/bbla.hpp"

namespace eil {
namespace geom {

// ---------------------------------------------------------------------------
// phobos_atan / phobos_atan2 — thin aliases for the substrate's eil::atan and
// eil::atan2, which own Phobos' arc tangent (substrate/include/eil/math.hpp).
//
// Arc was the first consumer that needed it, and this module carried the replica
// until T-910 showed geom/elements/vector3.d:523 (rotate_about_zaxis) needs it just
// as much: a 1 ULP difference in the angle becomes a 2.9e-09 relative change in
// cos(theta) where theta sits on the pi/2 zero crossing, and r multiplies that into
// the coordinate. Two copies of a transcendental would drift, so there is now one.
// The names stay because tests/test_geom_gpath_arcsteps.cpp calls them.
// ---------------------------------------------------------------------------
double phobos_atan(double x) { return eil::atan(x); }
double phobos_atan2(double y, double x) { return eil::atan2(y, x); }

// D:22-25
Arc::Arc(const Vector3& va, const Vector3& vb, const Vector3& vc) : a(va), b(vb), c(vc) {}

// D:26-29
Arc::Arc(const Arc& other) : a(other.a), b(other.b), c(other.c) {}

// D:30-33
std::shared_ptr<Path> Arc::dup() const { return std::make_shared<Arc>(a, b, c); }

// D:34-40. D declares the locals with no initialiser, which is their .init
// (NaN-filled); the values are written by evaluate_position_and_length before use.
Vector3 Arc::operator()(double t) const {
    number L = eil::d_init<number>();
    Vector3 p;
    evaluate_position_and_length(t, p, L);
    return p;
}

// D:41-44
std::string Arc::toString() const {
    return "Arc(a=" + d_tostr(a) + ", b=" + d_tostr(b) + ", c=" + d_tostr(c) + ")";
}

// D:45-48
std::string Arc::classString() const { return "Arc"; }

// D:49-56
number Arc::length() const {
    // Returns the geometric length.
    number L = eil::d_init<number>();
    Vector3 p;
    evaluate_position_and_length(1.0, p, L);
    return L;
}

// D:58-101
void Arc::evaluate_position_and_length(double t, Vector3& loc, number& L) const {
    // D's `out Vector3 loc, out number L` reset the caller's variables to their
    // .init on entry. That is observable on the three throw paths below, where the
    // body never reaches the assignments, so the reset is reproduced.
    loc = Vector3();
    L = eil::d_init<number>();
    // Both the position of the point and the length of the full arc are evaluated
    // using mostly the same process of transforming to the plane local to the arc.
    Vector3 ca, cb, tangent1, tangent2, n, cb_local;
    number ca_mag, cb_mag, theta;

    L = 0.0;
    ca = a - c; ca_mag = eil::geom::abs(ca);
    cb = b - c; cb_mag = eil::geom::abs(cb);
    if (std::fabs(ca_mag - cb_mag) > 1.0e-5) {
        throw eil::ErrorBase(eil::text("Arc.evaluate(): radii do not match ca=", d_tostr(ca),
                                       " cb=", d_tostr(cb)),
                            __FILE__, __LINE__);
    }
    // First vector in plane.
    tangent1 = Vector3(ca); tangent1.normalize();
    // Compute unit normal to plane of all three points.
    n = cross(ca, cb);
    if (eil::geom::abs(n) > 0.0) {
        n.normalize();
    } else {
        throw eil::ErrorBase("Arc.evaluate(): cannot find plane of three points.",
                             __FILE__, __LINE__);
    }
    // Third (orthogonal) vector is in the original plane.
    tangent2 = cross(n, tangent1);
    // Now transform to local coordinates so that we can do
    // the calculation of the point along the arc in
    // the local xy-plane, with ca along the x-axis.
    cb_local = cb;
    Vector3 zerov = Vector3(0.0, 0.0, 0.0);
    cb_local.transform_to_local_frame(tangent1, tangent2, n, zerov);
    if (std::fabs(cb_local.z) > 1.0e-6) {
        throw eil::ErrorBase(eil::text("Arc.evaluate(): problem with transformation cb_local=",
                                       d_tostr(cb_local)),
                            __FILE__, __LINE__);
    }
    // Angle of the final point on the arc is in the range -pi < th <= +pi.
    // D's atan2 is Phobos' own atan2Impl, not libm's (see arc.hpp); 1 ULP here
    // moves every point on the arc and the length L.
    theta = phobos_atan2(cb_local.y, cb_local.x);
    // The length of the circular arc.
    L = theta * cb_mag;
    // Move the second point around the arc in the local xy-plane.
    theta *= t;
    loc.set(eil::cos(theta) * cb_mag, eil::sin(theta) * cb_mag, number(0.0));
    // Transform back to global xyz coordinates
    // and remember to add the centre coordinates.
    loc.transform_to_global_frame(tangent1, tangent2, n, c);
} // end evaluate_position_and_length()

// ---------------------------------------------------------------------------
// Arc3
// ---------------------------------------------------------------------------

// D:110-140, the statements that run before `super(a, b, c)` in D's constructor.
Vector3 Arc3::compute_centre(const Vector3& a, const Vector3& m, const Vector3& b) {
    Vector3 n = cross(m - a, m - b); // normal to plane of arc
    if (eil::re(eil::geom::abs(n)) <= 1.0e-11) {
        throw eil::ErrorBase(eil::text("Arc3: Points appear colinear.",
                                       " a=", d_tostr(a),
                                       " m=", d_tostr(m),
                                       " b=", d_tostr(b)),
                            __FILE__, __LINE__);
    }
    // The centre of the circle lies along the bisector of am and
    // the bisector of mb.
    number half = 0.5;
    Vector3 mid_am = half * (a + m);
    Vector3 bisect_am = cross(n, a - m);
    Vector3 mid_mb = half * (b + m);
    Vector3 bisect_mb = cross(n, m - b);
    // Solve least-squares problem to get s_am, s_mb.
    // nm/bbla.d keeps its Matrix/lsqsolve in the module scope, which the port
    // spells as the nested namespace eil::geom::bbla (nm/bbla.hpp:70-71).
    eil::nm::bbla::Matrix<number> amatrix = eil::nm::bbla::new_matrix<number>(
        {{bisect_am.x, -bisect_mb.x}, {bisect_am.y, -bisect_mb.y}, {bisect_am.z, -bisect_mb.z}});
    Vector3 diff_mid = mid_mb - mid_am;
    eil::nm::bbla::Matrix<number> rhs = eil::nm::bbla::new_matrix<number>(
        {diff_mid.x, diff_mid.y, diff_mid.z}, "column");
    eil::nm::bbla::Matrix<number> s_values = eil::nm::bbla::lsqsolve<number>(amatrix, rhs);
    number s_am = s_values(0, 0);
    number s_mb = s_values(1, 0);
    Vector3 ccentre = mid_am + s_am * bisect_am;
    Vector3 c_check = mid_mb + s_mb * bisect_mb;
    Vector3 delc = c_check - ccentre;
    if (eil::re(eil::geom::abs(delc)) > 1.0e-9) {
        throw eil::ErrorBase(eil::text("Arc3: Points inconsistent centre estimates.",
                                       " c=", d_tostr(ccentre),
                                       " c_check=", d_tostr(c_check)),
                            __FILE__, __LINE__);
    }
    return ccentre;
}

// D:108-143
Arc3::Arc3(const Vector3& a, const Vector3& m, const Vector3& b)
    : Arc(a, b, compute_centre(a, m, b)), m(m) {}

// D:144-147, `this(ref const(Arc3) other) { this(other.a, other.m, other.b); }`
Arc3::Arc3(const Arc3& other) : Arc3(other.a, other.m, other.b) {}

// D:148-151
std::shared_ptr<Path> Arc3::dup() const { return std::make_shared<Arc3>(a, m, b); }

// D:152-155
std::string Arc3::toString() const {
    return "Arc3(a=" + d_tostr(a) + ", m=" + d_tostr(m) + ", b=" + d_tostr(b) + ")";
}

// D:156-159
std::string Arc3::classString() const { return "Arc3"; }

} // namespace geom
} // namespace eil
