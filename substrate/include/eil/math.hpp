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
#include <cstdint>
#include <cstring>
#include <limits>

#include "eil/number.hpp"

namespace eil {

// Measured NaN behaviour, now pinned by probes/substrate/probe_nan_sign.d (75 rows of
// "input bits -> output bits"). The first version of this block was wrong in two ways,
// both found while closing geom_surface_aopatch:
//
//   1. SCOPE. It canonicalised every NaN, including one that arrived as an *operand*.
//      Phobos propagates an operand's NaN with sign and payload intact
//      (sqrt(0x7ff8..) = 0x7ff8.., sqrt(0xfff8..) = 0xfff8.., sqrt(0x7ffc..) = 0x7ffc..)
//      and substitutes its own NaN only for a domain error on a finite operand. And that
//      substitute is not a single value: sqrt/log/log10/log2/pow give +nan where glibc
//      gives -nan, but asin/acos give -nan — the opposite polarity, which the old code
//      got backwards for those two. (fabs clears the sign and has no domain error, so it
//      stays unwrapped.)
//   2. Its own claim about polarity was itself wrong, and was repeated here until
//      someone measured it. std::numeric_limits<double>::quiet_NaN() on this machine is
//      0x7ff8000000000000 — sign bit CLEAR, measured with g++ 11.4 at -O0, -O2 and -O3,
//      and matched by ldc2's double.nan in probe_grid_nanadd.d. An earlier revision of
//      this comment asserted it was 0xfff8000000000000, which would have made the old
//      canonical_nan a second bug; it was not. The two polarities are still built from
//      explicit bit patterns below, because asin/acos need the negative one and
//      quiet_NaN() cannot give it. Distinguishing them by inspection is how the false
//      claim survived as long as it did.
inline number quiet_nan_positive() {
    const uint64_t u = 0x7ff8000000000000ull;
    number r;
    std::memcpy(&r, &u, sizeof r);
    return r;
}

inline number quiet_nan_negative() {
    const uint64_t u = 0xfff8000000000000ull;
    number r;
    std::memcpy(&r, &u, sizeof r);
    return r;
}

// Keep Phobos' two cases apart: a NaN operand passes through untouched; a NaN generated
// from a finite operand is replaced by `filler`, which is the only place the two
// libraries disagree.
inline number domain_nan(number x, number v, number filler) {
    if (std::isnan(x)) return x;
    return std::isnan(v) ? filler : v;
}
inline number domain_nan2(number x, number y, number v, number filler) {
    if (std::isnan(x)) return x;
    if (std::isnan(y)) return y;
    return std::isnan(v) ? filler : v;
}

// Retained for call sites that only need "is this a NaN, and print it Phobos' way"
// without an operand to consult. New code should prefer domain_nan.
inline number canonical_nan(number v) {
    return std::isnan(v) ? quiet_nan_positive() : v;
}

// -----------------------------------------------------------------------------
// Which NaN an operation returns when *both* operands are NaN.
//
// IEEE 754-2008 6.3 leaves this unspecified, and x86-64 SSE resolves it by a rule
// about register roles rather than about values: `addsd xmm_dst, xmm_src` returns
// xmm_dst's NaN, quieted (bit 51 set), with its sign and payload preserved. Which
// source value the compiler loads into that destination register is register
// allocation — so the answer depends on -O and on the surrounding code, in D as
// much as in C++. Measured over the 49-case operand matrix of
// probes/geom/grid/probe_grid_nanadd.d (ldc2 1.40.0 and g++ 11.4, both compiling
// geom/elements/vector3 in its own translation unit):
//
//   Vector3.add(other) / add(*other) / operator+=   ldc2 -O0: the ADDEND wins
//                                                   ldc2 -O1..-O3: the RECEIVER wins
//                                                   g++ -O0:  the ADDEND wins
//                                                   g++ -O1..-O3: the RECEIVER wins
//   Vector3.scale(f) / operator*=                   ldc2 -O0: the FACTOR wins
//                                                   ldc2 -O1..-O3: the FACTOR wins
//                                                   g++ -O0:  the RECEIVER wins
//                                                   g++ -O1..-O3: the FACTOR wins
//   Vector3 operator+ / operator- / operator-=      same choice at every level, and
//   operator/=                                      the two languages agree (the
//                                                   operands are already in registers,
//                                                   or the op is not commutative)
//   add(other, factor)                              stable in D, so its choice is the
//                                                   same one the pin writes; pinned here
//                                                   too because the add is the same
//                                                   instruction, not because it flips.
//
// The ldc2 -O1..-O3 line is the important one, and it is measured rather than assumed:
// running the same probe source through ldc2 at -O1, -O2 and -O3 and diffing against this
// -O0 capture loses 90, 103 and 103 rows respectively. Which groups move is exactly
// add.ref, add.ptr, opplus_eq, scale, opmul_eq and the walk rows that go through them;
// opplus, opminus, opminus_eq, opdiv, opdiv_eq and add.fac do not move at all. The
// REFERENCE therefore has no fixed answer for the first set either — had the baseline been
// captured at -O1, geom_grid_grid row 178 would read `x=nan' and the unpinned Release
// build would have been the correct port. So there is no value-level rule to discover
// here, only a level to contract to.
//
// Note that `add(other)` wants the addend while the temporary form `operator+`
// wants the receiver, both at the same -O0: that is the proof that no value-level
// rule is being expressed here. It is also why row 178 of geom_grid_grid was the
// only red row in the whole suite and only in Release — `Vector3 cbc;` starts NaN,
// `cbc.set(v0)` installs v0's NaN, and `cbc.add(v1)` sums two NaNs of opposite
// sign, so the printed text flips between "-nan" and "nan".
//
// Every baseline under baselines/e587b235 is an ldc2 -O0 capture (the build line at
// the top of probes/geom/grid/probe_grid.d has no -O flag), so the -O0 choice is the
// contract. Writing it out here instead of leaving it to the register allocator makes
// the port agree with that capture at -O0, -O1, -O2 and -O3, and removes the
// Debug-vs-Release split the same artifact had created for scale(). Only the
// both-NaN case is pinned: with one NaN operand the hardware propagates that operand
// whatever the order, so every finite, zero, infinity and single-NaN input still
// goes through the plain `self + addend` below, bit for bit.
inline number quiet_copy(number v) {
    uint64_t u;
    std::memcpy(&u, &v, sizeof u);
    u |= (uint64_t{1} << 51);  // sNaN -> QNaN: SSE sets the quiet bit, keeps payload
    number r;
    std::memcpy(&r, &u, sizeof r);
    return r;
}

// D's `x += other.x` at ldc2 -O0: the addend's NaN propagates.
inline number add_rhs_nan(number self, number addend) {
    if (std::isnan(self) && std::isnan(addend)) return quiet_copy(addend);
    return self + addend;
}

// D's `x *= factor` at ldc2 -O0: the factor's NaN propagates.
inline number mul_rhs_nan(number self, number factor) {
    if (std::isnan(self) && std::isnan(factor)) return quiet_copy(factor);
    return self * factor;
}

inline number sqrt(number x) { return domain_nan(x, std::sqrt(x), quiet_nan_positive()); }
inline number log(number x) { return domain_nan(x, std::log(x), quiet_nan_positive()); }
inline number log10(number x) { return domain_nan(x, std::log10(x), quiet_nan_positive()); }
inline number log2(number x) {
    return domain_nan(x, std::log2(x), quiet_nan_positive());
}

// -----------------------------------------------------------------------------
// eil::asin / eil::acos — Phobos' definition, not libm's entry point.
//
// Phobos writes them through atan2 and forms them in 80-bit `real`:
//     acos(x) = atan2(sqrt(1 - x*x), x)      asin(x) = atan2(x, sqrt(1 - x*x))
// glibc's std::acos/std::asin are other functions, and they agree with the above only in
// the middle of the domain. Measured against ldc2 1.40.0 over 8,492 arguments
// (probes/substrate/probe_acos_one.d -> baselines/e587b235/probes/substrate_acos_one.txt:
// a +-2^-53..2^-53*61 ladder from both endpoints, the exact endpoints and one ULP past
// them, 4,001 uniform samples across [-1,1], the cosine-rule arguments paver produces,
// and the specials 0/-0/+-1/+-inf/+-nan/+-min_normal, each also stepped by 2^-30):
//   * the formula reproduces D on 8,492/8,492 rows;
//   * std::acos/std::asin miss 120 of them, and 118 of those 120 are arguments within
//     2^-30 of +-1 or outside the domain -- where acos has its square-root singularity,
//     so a difference in method is a whole ULP of *result*.
// geom/grid/paver.d:579 computes acos(r) from the cosine rule and two nodes of the
// paver_demo.d boundary land in that band; that is backlog T-931. The earlier
// "measured bit-for-bit, so leave it glibc" reading (T-911, 11 samples; T-912) never put
// an argument near an endpoint.
//
// NaN polarity is part of the contract and is NOT libm's: D gives a NEGATIVE quiet NaN
// both for a finite argument outside [-1,1] and for +-inf (rows acosspec/asin* at
// 7ff0000000000000 -> fff8000000000000), and a NaN operand propagates its own payload.
inline number asin(number x) {
    if (std::isnan(x)) return x;
    const real r = static_cast<real>(x);
    const real s = std::sqrt(static_cast<real>(1.0) - r * r);
    number v = static_cast<number>(std::atan2(r, s));
    return std::isnan(v) ? quiet_nan_negative() : v;
}
inline number acos(number x) {
    if (std::isnan(x)) return x;
    const real r = static_cast<real>(x);
    const real s = std::sqrt(static_cast<real>(1.0) - r * r);
    number v = static_cast<number>(std::atan2(s, r));
    return std::isnan(v) ? quiet_nan_negative() : v;
}

// -----------------------------------------------------------------------------
// eil::sin / eil::cos — keep a cos(x)+sin(x) pair as TWO libm calls.
//
// Both compilers fuse them. g++ at -O1 and above turns `std::cos(x)` and `std::sin(x)`
// of one argument into a single glibc `sincos()` call; LDC lowers Phobos'
// std.math.cos/std.math.sin to llvm.cos.f64/llvm.sin.f64 and LLVM's own sincos pass
// fuses them again from -O1 up. `sincos` is not the same function as the two separate
// entries and differs from them by 1 ULP on roughly 1 component in 1466 (scan: 6e7
// angles over 0..2pi).
//
// Classified against the 415 angles of tests/test_data/sincos_angle.txt (see
// probes/substrate/probe_sin_cos_pair.d), by which form reproduces BOTH bit patterns:
//   ldc2 -O0   separate-only 401   sincos-only 0     both 14
//   ldc2 -O3   separate-only   0   sincos-only 401   both 14
//   g++  -O0   separate            g++ -O1..-O3      fused
// So 401 of 415 rows move inside the reference itself as its -O changes. Every baseline
// in this repository is an ldc2 -O0 capture, which makes the separate form the contract
// -- the same reasoning as the both-NaN pin above (T-919) and for the same reason: a
// port must not inherit its answer from the optimiser.
//
// 1 ULP of angle is not end-of-line noise. T-910 measured that near a zero crossing of
// cos(theta), 1 ULP of theta is a ~2.9e-09 RELATIVE change in a cosine of magnitude
// 7.7e-08, and the geometry code multiplies that by a radius into a coordinate. The
// wedgevolume stage found the same thing the hard way: 20 of its 4594 corpus rows went
// red at -O3 before its local guard (src/geom/volume/wedgevolume.cpp:49-53, now just a
// caller of these two).
//
// The volatile copy is the measured way to stop the fusion: a TU whose only sin/cos use
// goes through it has `U sin`, `U cos` and NO `U sincos` in nm -u at -O3, and its values
// equal ldc2 -O0's. Sites routed here: geom/elements/vector3.cpp (rotate_about_zaxis,
// rotate2d), geom/elements/projection.cpp:78, geom/grid/grid.cpp:533-534,
// geom/gpath/arc.cpp:119, geom/gpath/helix.cpp:70, geom/volume/wedgevolume.cpp, and every
// sin/cos pair in gasdyn/idealgasflow.cpp. Guarded by ctest:substrate_sin_cos_pair.
inline number sin(number x) { volatile number a = x; return std::sin(a); }
inline number cos(number x) { volatile number a = x; return std::cos(a); }

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
// geom/gpath/helix.d:59. `asin`/`acos` are NOT libm either -- see the block above them;
// the gasdyn sites that used to carry a local d_asin replica now call eil::asin.
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

// D's `a ^^ b` is std.math.pow, and it is *not* the same bits as repeated multiplication
// -- except at exponent 2, where it is exactly one multiply. Measured at -O0 with ldc2
// 1.40.0 over 2222 operands (i/7, i*0.001, 1/i, i*1e-8 and 0.05+i*1e-4 for i=1..400, a
// 200-step walk from 0.040262832185305138, plus 0, -0, ±1, ±2, 0.5, 1/3, 2/3,
// ±min_normal, ±5e-324, ±double.max, both sides of the squaring overflow boundary,
// ±inf, nan):
//   * `x ^^ 2` == `x*x` on 2222/2222 bit patterns (and == Phobos `pow(x,2.0)` on
//     2222/2222, so D's two spellings of exponent 2 agree with each other);
//   * `x ^^ 3` == Phobos `pow(x,3.0)` on 2222/2222, but differs from `(x*x)*x` on
//     527/2222 (23.7%) -- so exponents 3 and up stay pow and must NOT be expanded;
//   * glibc `pow(x,2.0)` -- which is what a port `eil::pow(x,2)` call becomes -- differs from
//     D's `x ^^ 2` on 3 of the first 2002 operands, always by 1 ULP.
// That last line is backlog T-931's root cause: geom/grid/paver.d:202 computed `b` one
// bit away from the reference, and the demo's paved_grid.vtk lost 398 of 1490 vertices in
// the last digit. So: `x ^^ 2` is eil::sqr(x), `x ^^ 0.5` is eil::sqrt(x), everything
// else is eil::pow(x, e). Rows sq*/cu* of the substrate_pow corpus pin it.
//
// The rule was previously written the other way round, on the strength of a 7-operand
// sample in which pow and multiply happened to agree on every operand. 0.15% divergence
// needs a few hundred operands to show up, and 7 is not a few hundred.
//
// Residue this does NOT remove: Phobos' pow and glibc's pow disagree by 1 ULP on roughly
// 1 operand in 650 for exponents 3 and 4 as well (3/2002 and 2/2002 measured). Spelling
// cannot fix that; only a replica of powImpl could, and backlog T-911 decided not to
// build replicas for 1-ULP classes. The census rows are in substrate_phobos_bits.
//
// ONE EXCEPTION, and it is not about pow: exponent 0.5. D's `x ^^ 0.5` is sqrt(x),
// not pow(x, 0.5) -- measured over 765 non-negative operands, `x^^0.5` matches sqrt(x)
// on all 765 bit patterns while pow(x,0.5) differs on one of them, x = -0.0, where
// sqrt preserves the sign of zero and pow returns +0.0 (backlog T-925; the rows are in
// the substrate_pow corpus, and mapping `^^0.5' onto this function turns exactly that row
// red). A signed zero is not cosmetic: it decides the sign of every later division by it.
// So transcribe `(...)^^0.5' as eil::sqrt(...), and leave this function for the other
// exponents. geom/grid/paver.cpp:171 and :202 are the sites this came from.
inline number pow(number x, number y) {
    return domain_nan2(x, y, std::pow(x, y), quiet_nan_positive());
}
inline number pow(number x, int y) { return pow(x, static_cast<number>(y)); }

// D's `x ^^ 2` (and `pow(x,2)` / `pow(x,2.0)`): one hardware multiply, bit-for-bit.
// Do not route exponent 2 through eil::pow -- see the measurement above.
inline number sqr(number x) { return x * x; }

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
