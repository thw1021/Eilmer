#pragma once
/**
 * ridder.d
 *
 * Solve a nonlinear equation f(x)=0 using the method of Ridder.
 *
 * Peter J.
 * mech3750 demo code 12-Mar-2014
 * added bracketing 19-Mar-2014
 * D version 13-Jun-2014
 */
// nm/ridder.d -> src/nm/ridder.hpp (module nm.ridder).
// D module scope -> namespace eil::nm::ridder: `solve` is the name used by
// nm/brent.d, nm/secant.d and nm/newton.d too, which D separates by module (see the
// note in brent.hpp).

#include <cmath>
#include <type_traits>

#include "eil/error.hpp"
#include "eil/math.hpp"
#include "eil/number.hpp"

namespace eil::nm::ridder {

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
} // namespace detail

/**
 * Locate a root of f(x) by subdividing the original range,
 * assuming a linear model of the underlying transformed function.
 *
 * Params:
 *    f: user-supplied function f(x)
 *    x1: first end of range
 *    x2: other end of range
 *    tol: minimum size for range
 *
 * Returns:
 *    x, a point near the root.
 */
template <auto f, typename T,
          typename = typename std::enable_if<detail::constraint<f>::value>::type>
T solve(T x1, T x2, double tol = 1.0e-9)
{
    T x3 = T();
    T f3 = T();
    T x4 = x1; // So that g++ doesn't warn on maybe unitialized.
    T f4 = T();
    // D's default initializer for a floating local is NaN; C++ needs the explicit
    // value-initialization above, and every one of these is assigned before it is
    // read, so the difference is not observable.
    T eps = T();

    T f1 = f(x1);
    T f2 = f(x2);
    if ( std::fabs(f1) == 0.0 ) return x1;
    if ( std::fabs(f2) == 0.0 ) return x2;
    if ( x1 == x2 ) {
        throw eil::ExceptionBase("Bad initial range given to bracket.", __FILE__, __LINE__);
    }
    if ( f1 * f2 > 0.0 ) {
        throw eil::ExceptionBase("Range does not clearly bracket a root.", __FILE__, __LINE__);
    }
    while ( std::fabs(x2 - x1)/(1.0+std::fabs(x1)+std::fabs(x2)) > tol ) {
        x3 = 0.5*(x1+x2);
        f3 = f(x3);
        if ( f3 == 0.0 ) return x3;
        eps = (f3 + std::copysign(eil::sqrt(f3*f3-f1*f2),f2))/f2;
        x4 = x3 - f3*eps*(x1-x3)/(f1 - eps*f3);
        f4 = f(x4);
        if ( f4 == 0.0 ) return x4;
        // Contract the bracket.
        if ( f3*f2 < 0.0 ) {
            if ( f4*f2 < 0.0 ) {
                x1 = x4; f1 = f4;
            } else {
                x1 = x3; f1 = f3;
                x2 = x4; f2 = f4;
            }
        } else {
            if ( f4*f1 < 0.0 ) {
                x2 = x4; f2 = f4;
            } else {
                x1 = x4; f1 = f4;
                x2 = x3; f2 = f3;
            }
        }
    } // end while
    return x4;
} // end solve()

} // namespace eil::nm::ridder
