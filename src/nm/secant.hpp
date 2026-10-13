#pragma once
/**
 * secant.d
 *
 * Solve a nonlinear equation f(x)=0 using the secant method.
 *
 * Peter J.
 * 2020-09-27: D version adapted from Python3 version
 */
// nm/secant.d -> src/nm/secant.hpp (module nm.secant).
// D module scope -> namespace eil::nm::secant, since `solve` is shared with
// nm/brent.d, nm/ridder.d and nm/newton.d (see the note in brent.hpp).
//
// secant.d:43,46,47 use D's floating-point `.re` property as both an rvalue and an
// lvalue (a real's real part is the value itself). eil::re() covers the read side;
// assign_re() below covers the write side, which C++ cannot express as a member.

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <utility>

#include "eil/error.hpp"
#include "eil/number.hpp"

namespace eil::nm::secant {

namespace detail {
// D's template constraint
//   if (is(typeof(f(0.0)) == double) || is(typeof(f(0.0)) == float)
//       || is(typeof(f(Complex!double(0.0))) == Complex!double))
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

// `x.re = v` from D: for a scalar the whole value is the real part; for the
// Complex aggregate only the re field changes.
template <typename T> void assign_re(T& x, const T& v) { x = v; }
template <typename T> void assign_re(eil::Complex<T>& x, const T& v) { x.re = v; }
} // namespace detail

/**
 * The iterative secant method for zero-finding in one-dimension.
 *
 * Params:
 *   f: user-supplied function f(x)
 *   x0: first guess
 *   x1: second guess, presumably close to x0
 *   tol: stopping tolerance for f(x)=0
 *   max_iterations: to stop the iterations running forever, just in case...
 *
 * Returns:
 *   x such that f(x)=0
 */
template <auto f, typename T,
          typename = typename std::enable_if<detail::constraint<f>::value>::type>
T solve(T x0, T x1, double tol = 1.0e-11, double limit0 = 0.0, double limit1 = 0.0,
        int max_iterations = 1000)
{
    // We're going to arrange x0 as the oldest (furtherest) point
    // and x1 and the closer-to-the-solution point.
    // x2, when we compute it, will be the newest sample point.
    T f0 = f(x0);
    T f1 = f(x1);
    if (std::fabs(f0) < std::fabs(f1)) { std::swap(x0, x1); std::swap(f0, f1); }
    bool have_limits = !((limit0 == 0.0) && (limit1 == 0.0));
    for (int i = 0; i < max_iterations; ++i) {
        T df = f0 - f1;
        if (eil::re(df) == 0.0) {
            throw eil::ExceptionBase("Cannot proceed with zero slope.", __FILE__, __LINE__);
        }
        T x2 = x1 - f1 * (x0 - x1) / df;
        if (have_limits) {
            detail::assign_re(x2, std::max(limit0, eil::re(x2)));
            detail::assign_re(x2, std::min(limit1, eil::re(x2)));
        }
        T f2 = f(x2);
        x0 = x1; f0 = f1; x1 = x2; f1 = f2;
        if (std::fabs(f2) < tol) { return x2; }
    }
    throw eil::ExceptionBase("Did not converge after max iterations.", __FILE__, __LINE__);
} // end solve()

} // namespace eil::nm::secant
