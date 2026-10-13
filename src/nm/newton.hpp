#pragma once
/**
3 * Solve a nonlinear equation f(x)=0 using Newton's method.
 *
 * Caller needs to supply f(x) and f'(x).
 *
 * Authors: Rowan G. and Peter J.
 * Version: 2019-12-05, first cut
 */
// nm/newton.d -> src/nm/newton.hpp (module nm.newton).
// D module scope -> namespace eil::nm::newton, since `solve` is shared with
// nm/brent.d, nm/ridder.d and nm/secant.d (see the note in brent.hpp).

#include <cmath>
#include <string>
#include <type_traits>

#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/number.hpp"
#include "nm/nm_exception.hpp"

namespace eil::nm::newton {

namespace detail {
// D's template constraint
//   if ( (is(typeof(f(0.0)) == double) && is(typeof(dfdx(0.0)) == double)) ||
//        (is(typeof(f(0.0)) == float) && is(typeof(dfdx(0.0)) == float)) ||
//        (is(typeof(f(Complex!double(0.0))) == Complex!double) &&
//         is(typeof(dfdx(Complex!double(0.0))) == Complex!double)) )
// via the detection idiom (see the note in bracketing.hpp).
template <auto f, typename A, typename = void> struct call_result { using type = void; };
template <auto f, typename A>
struct call_result<f, A, std::void_t<decltype(f(A()))>> { using type = decltype(f(A())); };

template <auto f, auto dfdx>
struct constraint : std::integral_constant<bool,
    (std::is_same<typename call_result<f, double>::type, double>::value &&
     std::is_same<typename call_result<dfdx, double>::type, double>::value) ||
    (std::is_same<typename call_result<f, float>::type, float>::value &&
     std::is_same<typename call_result<dfdx, float>::type, float>::value) ||
    (std::is_same<typename call_result<f, eil::Complex<double>>::type,
                  eil::Complex<double>>::value &&
     std::is_same<typename call_result<dfdx, eil::Complex<double>>::type,
                  eil::Complex<double>>::value)> {};
} // namespace detail

/**
 * Locate a root of f(x) using Newton's method.
 *
 * This algorithm is a variation on the classic Newton
 * method in that it has some safeguards if the method
 * wants to wander into territory beyond some bounds.
 * Here we denote those bounds as xMin and xMax.
 *
 * This is an implementation of the function 'rtsafe'
 * that appears in Section 9.4 of Press et al.
 * Note I have added a modification such that the user
 * supplies an initial guess, whereas the original
 * suggestion of Press et al. is to use:
 *    guess = 0.5*(xMin + xMax);
 *
 * Reference:
 * Press, Teukolsky, Vetterling, Flannery (1992)
 * Numerical Recipes in C, The Art of Scientific Computing, 2nd ed.
 * Cambridge University Press
 */

#ifdef complex_numbers
// newton.d:40 `version(complex_numbers)`
// For the complex numbers version of the code we need
// a Newton's method with a fixed number of iterations.
// An explanation can be found in:
//     Efficient Construction of Discrete Adjoint Operators on Unstructured Grids
//     by Using Complex Variables, pg. 10, Nielsen et al., AIAA Journal, 2006.
// TODO: add in safeguards from the real number version.
// needs full ntypes/complex.d port (wave 12): the divide, `-=` and `T dx = 0.0`
// below are the ntypes/complex.d operators, which today's Complex<T> (re/im
// fields only) does not provide. This branch is not compiled in the trunk build.
template <auto f, auto dfdx, typename T,
          typename = typename std::enable_if<detail::constraint<f, dfdx>::value>::type>
T solve(T x0, T xMin, T xMax, double tol = 1.0e-9)
{
    const int MAXIT = 10; // maximum number of iterations
    T rts = x0;
    T dx = 0.0;
    T f0 = f(rts);
    T df0 = dfdx(rts);

    for (int j = 0; j < MAXIT; ++j) {
        dx = f0/df0;
        rts -= dx;
        f0 = f(rts);
        df0 = dfdx(rts);
    }

    return rts;
}
#else
template <auto f, auto dfdx, typename T,
          typename = typename std::enable_if<detail::constraint<f, dfdx>::value>::type>
T solve(T x0, T xMin, T xMax, double tol = 1.0e-9)
{
    const int MAXIT = 30; // maximum number of iterations
    T xL = xMin;
    T xH = xMax;
    T fL = f(xL);
    T fH = f(xH);
    if ( (fL > 0.0 && fH > 0.0) || (fL < 0.0 && fH < 0.0) ) {
        std::string msg = "Root must be bracketed in call to nm.newton.solve";
        EIL_THROW_NM(msg);
    }

    if (fL == 0.0) return xMin;
    if (fH == 0.0) return xMax;

    // Orient the search so that that f(xL) < 0.0
    if (fL < 0.0) {
        xL = xMin;
        xH = xMax;
    }
    else {
        xH = xMin;
        xL = xMax;
    }

    T rts = x0;
    T dxold = (xMax - xMin);
    T dx = dxold;
    T f0 = f(rts);
    T df0 = dfdx(rts);
    for (int j = 0; j < MAXIT; ++j) {
        // Bisect if Newton prediction is out of range.
        if ( (((rts-xH)*df0 - f0)*((rts-xL)*df0 - f0) > 0.0) ||
             (std::fabs(2.0*f0) > std::fabs(dxold*df0)) ) {
            dxold = dx;
            dx = 0.5*(xH - xL);
            rts = xL + dx;
            // Check if change in root is negligible.
            // Accept it, if it is.
            if (xL == rts) return rts;
        }
        else { // A Newton step is acceptable
            dxold = dx;
            dx = f0/df0;
            T tmp = rts;
            rts -= dx;
            if (tmp == rts) return rts;
        }
        if (std::fabs(dx) < tol) return rts;
        // Otherwise, re-evaluate for next iteration
        f0 = f(rts);
        df0 = dfdx(rts);
        if (f0 < 0.0) {
            xL = rts;
        }
        else {
            xH = rts;
        }
    }
    // When successful, we should never reach here.
    std::string msg = "Newton method failed to converge";
    // D's `debug { msg ~= format(" in %d iterations.", MAXIT); }`: debug code is
    // live in the reference (non -release) build, so it is kept unconditionally.
    msg += eil::format(" in %d iterations.", MAXIT);
    EIL_THROW_NM(msg);
}
#endif

} // namespace eil::nm::newton
