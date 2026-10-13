#pragma once
/**
 * brent.d
 *
 * Solve a nonlinear equation f(x)=0 using the method of Brent.
 * ref:pg_454_numerical_recipes_3rd[PRESS, 2007]
 *
 * Kyle D.
 * D version 16-Oct-2015
 * Bracketing taken from ridder.d
 */
// nm/brent.d -> src/nm/brent.hpp (module nm.brent).
// Template, so the definition lives in the header; see brent.cpp for the TU note.
// The D module scope is namespace eil::nm::brent, because `solve` is also the name
// used by nm/ridder.d, nm/secant.d, nm/newton.d (and nm/bbla.d, nm/smla.d), which D
// keeps apart by module. A ported call site writes eil::nm::brent::solve<...>()
// where D writes nm.brent.solve!(...), and `using eil::nm::brent::solve;` stands for
// a plain `import nm.brent;`.
//
// Note on the reference build: nm/brent.d cannot be instantiated by ldc2 1.40 as
// written -- it is `@nogc` and throws `new Exception` (brent.d:51, :123), which the
// compiler rejects for every instantiation, including its own unittest block. The
// algorithm below is the same text; the throw is by value in C++ and has no GC.

#include <cmath>
#include <limits>
#include <string>
#include <type_traits>

#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/number.hpp"

namespace eil::nm::brent {

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
 * Locate a root of f(x) known to lie between x1 and x2. The method
 * is guaranteed to converge (according to Brent) given the initial
 * x values bound the solution. The method uses root bracketing,
 * bisection and inverse quadratic interpolation.
 *
 * Params:
 *    f: user-supplied function f(x)
 *    x1: first end of range
 *    x2: other end of range
 *    tol: minimum size for range
 *
 * Returns:
 *    b, a point near the root.
 *
 * That is D's own doc comment for solve(); the two solve() overloads at the bottom of
 * this file are the entry points it describes, and they share the body below.
 */
/**
 * The Brent iteration itself, spelled once.
 *
 * D's nm/brent.d declares one `solve` whose objective is a template parameter
 * (`solve!(zeroFun, double)`), and geom/misc/univariatefunctions.d:603 passes a
 * *nested function* that closes over the caller's B. A C++ non-type template parameter
 * can only name an object with linkage, so that shape needs a callable entry point.
 * Both spellings below forward to this function, exactly as
 * eil::nm::linesearch::minimize and its callable overload share minimize_fn
 * (wave-02-brief section 4b.5), so the root-finding arithmetic exists once and the two
 * doors cannot drift apart.
 */
template <typename F, typename T>
T solve_fn(F f, T x1, T x2, double tol = 1.0e-9)
{
    const int ITMAX = 100;           // maximum allowed number of iterations
    // Machine floating-point precision
    const double EPS = std::numeric_limits<double>::epsilon();
    T a = x1;
    T b = x2;
    T fa = f(a);
    T fb = f(b);
    if (std::fabs(fa) == 0.0) { return a; }
    if (std::fabs(fb) == 0.0) { return b; }
    if (fa * fb > 0.0) {
        // Supplied bracket does not encompass zero of the function.
        std::string msg = "Root must be bracketed by x1 and x2";
        // D's `debug { msg ~= format(...); }`: debug code is live in the
        // reference (non -release) build, so it is kept unconditionally.
        msg += eil::format("\nx1=%g f(x1)=%g x2=%g f(x2)=%g\n", x1, fa, x2, fb);
        throw eil::ExceptionBase(msg, __FILE__, __LINE__);
    }
    T c = b;
    T fc = fb;
    // Define d, e outside the loop body so that
    // they don't get initialized to nan each pass.
    // (D's default initializer for a floating local is NaN; C++ would leave them
    // indeterminate, so they are value-initialized. The first loop pass always
    // executes the `e = d = b-a` branch above any read of d or e, so the difference
    // is not observable.)
    T d = T();
    T e = T();
    for (int iter = 0; iter < ITMAX; ++iter) {
        if ((fb > 0.0 && fc > 0.0) || (fb < 0.0 && fc < 0.0)) {
            // On first pass fc==fb and we expect to enter here.
            c = a;
            fc = fa;
            e = d = b-a;
        }
        if (std::fabs(fc) < std::fabs(fb)) {
            a = b;
            b = c;
            c = a;
            fa = fb;
            fb = fc;
            fc = fa;
        }
        // Convergence check
        T tol1 = 2.0*EPS*std::fabs(b)+0.5*tol;
        T xm = 0.5*(c-b);
        if (std::fabs(xm) <= tol1 || fb == 0.0) {
            // If converged, let's return the best estimate
            return b;
        }
        if (std::fabs(e) >= tol1 && std::fabs(fa) > std::fabs(fb)) {
            // Attempt inverse quadratic interpolation for new bound
            T p = T();
            T q = T();
            T s = fb/fa;
            if ( a == c ) {
                p = 2.0*xm*s;
                q = 1.0-s;
            } else {
                q = fa/fc;
                T r = fb/fc;
                p = s*(2.0*xm*q*(q-r)-(b-a)*(r-1.0));
                q = (q-1.0)*(r-1.0)*(s-1.0);
            }
            // Check whether quadratic interpolation is in bounds
            if (p > 0.0) { q = -q; }
            p = std::fabs(p);
            T min1 = 3.0*xm*q-std::fabs(tol1*q);
            T min2 = std::fabs(e*q);
            if (2.0*p < (min1 < min2 ? min1 : min2) ) {
                // Accept interpolation
                e = d;
                d = p/q;
            } else {
                // Interpolation failed, use bisection
                d = xm;
                e = d;
            }
        } else {
            // Bounds decreasing too slowly, use bisection
            d = xm;
            e = d;
        }
        // Move last guess to a
        a = b;
        fa = fb;
        // Evaluate new trial root
        if (std::fabs(d) > tol1) {
            b += d;
        } else {
            b += std::copysign(tol1, xm);
        }
        fb = f(b);
    }
    throw eil::ExceptionBase("Maximum number of iterations exceeded in brent.solve()",
                             __FILE__, __LINE__);
} // end solve_fn()

template <auto f, typename T,
          typename = typename std::enable_if<detail::constraint<f>::value>::type>
T solve(T x1, T x2, double tol = 1.0e-9)
{
    return solve_fn(f, x1, x2, tol);
} // end solve()

/**
 * Overload for stateful callables (lambdas, closures over a member). See solve_fn.
 */
template <typename F, typename T>
T solve(F f, T x1, T x2, double tol = 1.0e-9)
{
    return solve_fn(f, x1, x2, tol);
} // end solve()

} // namespace eil::nm::brent
