// limiters.d

#include "nm/limiters.hpp"

#include <cmath>

namespace eil::nm {

void min_mod_limit(number& a, number& b)
// A rough slope limiter.
{
    if (a * b < 0.0) {
        a = 0.0;
        b = 0.0;
    } else {
        a = std::copysign(std::fmin(std::fabs(a), std::fabs(b)), a);
        b = a;
    }
}

void van_albada_limit(number& a, number& b)
// A smooth slope limiter.
{
    const double eps = 1.0e-12;
    number s = (a*b + std::fabs(a*b) + eps)/(a*a + b*b + eps);
    a *= s;
    b *= s;
}

number van_albada_limit1(number a, number b)
// A smooth slope limiter.
{
    const double eps = 1.0e-12;
    number s = (a*b + std::fabs(a*b) + eps)/(a*a + b*b + eps);
    return s;
}

number clip_to_limits(number q, number A, number B)
// Returns q if q is between the values A and B, else
// it returns the closer limit of the range [min(A,B), max(A,B)].
{
    const number lower_limit = (A <= B) ? A : B;
    const number upper_limit = (A > B) ? A : B;
    const number qclipped = (q > lower_limit) ? q : lower_limit;
    return (qclipped <= upper_limit) ? qclipped : upper_limit;
}

} // namespace eil::nm
