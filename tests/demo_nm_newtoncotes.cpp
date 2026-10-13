// tests/demo_nm_newtoncotes.cpp — port of nm/newtoncotes_demo.d.
//
// newtoncotes_demo.d
// Peter J.
// 2014-06-20, Demo code adapted from the Python version.
//
// Example transcript:
// peterj@helmholtz ~/cfcfd3/dlang/nm $ ./newtoncotes_demo
// Begin Newton-Cotes integration demonstration...
// Estimates of pi/4: 0.785398 0.785398
// errors: 8.47579e-09 -1.0154e-08
// number of function calls: 315 55
// Done.
//
// Comparison basis: ldc2 -w -dip1008 -preview=in over newtoncotes_demo.d,
// newtoncotes.d, ntypes/complex.d, number.d; stdout diffed byte for byte.
//
// Two spelling differences, both forced by C++ rather than chosen:
//   * D's `alias f` becomes the auto template parameter of
//     eil::nm::newtoncotes::integrate (see the header note), so the call site
//     reads integrate<fun1,double>(a, b) instead of integrate!(fun1,double)(a, b).
//   * std.math.PI is an 80-bit `real` in this ldc2, so D's `PI/4 - pi4_1` is
//     evaluated in extended precision. The port has no `real` (see
//     substrate/include/eil/number.hpp) and uses the nearest double, 0x1.921fb54442d18p+1.
//     Measured effect on the printed value: 3.06e-17 absolute on a ~8.5e-9 result,
//     which is invisible at %.6g -- both languages print "8.47579e-09".

#include <cmath>
#include <cstdio>

#include "eil/eil.hpp"
#include "nm/newtoncotes.hpp"

namespace {

// D's `static uint count1/count2` at file scope.
unsigned count1 = 0;
unsigned count2 = 0;

// The 80-bit real literal std.math.PI rounds to the double below.
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
    eil::writeln("Begin Newton-Cotes integration demonstration...");
    double a = 0.0;
    double b = 1.0;
    double pi4_1 = eil::nm::newtoncotes::integrate<fun1,double>(a, b);
    double pi4_2 = eil::nm::newtoncotes::integrate<fun2,double>(a, b);
    eil::writefln("Estimates of pi/4: %s %s", pi4_1, pi4_2);
    eil::writefln("errors: %s %s", PI/4 - pi4_1, PI/4 - pi4_2);
    eil::writefln("number of function calls: %s %s", count1, count2);
    eil::writeln("Done.");
    return 0;
}
