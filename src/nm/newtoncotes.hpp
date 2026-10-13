#pragma once
/**
 * newtoncotes.d
 *
 * Adaptive quadrature using Newton-Cotes 5- and 3-point rules.
 *
 * Author: PA Jacobs, School of Mechanical and Mining Engineering, UQ
 * Version: 2014-06-20, Adapted from MECH2700 demonstration code written back in 2003.
 */
// nm/newtoncotes.d -> src/nm/newtoncotes.hpp (module nm.newtoncotes).
//
// Naming. namespace eil::nm::newtoncotes, following the convention in
// nm/brent.hpp: D keeps `nm.newtoncotes.integrate` apart from
// `nm.gaussquad.integrate` by module, so the port keeps them apart by
// namespace. A call site writes eil::nm::newtoncotes::integrate<fun,number>(a,b)
// where D writes integrate!(fun,number)(a,b).
//
// The callback. D's `alias f` binds the user function at compile time; it is not
// a delegate and carries no context. The C++17 equivalent is an `auto` non-type
// template parameter (the shape every earlier port in this cluster uses -- see
// nm/bracketing.hpp:56), so the body still says `f(x)` with no indirection and
// the calls inline exactly as ldc2 inlines them. The cost is that the bound
// entity must name a function with external linkage: the D unittest at
// newtoncotes.d:54-60 declares `number fun1(number x)` *inside* the unittest
// block, which a C++ template argument cannot do, so tests/test_nm_newtoncotes.cpp
// hoists fun1/fun2 to namespace scope with their bodies untouched. Nothing else
// in the algorithm depends on where the function is declared.
//
// Constraint. D's
//   if ( is(typeof(f(0.0)) == double) || is(typeof(f(0.0)) == float)
//        || is(typeof(f(Complex!double(0.0))) == Complex!double) )
// becomes the detection-idiom trait below (see the note in bracketing.hpp for
// why a bare decltype test in a default template argument is a hard error).
// The Complex branch is wave-12 work, as elsewhere: ntypes/complex.d's
// arithmetic is not ported, so only the double/float instantiations compile.
//
// Recursion. The split test and the two recursive calls are kept verbatim,
// including `tol/2.0` and the left-then-right order of the sum, because the
// number of rule evaluations -- and therefore the accumulated rounding --
// depends on it. Note that the rule is *not* guaranteed to terminate for very
// small tol: when a split leaves `mid == a` (adjacent doubles), the right-hand
// call re-integrates the same interval with tol/2, and once tol has halved down
// to 0.0 the test `abs(I4 - I2) > tol` can never fail. Measured on both sides of
// this port, on the sqrt kernel over [0.75, 1.0]: tol=1.0e-9 returns
// 0.11332793849437685 in both languages, while tol=1.0e-10 and tighter never
// terminate and die on memory exhaustion in D and in C++ alike. Reproducing that
// is the correct outcome for a strictly equivalent migration, not a bug to fix.
//
// The module body itself calls no transcendental: only +, -, *, / and fabs, so
// its results are pure IEEE and bit-reproducible; sqrt appears in the integrands
// the demos and tests supply. The float instantiation D permits is supported too
// -- integrate!(funf,float)(0.0f, 1.0f) gives 0x1.921fb4p-1 in both languages.

#include <cmath>
#include <type_traits>

#include "eil/number.hpp"

namespace eil::nm::newtoncotes {

namespace detail {
// D's template constraint via the detection idiom (see the note in
// bracketing.hpp). Only the return type is inspected, exactly as `typeof` does.
template <auto f, typename A, typename = void> struct call_result { using type = void; };
template <auto f, typename A>
struct call_result<f, A, std::void_t<decltype(f(A()))>> { using type = decltype(f(A())); };

template <auto f>
struct constraint : std::integral_constant<bool,
    std::is_same<typename call_result<f, double>::type, double>::value ||
    std::is_same<typename call_result<f, float>::type, float>::value ||
    std::is_same<typename call_result<f, eil::Complex<double>>::type,
                 eil::Complex<double>>::value> {};
} // namespace detail

/**
 * Apply Newton-Cotes 5- and 3-point quadrature rules to the segment [a,b].
 *
 * Params:
 *     f: user-supplied function, f(x)
 *     a, b: range of integration
 *     tol: maximum difference between rules, above which the range is split
 *
 * Returns:
 *     integral of f(x) from a to b.
 */
template <auto f, typename T,
          typename = typename std::enable_if<detail::constraint<f>::value>::type>
T integrate(T a, T b, double tol = 1.0e-5)
{
    T dx = b - a;
    T f0 = f(a);
    T f1 = f(a + 0.25 * dx);
    T f2 = f(a + 0.5 * dx);
    T f3 = f(a + 0.75 * dx);
    T f4 = f(b);
    T I2 = dx/6.0 * (f0 + 4 * f2 + f4);
    T I4 = dx/90.0 * (7*f0 + 32*f1 + 12*f2 + 32*f3 + 7*f4);
    T I;
    if (std::fabs(I4 - I2) > tol) {
        T mid = 0.5 * (a + b);
        I = integrate<f,T>(a, mid, tol/2.0) + integrate<f,T>(mid, b, tol/2.0);
    } else {
        I = I4;
    }
    return I;
}

} // namespace eil::nm::newtoncotes
