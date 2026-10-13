/**
 * tests/demo_nm_linesearch.cpp — port of nm/linesearch_demo.d.
 *
 * linesearch_demo.d
 * Example of using the one-dimensional line search for minimization.
 *
 * Peter J. 2014-06-14
 *
 * Example transcript:
 *
 * peterj@laval ~/cfcfd3/dlang/nm $ ./linesearch_demo
 * Begin demonstration of line search.
 * x=-1.4721 f(-1.4721)=2.13091
 * bracket=(-0.588534,-0.588533)
 * Done.
 */

#include <cmath>
#include <cstdio>

#include "eil/eil.hpp"
#include "nm/linesearch.hpp"

double fdemo(double x) {
    return std::exp(x) + 2.0 - std::cos(x);
}

int main() {
    eil::writeln("Begin demonstration of line search.");
    double x = -1.4721;
    eil::writeln("x=", x, " f(", x, ")=", fdemo(x));

    double a = -3;
    double b = 1;
    eil::nm::linesearch::minimize<fdemo>(a, b, 1.0e-6);
    eil::writeln("bracket=(", a, ",", b, ")");
    eil::writeln("Done.");
    return 0;
}
