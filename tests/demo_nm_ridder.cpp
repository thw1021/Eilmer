// tests/demo_nm_ridder.cpp — port of nm/ridder_demo.d.
//
// ridder_demo.d
// Solve a nonlinear equation f(x)=0 using the method of Ridder.
// Peter J.
// demo code 13-Jun-2014, adapted from the C++ version
//
// The D demo binds `test_fun_3` as a closure delegate over the local `my_a`
// (ridder_demo.d:33-34). A C++ non-type template argument has to name a
// function, so my_a is a namespace-scope constant and test_fun_3 a named
// function; the demo never changes my_a from 3.0, so nothing else moves.

#include <cmath>
#include <cstdio>

#include "eil/eil.hpp"
#include "nm/bracketing.hpp"
#include "nm/ridder.hpp"

double test_fun_1(double x) {
    return std::pow(x,3.0) + x*x - 3*x - 3;
}

double test_fun_2(double x, double a) {
    return a*x + std::sin(x) - std::exp(x);
}

namespace {
const double my_a = 3.0;
double test_fun_3(double x) { return test_fun_2(x, my_a); }
} // namespace

int main() {
    eil::writeln("Begin demonstration of using Ridder's function solver...");
    eil::writeln();
    eil::writeln("Example 1 is from Gerald and Wheatley, p. 45");
    eil::writeln("Solve f(x) = x^3 + x^2 - 3x -3 = 0 with initial ",
            "guesses of x0 = 1 and x1 = 2.");
    eil::writeln("Final result x = ", eil::nm::ridder::solve<test_fun_1,double>(1, 2));
    eil::writeln("Gerald and Wheatley report x = 1.732051");
    eil::writeln();
    //
    eil::writeln("Example 2 is from Gerald and Wheatley, p.45 also, ",
            "but using a delegate for closure.");
    eil::writeln("Solve f(x) = 3*x + sin(x) - e^x = 0 with initial ",
            "guesses of x0 = 0 and x1 = 1.");
    eil::writeln("Final result x = ", eil::nm::ridder::solve<test_fun_3,double>(0, 1));
    eil::writeln("Gerald and Wheatley report x = 0.3604217");
    eil::writeln();
    //
    eil::writeln("Bracket a root of the second example.");
    double x1 = 0.4;
    double x2 = 0.5;
    int result_flag = eil::nm::bracket<test_fun_3,double>(x1, x2);
    eil::writeln("result_flag=", result_flag, " x1=", x1, " x2=", x2);
    eil::writeln("Done.");
    return 0;
}
