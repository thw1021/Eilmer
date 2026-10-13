#pragma once
#include <cfloat>
#include <limits>
// eil/number.hpp — the scalar type of the ported solver.
//
// Mirrors gdtk src/nm/number.d:11-15, which selects the scalar through a D
// version identifier rather than a numeric type:
//
//     version(complex_numbers) { immutable alias number = Complex!double; }
//     else                     { immutable alias number = double;         }
//
// Two facts measured on this machine, not assumed:
//   * D's 80-bit `real` is unused throughout src/eilmer (0 declarations), and
//     ldc2 here treats `real` as `double` — ldc2 rejects the literal 5e-324 as
//     "not representable", which only happens if real == double.
//   * Outside the complex_numbers branch, ntypes/complex.d:1926-1941 collapses
//     to a bare `struct Complex(T) { T re; T im; }`. That is all the trunk needs.
//
// The complex_numbers axis (560 version sites) is deferred to its own wave; it
// fails loudly here rather than silently porting doubles where Jacobian
// arithmetic expects a complex aggregate.

namespace eil {

// D's `real` is 80-bit extended on this toolchain, measured: sizeof(real)==16,
// real.mant_dig==64, and std.math.PI prints 0x1.921fb54442d1846ap+1 (18 hex
// mantissa digits). That matters even though src/eilmer declares no `real`
// variables: any expression touching a Phobos constant is computed in extended
// precision first and only rounded when stored, so `2.0*PI` is not `2.0*M_PI`.
// long double here is the same 80-bit format, so ports write eil::real and
// eil::PI and let the assignment do the rounding, exactly as D does.
using real = long double;
static_assert(sizeof(real) == 16, "expected 16-byte 80-bit long double");
static_assert(std::numeric_limits<real>::has_infinity, "long double must be IEEE");

inline constexpr real PI_v = 3.141592653589793238462643383279502884197L;
inline constexpr real E_v = 2.718281828459045235360287471352662497757L;
#define EIL_PI ::eil::PI_v
#define EIL_E ::eil::E_v

// Trunk build of ntypes/complex.d: the re/im carrier only, no arithmetic.
// Field names are load-bearing — generated accessors in celldata.d write
// `...re`, and solver text prints through them.
template <typename T> struct Complex {
    T re = T(0);
    T im = T(0);
};

#ifdef EIL_COMPLEX_NUMBERS
#error "EIL_COMPLEX_NUMBERS is wave 12 work: number becomes Complex<double> only once ntypes/complex.d is ported in full (see reports/phase-0.md)"
#else
using number = double;
#endif

// celldata.d's generated accessors spell every cell variable as `<path>.re`,
// because `number` may be Complex. With number == double there is no `.re`
// member, so the macros expand through this instead.
inline number re(number v) { return v; }
inline number im(number v) { return v; }
template <typename T> T re(Complex<T> v) { return v.re; }
template <typename T> T im(Complex<T> v) { return v.im; }

} // namespace eil
