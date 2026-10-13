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
// phobos_atan / phobos_atan2 — see the declaration in arc.hpp for why Arc needs
// D's own arc tangent rather than libm's. Translated statement for statement from
// std/math/trigonometry.d: atan2Impl (990-1035) and the `realFormat ==
// RealFormat.ieeeDouble` branch of atanImpl (694-846), with T = double.
// ---------------------------------------------------------------------------
namespace {

// D's constants are 80-bit `real` and reach a `double` only through the casts the
// Phobos source writes, so the wide value is computed here and rounded on store
// (wave-02-brief section 5).
constexpr double kPI = static_cast<double>(EIL_PI);
constexpr double kPI_2 = static_cast<double>(EIL_PI / 2.0L);
constexpr double kPI_4 = static_cast<double>(EIL_PI / 4.0L);

// atanImpl!double coefficients (trigonometry.d:753-766). Written as long-double
// literals because that is how D spells them; the narrow to double is D's too.
const double kP[5] = {
    -6.485021904942025371773E1L, -1.228866684490136173410E2L, -7.500855792314704667340E1L,
    -1.615753718733365076637E1L, -8.750608600031904122785E-1L,
};
const double kQ[6] = {
    1.945506571482613964425E2L,  4.853903996359136964868E2L, 4.328810604912902668951E2L,
    1.650270098316988542046E2L,  2.485846490142306297962E1L, 1.000000000000000000000E0L,
};
constexpr double kMOREBITS = static_cast<double>(6.123233995736765886130E-17L);
// tan(PI/8) and tan(3*PI/8), as D spells them (trigonometry.d:779-782).
constexpr double kTAN_PI_8 = static_cast<double>(0.414213562373095048801688724209698078569672L);
constexpr double kTAN3_PI_8 = static_cast<double>(2.414213562373095048801688724209698078569672L);

// std.math.algebraic.poly with the statically unrolled `ref const T[N]` overload
// (algebraic.d:557-566): Horner from the highest coefficient down.
template <size_t N> double poly(double x, const double (&A)[N]) {
    double r = A[N - 1];
    for (size_t i = 1; i < N; ++i) {
        r *= x;
        r += A[N - 1 - i];
    }
    return r;
}

} // namespace

double phobos_atan(double x) {
    // Special cases. (trigonometry.d:785-789)
    if (x == 0.0) return x;
    if (std::isinf(x)) return std::copysign(kPI_2, x);

    // Make argument positive but save the sign. (trigonometry.d:791-795)
    bool sign = false;
    if (std::signbit(x)) {
        sign = true;
        x = -x;
    }

    // The double-precision branch (trigonometry.d:797-825).
    short flag = 0;
    double y;
    if (x > kTAN3_PI_8) {
        y = kPI_2;
        flag = 1;
        x = -(1.0 / x);
    } else if (x <= 0.66) {
        y = 0.0;
    } else {
        y = kPI_4;
        flag = 2;
        x = (x - 1.0) / (x + 1.0);
    }

    double z = x * x;
    z = z * poly(z, kP) / poly(z, kQ);
    z = x * z + x;
    if (flag == 2)
        z += 0.5 * kMOREBITS;
    else if (flag == 1)
        z += kMOREBITS;
    y = y + z;

    return sign ? -y : y;
}

double phobos_atan2(double y, double x) {
    // atan2Impl!double (trigonometry.d:990-1035), special cases first.
    if (std::isnan(x) || std::isnan(y)) return std::numeric_limits<double>::quiet_NaN();
    if (y == 0.0) {
        if (x >= 0 && !std::signbit(x))
            return std::copysign(0.0, y);
        else
            return std::copysign(kPI, y);
    }
    if (x == 0.0) return std::copysign(kPI_2, y);
    if (std::isinf(x)) {
        if (std::signbit(x)) {
            if (std::isinf(y))
                return std::copysign(3 * kPI_4, y);
            else
                return std::copysign(kPI, y);
        } else {
            if (std::isinf(y))
                return std::copysign(kPI_4, y);
            else
                return std::copysign(0.0, y);
        }
    }
    if (std::isinf(y)) return std::copysign(kPI_2, y);

    // Call atan and determine the quadrant.
    double z = phobos_atan(y / x);

    if (std::signbit(x)) {
        if (std::signbit(y))
            z = z - kPI;
        else
            z = z + kPI;
    }

    if (z == 0.0) return std::copysign(z, y);

    return z;
}

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
    loc.set(std::cos(theta) * cb_mag, std::sin(theta) * cb_mag, number(0.0));
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
