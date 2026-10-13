/** tests/demo_nm_rungekutta.cpp — port of nm/rungekutta_demo.d.
 *
 * Try out the Runge-Kutta ODE stepper.
 *
 * Author: Peter J.
 * Version: 2014-06-15, fresh start with Dlang
 *
 * Example transcript:
 * peterj@helmholtz ~/cfcfd3/dlang/nm $ ./rungekutta_demo
 * Begin demonstration of ODE stepper...
 * x1 = [1.99402, 4.75749, 7.82178]
 * exact = [1.99403, 4.75749, 7.82175]
 * Done.
 *
 * Comparison basis: ldc2 -w -dip1008 -preview=in over rungekutta_demo.d,
 * rungekutta.d, ntypes/complex.d, number.d; stdout diffed byte for byte.
 *
 * Signature adaptations, both forced by C++ and neither of which moves a value:
 *   * D's `double[] f(double t, double[] x)` becomes
 *     eil::Array<double> f(double t, eil::Slice<double> x): the parameter is a
 *     view in D and the returned array literal allocates (see the note at the top
 *     of src/nm/rungekutta.hpp).
 *   * `t` is unused by the autonomous test system; D emits no warning for that and
 *     C++ does, so it is named-and-dropped.
 *
 * exp() here is glibc's, not Phobos'. On the four arguments this demo evaluates
 * (exp(0.2), exp(0.4), exp(0.6000000000000001) = exp(3.0*0.2) and its negation)
 * the two agree bit for bit, so the "exact" line matches; marching the same
 * solution to t=1.0 is where they part by 1 ULP (see tests/test_nm_rungekutta.cpp).
 */

#include <cmath>
#include <cstdio>

#include "eil/eil.hpp"
#include "nm/rungekutta.hpp"

/** Test system 1
 * Third-order system with a simple analytic solution.
 * Adapted from section 11.3 in Cheney and Kincaid, 6th ed.
 * Except for the zero-based indexing, the notation is
 * chosen to match that in the text.
 */
eil::Array<double> testSystem1(double t, eil::Slice<double> x)
{
    (void)t;  // the system is autonomous; D leaves t unused too
    double dx0dt =  -8.0/3.0*x[0] -  4.0/3.0*x[1] +     x[2] + 12.0;
    double dx1dt = -17.0/3.0*x[0] -  4.0/3.0*x[1] +     x[2] + 29.0;
    double dx2dt = -35.0/3.0*x[0] + 14.0/3.0*x[1] - 2.0*x[2] + 48.0;
    return eil::Array<double>{dx0dt, dx1dt, dx2dt};
}

eil::Array<double> solution1(double t)
{
    double x = std::exp(-3.0*t)/6.0*(6.0-50.0*std::exp(t)+10.0*std::exp(2.0*t)+34.0*std::exp(3.0*t));
    double y = std::exp(-3.0*t)/6.0*(12.0-125.0*std::exp(t)+40.0*std::exp(2.0*t)+73.0*std::exp(3.0*t));
    double z = std::exp(-3.0*t)/6.0*(14.0-200.0*std::exp(t)+70.0*std::exp(2.0*t)+116.0*std::exp(3.0*t));
    return eil::Array<double>{x, y, z};
}

int main()
{
    eil::writeln("Begin demonstration of ODE stepper...");
    eil::Array<double> x0 = {0.0, 0.0, 0.0};
    eil::Array<double> x1 = x0.dup();
    eil::Array<double> err = x0.dup();
    auto work = eil::nm::rungekutta::allocate_rk45_workspace<double>(3);
    double t1 = eil::nm::rungekutta::rkf45_step<testSystem1, double>(0.0, 0.2,
              x0.all(), x1.all(), err.all(), work);
    eil::writeln("x1 = ", x1);
    eil::writeln("exact = ", solution1(t1));
    eil::writeln("Done.");
    return 0;
}
