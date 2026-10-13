#pragma once
/**
 * Bracket a root of f(x).
 *
 * Params:
 *    f: user-supplied function f(x)
 *    x1: first end of range
 *    x2: other end of range
 *  x1min: limits the value of x1 so that it does not become lower than a certain value
 *
 * Returns:
 *    0: successfully bracketed a root
 *   -1: failed to bracket a root
 *
 * On return (x1, x2) should bracketing the root.
 */
// nm/bracketing.d -> src/nm/bracketing.hpp (module nm.bracketing).
//
// D's `bracket(alias f, T)` binds the user function at compile time, so it is
// translated as a C++17 auto non-type template parameter: the ported call site
// `bracket<zeroFun, number>(T1, T2)` mirrors `bracket!(zeroFun,number)(T1,T2)`,
// with f taking no runtime slot and the body calling `f(x1)` unchanged.
// One adaptation: D permits a closure/delegate *variable* as the alias argument
// (bracketing.d:80 does); a C++ non-type template argument cannot name a local,
// so the ported test/demo bodies express that case as a named function over a
// file-scope variable, which is what brent.d:140 and newton.d:164 already do.

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <utility>

#include "eil/error.hpp"
#include "eil/number.hpp"

namespace eil::nm {

namespace bracketing_detail {
// The D template constraint
//   if (is(typeof(f(0.0)) == double) || is(typeof(f(0.0)) == float)
//       || is(typeof(f(Complex!double(0.0))) == Complex!double))
// through the C++17 detection idiom, because a bare decltype test inside a
// default template argument is a hard error rather than a substitution failure.
template <auto f, typename A, typename = void> struct call_result { using type = void; };
template <auto f, typename A>
struct call_result<f, A, std::void_t<decltype(f(A()))>> { using type = decltype(f(A())); };

template <auto f>
struct constraint : std::integral_constant<bool,
    std::is_same<typename call_result<f, double>::type, double>::value ||
    std::is_same<typename call_result<f, float>::type, float>::value ||
    std::is_same<typename call_result<f, eil::Complex<double>>::type,
                 eil::Complex<double>>::value> {};
} // namespace bracketing_detail

template <auto f, typename T,
          typename = typename std::enable_if<bracketing_detail::constraint<f>::value>::type>
int bracket(T& x1, T& x2, T x1_min = -1.0e38, T x2_max = +1.0e38,
            int max_try = 50, T factor = 1.6)
{
    if (x1 == x2) {
        throw eil::ExceptionBase("Bad initial range given to bracket.", __FILE__, __LINE__);
    }
    // We assume that x1 < x2.
    if (x1 > x2) { std::swap(x1, x2); }
    if (x1_min > x2_max) { std::swap(x1_min, x2_max); }
    T f1 = f(x1);
    T f2 = f(x2);
    for (int i = 0; i < max_try; ++i) {
        if (f1*f2 < 0.0) { return 0; } // we have success
        if (std::fabs(f1) < std::fabs(f2)) {
            x1 += factor * (x1 - x2);
            //prevent the bracket from being expanded beyond a specified domain
            x1 = std::fmax(x1_min, x1);
            try {
                f1 = f(x1);
            } catch (const eil::ExceptionBase& e) {
                // Presume that we have gone into an invalid region,
                // so reset to the boundary.
                // debug { writeln("bracket trying x1 e.msg=", e.msg); }
                (void)e;
                x1 = x1_min;
                f1 = f(x1);
            }
        } else {
            x2 += factor * (x2 - x1);
            x2 = std::fmin(x2_max, x2);
            try {
                f2 = f(x2);
            } catch (const eil::ExceptionBase& e) {
                // Presume that we have gone into an invalid region,
                // so reset to the boundary.
                // debug { writeln("bracket trying x2 e.msg=", e.msg); }
                (void)e;
                x2 = x2_max;
                f2 = f(x2);
            }
        }
    }
    // If we leave the loop here, we were unsuccessful.
    return -1;
} // end bracket()

} // namespace eil::nm
