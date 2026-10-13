#pragma once
/**
 * rungekutta.d
 *
 * Ordinary differential equation integration.
 *
 * Author: Peter J.
 * Version: 2014-Jun-15, adapted from the mech2700 class example
 *          2014-Jul-09, preallocate work arrays and pass them in.
 *          2018-May-26, work with double or complex numbers
 *          2018-May-30, accept the type of the dependent variables as a parameter
 */
// nm/rungekutta.d -> src/nm/rungekutta.hpp (module nm.rungekutta).
//
// Naming. namespace eil::nm::rungekutta. This is what the solver's ODE-style
// updates build on, so the body is a line-for-line transcription: the six stage
// evaluations and the five ytmp updates strictly alternate (k1, ytmp, k2, ytmp,
// ...), and that composition order is the algorithm. Nothing here may be
// hoisted, folded or re-associated; each `ytmp[i] = ...` statement keeps D's
// operand order and its literal coefficients (1932.0/2197.0 etc. are *not*
// simplified), with -ffp-contract=off keeping the multiply/add pairs as two
// instructions the way ldc2 emits them.
//
// The callback. D's `alias f` is bound at compile time and takes
// (t, y) -> dy/dt as a fresh array; it is not a delegate and has no captured
// context, so the port keeps it as an `auto` non-type template parameter (the
// shape used by nm/bracketing.hpp and nm/newtoncotes.hpp) and calls `f(t0, y0)`
// unchanged. Two signature points:
//   * the dependent-variable array argument becomes `eil::Slice<T>` (D's `T[]`
//     parameter is a view, per substrate/include/eil/array.hpp), and
//   * the return becomes `eil::Array<T>`, the owned `new T[n]` side of the same
//     rule, because D's array literal `[dx0dt, dx1dt, dx2dt]` allocates.
// D's constraint `is(typeof(f(0.0, [0.0,])) == double[])` inspects only the
// return type of an unevaluated call, so the trait below uses declval for both
// arguments and compares against eil::Array<double>/float/Complex<double>.
//
// Views vs. references. In D, `k1 = work_arrays[1]` aliases the workspace row and
// `k1[] = f(...)` writes through that alias; the port takes `eil::Slice<T>` views
// of the same rows, so writes land in the caller's storage exactly as they do in
// D. The `ref T[] y1`, `ref T[] err` and `ref T[][] work_arrays` parameters are
// ported as a Slice by value and an Array reference: D's `ref` on an array matters
// only if the callee rebinds or resizes it, and rkf45_step does neither -- it
// writes elements through the view, which a Slice also does. The workspace itself
// is passed by reference because the function indexes into the caller's array of
// rows.
//
// One shape that is not 1:1, and is loud in both languages rather than silent: if
// a stage array coming back from f (or a y1/err element index) does not match the
// workspace row, ldc2's bounds check aborts the process with SIGILL and prints
// nothing (measured with a f returning 2 elements into rows of 3). The port throws
// eil::SliceError / eil::RangeError instead. The guard D wrote at
// rungekutta.d:60-62, which is the path a caller can actually hit, keeps its
// message text and throws ExceptionBase.
//
// The Complex branch (rungekutta.d:54-55) is wave-12 work: the trait recognises
// Array<Complex<double>>, but the arithmetic needs ntypes/complex.d's operators,
// which today's re/im-only Complex<T> does not provide.
//
// libm. The stepper body uses no transcendental at all -- only +, -, *, / and the
// fabs over the error estimates -- so its output is pure IEEE and reproduces bit
// for bit; measured over a 10-step march at h=0.1 every state and error component
// matches D exactly. exp() appears only in the analytic solution the demos and the
// test compare against, and that is where the one divergence in this cluster
// lives (Phobos' exp is 1 ULP off glibc's on some arguments; see
// tests/test_nm_rungekutta.cpp). The float instantiation D permits is supported
// and matches bit for bit: one float step of y' = -2y from y = 1 with h = 0.2
// gives t1=0x1.99999ap-3, y1=0x1.5733bep-1, err=0x1.fa99dap-17 in both languages.

#include <cmath>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

#include "eil/array.hpp"
#include "eil/error.hpp"
#include "eil/number.hpp"

namespace eil::nm::rungekutta {

/**
 * Allocate workspace arrays for the ODE stepper.
 *
 * To avoid the allocation of the intermediate arrays at every update,
 * we get the user to preallocate them with this function.
 */
template <typename T>
eil::Array<eil::Array<T>> allocate_rk45_workspace(unsigned n)
{
    eil::Array<eil::Array<T>> workspace_arrays;
    workspace_arrays.set_length(7);
    for (auto& vect : workspace_arrays) { vect.set_length(n); }
    return workspace_arrays;
}

namespace detail {
// D's template constraint
//   if (is(typeof(f(0.0, [0.0,])) == double[]) ||
//       is(typeof(f(0.0, [0.0,])) == float[]) ||
//       is(typeof(f(Complex!double(0.0), [Complex!double(0.0),Complex!double(0.0)]))
//          == Complex!double[]))
// via the detection idiom (see the note in bracketing.hpp). The call is
// unevaluated in both languages, so the array argument is a declval view rather
// than a one-element literal.
template <auto f, typename A, typename = void> struct call_result { using type = void; };
template <auto f, typename A>
struct call_result<f, A,
    std::void_t<decltype(f(std::declval<A>(), std::declval<eil::Slice<A>&>()))>>
{
    using type = decltype(f(std::declval<A>(), std::declval<eil::Slice<A>&>()));
};

template <auto f>
struct constraint : std::integral_constant<bool,
    std::is_same<typename call_result<f, double>::type, eil::Array<double>>::value ||
    std::is_same<typename call_result<f, float>::type, eil::Array<float>>::value ||
    std::is_same<typename call_result<f, eil::Complex<double>>::type,
                 eil::Array<eil::Complex<double>>>::value> {};
} // namespace detail

/**
 * Steps the set of ODEs by the Runge-Kutta-Fehlberg method.
 *
 * Params:
 *     f: a callable function that returns the derivative of y wrt t
 *        The signature of this function is f(t, y) where
 *        t is a float value, y is an array of number values.
 *     t0: is the starting value of the independent variable
 *     h: the requested step size
 *     y0: an array of starting values for the dependent variables
 *         It is assumed that the y-elements are indexed 0 .. n-1
 *         where n = y0.length
 *     y1: an array of final values of the dependent variables
 *     err: estimates of the errors in the values of y1
 *
 * Returns:
 *     the final value of the dependent variable
 */
template <auto f, typename T,
          typename = typename std::enable_if<detail::constraint<f>::value>::type>
T rkf45_step(T t0, T h, eil::Slice<T> y0, eil::Slice<T> y1, eil::Slice<T> err,
             eil::Array<eil::Array<T>>& work_arrays)
{
    // Assuming a system of equations, we need arrays for the intermediate data.
    size_t n = y0.length();
    eil::Slice<T> k1 = work_arrays[1].all();
    if ( k1.length() != y0.length() ) {
        // D: `throw new Exception(...)`. The message text is D's, including the
        // trailing stray "and."; call sites print e.msg.
        throw eil::ExceptionBase("Array lengths don't match the workspace and.",
                                 __FILE__, __LINE__);
    }
    eil::Slice<T> k2 = work_arrays[2].all();
    eil::Slice<T> k3 = work_arrays[3].all();
    eil::Slice<T> k4 = work_arrays[4].all();
    eil::Slice<T> k5 = work_arrays[5].all();
    eil::Slice<T> k6 = work_arrays[6].all();
    eil::Slice<T> ytmp = work_arrays[0].all();
    // Build up the sample point information as per the text book descriptions.
    // We assign the result of intermediate array expressions to ytmp
    // because that's needed for D.
    k1.assign_from(f(t0, y0).all());
    for (size_t i = 0; i < n; ++i) { ytmp[i] = y0[i] + 0.25*h*k1[i]; }
    k2.assign_from(f(t0 + h/4.0, ytmp).all());
    for (size_t i = 0; i < n; ++i) { ytmp[i] = y0[i] + 3.0*h*k1[i]/32.0 + 9.0*h*k2[i]/32.0; }
    k3.assign_from(f(t0 + 3.0*h/8.0, ytmp).all());
    for (size_t i = 0; i < n; ++i) {
        ytmp[i] = y0[i] + 1932.0*h*k1[i]/2197.0 - 7200.0*h*k2[i]/2197.0 +
            7296.0*h*k3[i]/2197.0;
    }
    k4.assign_from(f(t0 + 12.0*h/13.0, ytmp).all());
    for (size_t i = 0; i < n; ++i) {
        ytmp[i] = y0[i] + 439.0*h*k1[i]/216.0 - 8.0*h*k2[i] +
            3680.0*h*k3[i]/513.0 - 845.0*h*k4[i]/4104.0;
    }
    k5.assign_from(f(t0 + h, ytmp).all());
    for (size_t i = 0; i < n; ++i) {
        ytmp[i] = y0[i] - 8.0*h*k1[i]/27.0 + 2.0*h*k2[i] -
            3544.0*h*k3[i]/2565.0 + 1859.0*h*k4[i]/4104.0 - 11.0*h*k5[i]/40.0;
    }
    k6.assign_from(f(t0 + h/2.0, ytmp).all());
    // Now, do the integration as a weighting of the sampled data.
    for (size_t i = 0; i < n; ++i) {
        y1[i] = y0[i] + 16.0*h*k1[i]/135.0 + 6656.0*h*k3[i]/12825.0 +
            28561.0*h*k4[i]/56430.0 - 9.0*h*k5[i]/50.0 + 2.0*h*k6[i]/55.0;
        err[i] = h*k1[i]/360.0 - 128.0*h*k3[i]/4275.0 - 2197.0*h*k4[i]/75240.0 +
            h*k5[i]/50.0 + 2.0*h*k6[i]/55.0;
    }
    for (auto& e : err) { e = std::fabs(e); }
    return t0 + h;
} // end rkf45_step()

} // namespace eil::nm::rungekutta
