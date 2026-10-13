#pragma once
// nm/limiters.d -> src/nm/limiters.hpp (module nm.limiters).
//
// D marks these `@nogc` with `pragma(inline, true)`; the definitions live in
// limiters.cpp exactly as in D, and inlining is the compiler's choice.

#include "nm/number.hpp"

namespace eil::nm {

void min_mod_limit(number& a, number& b);

void van_albada_limit(number& a, number& b);

number van_albada_limit1(number a, number b);

number clip_to_limits(number q, number A, number B);

} // namespace eil::nm
