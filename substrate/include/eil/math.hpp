#pragma once
// eil/math.hpp — the numeric predicates whose exact truth values the solver
// branches on.
//
// nm/number.d:20-54 defines approxEqualNumbers entirely through
// std.math.isClose, and that function is what the port's tolerance gates call,
// so isClose has to agree with Phobos row-for-row rather than "usually".
// The 484-row truth table in tools/fmt_corpus.txt is what tests/test_format_corpus.cpp
// checks this against.

#include <cmath>
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

inline bool isNaN(number x) { return std::isnan(x); }
inline bool isInfinite(number x) { return std::isinf(x); }

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
inline bool isClose(number x, number y, number maxRelDiff = 1.0e-8,
                    number maxAbsDiff = 0.0) {
    if (x == y) return true;                       // so inf vs inf is close
    if (isInfinite(x) || isInfinite(y)) return false;
    if (isNaN(x) || isNaN(y)) return false;
    const number diff = std::fabs(x - y);
    if (diff <= maxAbsDiff) return true;
    const number scale = std::fmax(std::fabs(x), std::fabs(y));
    return diff <= maxRelDiff * scale;
}

} // namespace eil
