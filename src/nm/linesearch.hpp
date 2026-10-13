#pragma once
/**
 * linesearch.d
 *
 * Implementation of an algorithm for optimization from Gerald and Wheatley.
 *
 * A class demo for mech2700, originally in Python.
 * Author: PJ, 14-Jun-2014
*/
// nm/linesearch.d -> src/nm/linesearch.hpp (module nm.linesearch).
// D module scope -> namespace eil::nm::linesearch: nm/nelmin.d also declares
// `minimize`, and D keeps the two apart by module, so the port must too.

#include <type_traits>

#include "eil/number.hpp"

namespace eil::nm::linesearch {

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
 * Returns the bracket xL,xR containing the minimum of the function f.
 *
 * Params:
 *     f: a user supplied function
 *     a,b: the bracket containing a minimum
 *     tol: the final size of the bracket.
 *          It should not be set too small.
 */
template <auto f, typename T,
          typename = typename std::enable_if<detail::constraint<f>::value>::type>
void minimize(T& a, T& b, double tol = 1.0e-4)
{
    T r = 0.618034;
    T xL = a + (1-r)*(b-a);
    T xR = a + r*(b-a);
    T FL = f(xL);
    T FR = f(xR);
    while ((xR - xL) > tol) {
        if (FR > FL) {
            b = xR;
            xR = xL;
            FR = FL;
            xL = a + (1.0-r)*(b-a);
            FL = f(xL);
        } else {
            a = xL;
            xL = xR;
            FL = FR;
            xR = a + r*(b-a);
            FR = f(xR);
        }
    }
    a = xL;
    b = xR;
} // end minimize()

} // namespace eil::nm::linesearch
