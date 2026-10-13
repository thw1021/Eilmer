// tests/demo_nm_gaussquad.cpp — port of nm/gaussquad_demo.d.
//
// gaussquad_demo.d
// Peter J.
// 2014-06-21, Demo code adapted from the Newton-Cotes version.
//
// Example transcript:
// peterj@helmholtz ~/cfcfd3/dlang/nm $ ./gaussquad_demo
// Begin Gauss quadrature demonstration...
// Estimates of pi/4: 0.785398 0.785398
// errors: -1.3845e-08 -1.15028e-08
// number of function calls: 231 21
// Done.
//
// Comparison basis: the command in the porting notes (ldc2 -w -dip1008
// -preview=in over gaussquad_demo.d, gaussquad.d, ntypes/complex.d, number.d),
// stdout diffed byte for byte against this program.
//
// The D demo writes `integrate!fun1(a, b)`, letting D infer T from the argument
// type; a C++ template argument list has to name it, so the port spells
// integrate<fun1,double>. See tests/demo_nm_newtoncotes.cpp for the note on
// std.math.PI being an 80-bit real in the reference build.

#include <cmath>
#include <cstdio>

#include "eil/eil.hpp"
#include "nm/gaussquad.hpp"

namespace {

unsigned count1 = 0;
unsigned count2 = 0;

const double PI = 3.14159265358979323846264338327950288419716939937510;

} // namespace

double fun1(double x) {
    count1++;
    return std::fabs(x) < 1.0 ? eil::sqrt(1.0 - x*x): 0.0;
}

double fun2(double x) {
    count2++;
    return 1.0 / (1.0 + x * x);
}

int main() {
    eil::writeln("Begin Gauss quadrature demonstration...");
    double a = 0.0;
    double b = 1.0;
    double pi4_1 = eil::nm::gaussquad::integrate<fun1,double>(a, b);
    double pi4_2 = eil::nm::gaussquad::integrate<fun2,double>(a, b);
    eil::writefln("Estimates of pi/4: %s %s", pi4_1, pi4_2);
    eil::writefln("errors: %s %s", PI/4 - pi4_1, PI/4 - pi4_2);
    eil::writefln("number of function calls: %s %s", count1, count2);
    eil::writeln("Done.");
    return 0;
}
