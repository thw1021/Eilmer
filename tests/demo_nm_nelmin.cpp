// tests/demo_nm_nelmin.cpp — the D program nm/nelmin_demo.d, stdout byte-identical
// to baselines/e587b235/probes/nm_nelmin_demo.txt (captured from the ldc2 binary).
//
// The demo is the loosest possible end-to-end pin on the ported minimiser: it
// prints the converged point through to!string (six significant digits), f_min,
// the convergence flag, the evaluation count and the restart count for three
// objective functions, and the second and third calls reuse the `fx` declared by
// the first (nelmin_demo.d:69), which the port mirrors because the D `out`
// parameter resets it on each call.
//
// test 3 here runs with P=1 (nelmin_demo.d:98) where the nm/nelmin.d unittest uses
// P=2 (nelmin.d:422), so the demo and the unittest corpus cover both branch shapes.

#include <cmath>
#include <cstdio>
#include <string>

#include "eil/array.hpp"
#include "eil/format.hpp"
#include "nm/nelmin.hpp"
#include "nm/number.hpp"

namespace {

using number = eil::nm::number;
using Objective = double (*)(eil::Slice<number>);

// nelmin_demo.d:24-29
number testFunction1(eil::Slice<number> x) {
    number sum = 0.0;
    for (size_t i = 0; i < x.length(); ++i) sum += (x[i] - 1.0) * (x[i] - 1.0);
    return sum;
}

// nelmin_demo.d:33-45
number testFunction2(eil::Slice<number> x) {
    number x1 = x[0]; number x2 = x[1];   // rename to match the paper
    if ((x1 * x1 + x2 * x2) > 1.0) {
        return number(1.0e38);
    } else {
        number yp = 53.69 + 7.26 * x1 - 10.33 * x2 + 7.22 * x1 * x1
            + 6.43 * x2 * x2 + 11.36 * x1 * x2;
        number ys = 82.17 - 1.01 * x1 - 8.61 * x2 + 1.40 * x1 * x1
            - 8.76 * x2 * x2 - 7.20 * x1 * x2;
        return -yp + std::fabs(ys - 87.8);
    }
}

// nelmin_demo.d:49-67
number testFunction3(eil::Slice<number> z) {
    eil::Array<double> x({0.25, 0.50, 1.00, 1.70, 2.00, 4.00});
    eil::Array<double> y({0.25, 0.40, 0.60, 0.58, 0.54, 0.27});
    number a1 = z[0]; number a2 = z[1];
    number alpha1 = z[2]; number alpha2 = z[3];
    number sum_residuals = 0.0;
    for (int i = 0; i < 6; ++i) {
        number t = x[i];
        number eta = a1 * std::exp(alpha1 * t) + a2 * std::exp(alpha2 * t);
        number r = y[i] - eta;
        sum_residuals += r * r;
    }
    return sum_residuals;
}

} // namespace

int main() {
    eil::writeln("Begin nelmin demostration...");

    eil::writeln("---------------------------------------------------");
    eil::writeln("test 1: simple quadratic with zero at (1,1,...)");
    eil::Array<number> x({number(0.0), number(0.0), number(0.0), number(0.0)});
    eil::Array<number> dx;
    number fx;
    int nfe, nres;
    bool conv_flag = eil::nm::nelmin::minimize<Objective, number>(testFunction1, x, fx, nfe,
                                                                 nres, dx.all());
    eil::writeln("x = ", x, " fx = ", fx);
    eil::writeln("convergence-flag = ", conv_flag);
    eil::writeln("number-of-fn-evaluations = ", nfe);
    eil::writeln("number-of-restarts = ", nres);

    eil::writeln("---------------------------------------------------");
    eil::writeln("test 2: Example 3.3 in Olsson and Nelson f(0.811,-0.585)=-67.1");
    eil::Array<number> x2({number(0.0), number(0.0)});
    eil::Array<number> dx2({number(0.5), number(0.5)});
    conv_flag = eil::nm::nelmin::minimize<Objective, number>(testFunction2, x2, fx, nfe,
                                                            nres, dx2.all(), 1.0e-4);
    eil::writeln("x = ", x2, " fx = ", fx);
    eil::writeln("convergence-flag = ", conv_flag);
    eil::writeln("number-of-fn-evaluations = ", nfe);
    eil::writeln("number-of-restarts =", nres);

    eil::writeln("---------------------------------------------------");
    eil::writeln("test 3: Example 3.5 in Olsson and Nelson, nonlinear least-squares");
    eil::writeln("f(1.801, -1.842, -0.463, -1.205)=0.0009");
    eil::Array<number> x3({number(1.0), number(1.0), number(-0.5), number(-2.5)});
    eil::Array<number> dx3({number(0.1), number(0.1), number(0.1), number(0.1)});
    conv_flag = eil::nm::nelmin::minimize<Objective, number>(testFunction3, x3, fx, nfe,
                                                            nres, dx3.all(), 1.0e-9, 1, 800);
    eil::writeln("x = ", x3, " fx = ", fx);
    eil::writeln("convergence-flag = ", conv_flag);
    eil::writeln("number-of-fn-evaluations = ", nfe);
    eil::writeln("number-of-restarts = ", nres);

    eil::writeln("---------------------------------------------------");
    eil::writeln("Done.");
    return 0;   // end main()
}
