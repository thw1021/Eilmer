#pragma once
/**
 * gaussquad.d
 *
 * Adaptive integration of a function using Gauss quadrature rules.
 *
 * Author: Peter J.
 * Version: 2014-06-21: adapted from newtoncotes.d
 */
// nm/gaussquad.d -> src/nm/gaussquad.hpp (module nm.gaussquad).
//
// Naming. namespace eil::nm::gaussquad, because `integrate` is also declared by
// nm/newtoncotes.d and D separates the two by module (see the note in
// nm/brent.hpp).
//
// The four rule tables (gaussquad.d:17-27) are D *module-level mutable dynamic
// arrays*, not constants: integrate passes them to apply_gauss_rule implicitly,
// and the header comment invites a caller to supply its own xs/ws. They are
// ported as C++17 `inline eil::Array<double>` variables so that (a) one object
// is shared by all translation units, as the D module global is, (b) they keep
// D's `.length` member, which apply_gauss_rule reads through the slice, and
// (c) they stay mutable. The literal values are copied verbatim, digit for
// digit, from the D source: they are the load-bearing part of this file.
//
// The callback. As in nm/newtoncotes.hpp, D's `alias f` is a compile-time bound
// function with no context, so it becomes an `auto` non-type template parameter
// and the body's `f(x)` is unchanged. The D unittest declares fun1/fun2 inside
// the unittest block (gaussquad.d:88-89), which a template argument cannot name
// in C++, so tests/test_nm_gaussquad.cpp hoists them to namespace scope.
//
// Summation order. apply_gauss_rule accumulates `result += ws[i] * f(x)` in the
// order the nodes are stored, and the trailing `xrange2 * result` scales once at
// the end. That is reproduced exactly -- no pairwise sum, no hoisting of the
// scale factor into the loop, no reordering of the 3-point and 4-point rule
// calls inside integrate -- because the difference between the two rules, which
// drives the split test, is a small difference between two large sums.
//
// Nothing in this module body calls libm: only +, -, *, / and fabs, so its
// results are pure IEEE. Unlike newtoncotes, its split test does terminate even
// at tol=1.0e-14 on the sqrt endpoint segment (measured equal in both languages).
// The float instantiation D permits is supported: integrate!(funf,float)(0,1)
// gives 0x1.921fb6p-1 from both.

#include <cmath>
#include <cstddef>
#include <type_traits>

#include "eil/array.hpp"
#include "eil/number.hpp"

namespace eil::nm::gaussquad {

// Rule coefficients computed by Maxima.
// See the scripts in directory ./notes

inline eil::Array<double> xs3 = {-7.7459666924148337704e-1,
                                 0.0,
                                 7.7459666924148337704e-1};
inline eil::Array<double> ws3 = {5.5555555555555555556e-1,
                                 8.8888888888888888889e-1,
                                 5.5555555555555555556e-1};

inline eil::Array<double> xs4 = {-8.6113631159405257523e-1, -3.399810435848562648e-1,
                                 3.399810435848562648e-1, 8.6113631159405257523e-1};
inline eil::Array<double> ws4 = {3.4785484513745385738e-1, 6.5214515486254614263e-1,
                                 6.5214515486254614263e-1, 3.4785484513745385738e-1};

namespace detail {
// D's template constraint
//   if ( is(typeof(f(0.0)) == double) || is(typeof(f(0.0)) == float)
//        || is(typeof(f(Complex!double(0.0))) == Complex!double) )
// via the detection idiom (see the note in bracketing.hpp).
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
 * Adaptively apply Gauss 4- and 3-point quadrature rules to the segment [a,b].
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
T apply_gauss_rule(T a, T b, eil::Slice<double> xs, eil::Slice<double> ws)
{
    T xmid = 0.5 * (b + a);
    T xrange2 = 0.5 * (b - a);
    size_t N = xs.length();
    T result = 0.0;
    for (size_t i = 0; i < N; ++i) {
        T x = xmid + xs[i] * xrange2; // transform from range -1.0 +1.0
        result += ws[i] * f(x);
    }
    return xrange2 * result;
}

/**
 * Adaptively apply Gauss 4- and 3-point quadrature rules to the segment [a,b].
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
    T I3 = apply_gauss_rule<f,T>(a, b, xs3.all(), ws3.all());
    T I4 = apply_gauss_rule<f,T>(a, b, xs4.all(), ws4.all());
    T I;
    if ( std::fabs(I4 - I3) > tol ) {
        T mid = 0.5 * (a + b);
        I = integrate<f,T>(a, mid, tol/2.0) + integrate<f,T>(mid, b, tol/2.0);
    } else {
        I = I4;
    }
    return I;
}

} // namespace eil::nm::gaussquad
