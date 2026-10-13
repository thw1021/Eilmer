#pragma once
// eil/math.hpp — the numeric predicates whose exact truth values the solver
// branches on.
//
// nm/number.d:20-54 defines approxEqualNumbers entirely through
// std.math.isClose, and that function is what the port's tolerance gates call,
// so isClose has to agree with Phobos row-for-row rather than "usually".
// The 484-row truth table in tools/fmt_corpus.txt is what tests/test_format_corpus.cpp
// checks this against.

#include <cstddef>
#include <cmath>
#include <limits>
#include <limits>

#include "eil/number.hpp"

namespace eil {

// Measured difference (corpus line "nan_math"): at run time Phobos' sqrt(-1) and
// log(-1) print "nan" while glibc prints "-nan" — the libm results differ in the
// NaN sign bit, even though both produce "-nan" for 0.0/0.0. A diverged solver run
// prints NaN into logs and restart files, so the port calls these wrappers rather
// than std::sqrt/std::log directly wherever a domain error is reachable.
inline number canonical_nan(number v) {
    if (std::isnan(v)) {
        static const number pos_nan = std::numeric_limits<double>::quiet_NaN();
        return pos_nan;
    }
    return v;
}

inline number sqrt(number x) { return canonical_nan(std::sqrt(x)); }
inline number log(number x) { return canonical_nan(std::log(x)); }
inline number log10(number x) { return canonical_nan(std::log10(x)); }
inline number log2(number x) { return canonical_nan(std::log2(x)); }
inline number asin(number x) { return canonical_nan(std::asin(x)); }
inline number acos(number x) { return canonical_nan(std::acos(x)); }

// -----------------------------------------------------------------------------
// eil::atan / eil::atan2 — Phobos' own arc tangent, not libm's.
//
// D's std.math.atan2(double,double) is NOT the C library's atan2:
// std/math/trigonometry.d:945 routes it to atan2Impl!double, which reduces the
// quadrant and then calls atanImpl!double (the CEPHES/fdlibm rational
// approximation Phobos keeps in pure source). glibc implements the same name with
// the IBM Accurate Mathematical Engine, and the two differ by 1 ULP on arguments
// near the axes. Measured on this machine (probes/geom/gpath/probe_mp_rot_chain.d
// -> baselines/e587b235/probes/geom_gpath_mp_rot_chain.txt, 324 rows of value plus
// raw bit pattern):
//   atan2(2.0, 1.9999996915748783)  D 0.78539824050373475  glibc 0.78539824050373463
//   atan2(2.0, 1.7071067811865475)  D 0.86424295012305097  glibc 0.86424295012305108
//
// A 1 ULP angle error is harmless on its own and catastrophic one operator later:
// where cos(theta) is near a zero crossing, 1 ULP of theta is ~2.2e-16 of *absolute*
// change in a cosine whose magnitude is 7.7e-08, i.e. a 2.9e-09 relative change, and
// Vector3.rotate_about_zaxis then multiplies it by r. That is how the 84 tolerance-
// band rows of geom_gpath_modifiedpath got their 8.88e-10 residual (T-910) - not a
// band that was scaled wrong, but a library substitution that was.
//
// Any D call site of `atan2`/`atan` therefore maps here. Sites found so far:
// geom/elements/vector3.d:523 (rotate_about_zaxis), geom/gpath/arc.d:92,
// geom/gpath/helix.d:59. `asin`/`acos` are left as glibc: measured bit-for-bit
// against Phobos over the ~1500 in-domain values of the gasdyn corpus, and D's own
// asin is defined as atan2(x, sqrt(1-x*x)) computed in 80-bit real, which
// src/gasdyn/idealgasflow.cpp:86 mirrors explicitly.
//
// Translated statement for statement from std/math/trigonometry.d: atan2Impl
// (990-1035) and the `realFormat == RealFormat.ieeeDouble` branch of atanImpl
// (694-846), with T = double.
// -----------------------------------------------------------------------------
namespace trig_detail {

// D's constants are 80-bit `real` and reach a `double` only through the casts the
// Phobos source writes, so the wide value is computed here and rounded on store
// (wave-02-brief section 5).
inline constexpr double kPI = static_cast<double>(EIL_PI);
inline constexpr double kPI_2 = static_cast<double>(EIL_PI / 2.0L);
inline constexpr double kPI_4 = static_cast<double>(EIL_PI / 4.0L);

// atanImpl!double coefficients (trigonometry.d:753-766). Written as long-double
// literals because that is how D spells them; the narrowing to double is D's too.
inline constexpr double kP[5] = {
    -6.485021904942025371773E1L, -1.228866684490136173410E2L, -7.500855792314704667340E1L,
    -1.615753718733365076637E1L, -8.750608600031904122785E-1L,
};
inline constexpr double kQ[6] = {
    1.945506571482613964425E2L,  4.853903996359136964868E2L, 4.328810604912902668951E2L,
    1.650270098316988542046E2L,  2.485846490142306297962E1L, 1.000000000000000000000E0L,
};
inline constexpr double kMOREBITS = static_cast<double>(6.123233995736765886130E-17L);
// tan(PI/8) and tan(3*PI/8), as D spells them (trigonometry.d:779-782).
inline constexpr double kTAN_PI_8 = static_cast<double>(0.414213562373095048801688724209698078569672L);
inline constexpr double kTAN3_PI_8 = static_cast<double>(2.414213562373095048801688724209698078569672L);

// std.math.algebraic.poly with the statically unrolled `ref const T[N]` overload
// (algebraic.d:557-566): Horner from the highest coefficient down.
template <size_t N> inline double poly(double x, const double (&A)[N]) {
    double r = A[N - 1];
    for (size_t i = 1; i < N; ++i) {
        r *= x;
        r += A[N - 1 - i];
    }
    return r;
}

} // namespace trig_detail

inline double atan(double x) {
    using namespace trig_detail;
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

inline double atan2(double y, double x) {
    using namespace trig_detail;
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
    double z = atan(y / x);

    if (std::signbit(x)) {
        if (std::signbit(y))
            z = z - kPI;
        else
            z = z + kPI;
    }

    if (z == 0.0) return std::copysign(z, y);

    return z;
}


inline bool isNaN(number x) { return std::isnan(x); }
inline bool isInfinite(number x) { return std::isinf(x); }

// D's `a ^^ b` is std.math.pow, and that is *not* the same bits as repeated
// multiplication: measured at -O0 with ldc2 1.40.0 (probes/substrate/pow.d),
// 1.2345^^3 = 1.8813659636249997 while 1.2345*1.2345*1.2345 = 1.8813659636249995,
// and 2.0/3.0^^3 differs the same way. 107 `^^` sites are in Wave 2 scope (89 of
// them `^^2`), mostly in Bezier/NURBS/Paver polynomial evaluation, so ports must
// write eil::pow(x, 3) rather than expanding the product. std::pow reproduced
// every measured D row bit-for-bit, including the integer-exponent overload
// (pow(x,2) == pow(x,2.0)), so the widening here is only about spelling.
inline number pow(number x, number y) { return canonical_nan(std::pow(x, y)); }
inline number pow(number x, int y) { return canonical_nan(std::pow(x, static_cast<number>(y))); }

// Determined from the boundary rows in tools/fmt_corpus.txt (D's own answers):
//
//   isClose(1e-5, 0, rel=1.0,  abs=0)   => true     isClose(1e-5, 0, rel=0.5,  abs=0) => false
//   isClose(1e-5, 0, rel=2.0,  abs=0)   => true     isClose(2.0,  1, rel=0.9999999999, abs=0) => true
//   isClose(3.0,  2, rel=0,    abs=1.0) => true     isClose(3.0,  2, rel=0, abs=0.9999999999) => false
//   isClose(1.0,  2, rel=1.0,  abs=0)   => true     isClose(2.0,  1, rel=1.0,  abs=0) => true
//
// A `diff/|y| <= rel` reading contradicts row 4 (ratio is 1.0, rel is 0.99999) and
// every y == 0 row, because it divides by zero. What fits all of them, including
// the symmetry of the last row, is
//
//     diff <= maxAbsDiff  ||  diff <= maxRelDiff * max(|x|, |y|)
//
// with `<=` on both terms (the abs rows at exact equality decide that).
// This matters: nm/number.d:approxEqualNumbers is defined through isClose, and
// the port's tolerance gates call it.
// D declares the tolerances as `real` and computes the difference in `real`, so
// the subtraction is not rounded the way a double subtraction would be. Match
// that width: number arguments are widened, and the comparison happens in
// extended precision.
inline bool isClose(number x, number y, real maxRelDiff = static_cast<real>(1.0e-8),
                    real maxAbsDiff = static_cast<real>(0.0)) {
    if (x == y) return true;                       // so inf vs inf is close
    if (isInfinite(x) || isInfinite(y)) return false;
    if (isNaN(x) || isNaN(y)) return false;
    const real diff = std::fabs(static_cast<real>(x) - static_cast<real>(y));
    if (diff <= maxAbsDiff) return true;
    const real scale = std::fmax(static_cast<real>(std::fabs(x)),
                                 static_cast<real>(std::fabs(y)));
    return diff <= maxRelDiff * scale;
}

} // namespace eil
