#pragma once
/**
 number.d
 Specifies the type of numbers to use in the numerical methods.
*/
// nm/number.d -> src/nm/number.hpp (module nm.number).
// Header only, as nm/number.d has no unittest blocks and no non-inline code.

#include <cstddef>
#include <string>

#include "eil/array.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/math.hpp"
#include "eil/number.hpp"

namespace eil::nm {

#ifdef complex_numbers
// number.d:11-12 `version(complex_numbers) { immutable alias number = Complex!double; }`
// needs full ntypes/complex.d port (wave 12)
using number = eil::Complex<double>;
#else
// number.d:13-14 `else { immutable alias number = double; }`
using number = eil::number;
#endif

/**
 * Returns true if both components are approximately equal.
 */
inline bool approxEqualNumbers(const eil::Complex<double>& v1,
                               const eil::Complex<double>& v2,
                               double maxRelDiff = 1.0e-2, double maxAbsDiff = 1.0e-5) {
    return (eil::isClose(v1.re, v2.re, maxRelDiff, maxAbsDiff) &&
            eil::isClose(v1.im, v2.im, maxRelDiff, maxAbsDiff));
}

inline bool approxEqualNumbers(eil::Slice<eil::Complex<double>> v1,
                               eil::Slice<eil::Complex<double>> v2,
                               double maxRelDiff = 1.0e-2, double maxAbsDiff = 1.0e-5) {
    if (v1.length() != v2.length()) return false;
    for (size_t i = 0; i < v1.length(); ++i) {
        if (!eil::isClose(v1[i].re, v2[i].re, maxRelDiff, maxAbsDiff) ||
            !eil::isClose(v1[i].im, v2[i].im, maxRelDiff, maxAbsDiff)) return false;
    }
    return true;
}

inline bool approxEqualNumbers(const double v1, const double v2,
                               double maxRelDiff = 1.0e-2, double maxAbsDiff = 1.0e-5) {
    return eil::isClose(v1, v2, maxRelDiff, maxAbsDiff);
}

inline bool approxEqualNumbers(eil::Slice<double> v1, eil::Slice<double> v2,
                               double maxRelDiff = 1.0e-2, double maxAbsDiff = 1.0e-5) {
    if (v1.length() != v2.length()) return false;
    for (size_t i = 0; i < v1.length(); ++i) {
        if (!eil::isClose(v1[i], v2[i], maxRelDiff, maxAbsDiff)) return false;
    }
    return true;
}

} // namespace eil::nm
