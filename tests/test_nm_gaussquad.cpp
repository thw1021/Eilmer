// tests/test_nm_gaussquad.cpp — the unittest block from nm/gaussquad.d:84-92.
//
// The D block compiles and passes in the reference tree (`ldc2 -unittest -main`
// over gaussquad.d, number.d, ntypes/complex.d reports "1 modules passed
// unittests"). As in newtoncotes, the two assertions compare the integral of a
// quarter-circle kernel and an arctan kernel on [0, 1] against PI/4 to 1.0e-6.
//
// See tests/test_nm_newtoncotes.cpp for the shared translation notes: functions
// hoisted out of the unittest block so a template argument can name them, and
// std.math.PI being an 80-bit real in the reference build.
//
// Extra checks, all pinned to the D program's own output:
//   * the rule tables and their lengths (xs3/ws3 have 3 nodes, xs4/ws4 have 4),
//     because integrate reads them as module globals exactly as D does;
//   * apply_gauss_rule on a single panel with no splitting, which pins the node
//     transform and the left-to-right accumulation order of `result += ws[i]*f(x)`
//     (1/(1+x^2) needs no libm, so any difference there is a porting error, not a
//     transcendental);
//   * the split count at tol=1.0e-8, which only reproduces if the difference
//     between the two rules is bit-identical.
// The D-side reference values are printed at %.17g by /tmp/probe/gq_probe.d.

#include <cmath>
#include <cstdio>

#include "eil/eil.hpp"
#include "nm/gaussquad.hpp"
#include "nm/number.hpp"   // the D unittest's `import nm.number;`

namespace {

int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    } else {
        std::printf("ok: %s\n", what);
    }
}

const eil::nm::number PI = 3.14159265358979323846264338327950288419716939937510;

// gaussquad.d:88-89
eil::nm::number fun1(eil::nm::number x) {
    return std::fabs(x) < 1.0 ? eil::sqrt(1.0 - x*x) : 0.0;
}

eil::nm::number fun2(eil::nm::number x) {
    return 1.0 / (1.0 + x * x);
}

unsigned calls2 = 0;
eil::nm::number cfun2(eil::nm::number x) { ++calls2; return fun2(x); }

} // namespace

int main() {
    const eil::nm::number zero = 0.0;
    const eil::nm::number one = 1.0;

    // gaussquad.d:90
    eil::nm::number r1 = eil::nm::gaussquad::integrate<fun1, eil::nm::number>(zero, one);
    check(eil::nm::approxEqualNumbers(PI/4, r1, 1.0e-6),
          "gaussquad: integrate of sqrt(1 - x^2) on [0,1] is pi/4");
    // gaussquad.d:91
    eil::nm::number r2 = eil::nm::gaussquad::integrate<fun2, eil::nm::number>(zero, one);
    check(eil::nm::approxEqualNumbers(PI/4, r2, 1.0e-6),
          "gaussquad: integrate of 1/(1 + x^2) on [0,1] is pi/4");
    std::printf("     pi/4 estimates: %.17g %.17g\n", r1, r2);

    check(eil::nm::gaussquad::xs3.length() == 3 && eil::nm::gaussquad::ws3.length() == 3
          && eil::nm::gaussquad::xs4.length() == 4 && eil::nm::gaussquad::ws4.length() == 4,
          "gaussquad: rule tables have 3 and 4 nodes");
    // The coefficients as D wrote them, read back at the same precision.
    check(eil::nm::gaussquad::xs3[0] == -7.7459666924148337704e-1
          && eil::nm::gaussquad::xs3[1] == 0.0
          && eil::nm::gaussquad::xs3[2] == 7.7459666924148337704e-1
          && eil::nm::gaussquad::ws3[0] == 5.5555555555555555556e-1
          && eil::nm::gaussquad::ws3[1] == 8.8888888888888888889e-1
          && eil::nm::gaussquad::xs4[0] == -8.6113631159405257523e-1
          && eil::nm::gaussquad::xs4[1] == -3.399810435848562648e-1
          && eil::nm::gaussquad::xs4[2] == 3.399810435848562648e-1
          && eil::nm::gaussquad::xs4[3] == 8.6113631159405257523e-1
          && eil::nm::gaussquad::ws4[0] == 3.4785484513745385738e-1
          && eil::nm::gaussquad::ws4[1] == 6.5214515486254614263e-1
          && eil::nm::gaussquad::ws4[2] == 6.5214515486254614263e-1
          && eil::nm::gaussquad::ws4[3] == 3.4785484513745385738e-1,
          "gaussquad: rule coefficients are the D literals bit for bit");

    // Single panels on [0, 0.25], no recursion. D:
    //   r3f2 0.24497867619775285   r4f2 0.24497866309221636
    //   r3f1 0.24737085894326716   r4f1 0.24737085714545659
    eil::nm::number g3 = eil::nm::gaussquad::apply_gauss_rule<fun2, eil::nm::number>(
                             zero, 0.25, eil::nm::gaussquad::xs3.all(),
                             eil::nm::gaussquad::ws3.all());
    eil::nm::number g4 = eil::nm::gaussquad::apply_gauss_rule<fun2, eil::nm::number>(
                             zero, 0.25, eil::nm::gaussquad::xs4.all(),
                             eil::nm::gaussquad::ws4.all());
    check(g3 == 0.24497867619775285, "gaussquad: 3-point panel on [0,0.25] is D's value");
    check(g4 == 0.24497866309221636, "gaussquad: 4-point panel on [0,0.25] is D's value");
    std::printf("     panels [0,0.25]: %.17g %.17g\n", g3, g4);

    eil::nm::number h3 = eil::nm::gaussquad::apply_gauss_rule<fun1, eil::nm::number>(
                             zero, 0.25, eil::nm::gaussquad::xs3.all(),
                             eil::nm::gaussquad::ws3.all());
    eil::nm::number h4 = eil::nm::gaussquad::apply_gauss_rule<fun1, eil::nm::number>(
                             zero, 0.25, eil::nm::gaussquad::xs4.all(),
                             eil::nm::gaussquad::ws4.all());
    check(eil::nm::approxEqualNumbers(h3, 0.24737085894326716, 1.0e-15, 0.0),
          "gaussquad: 3-point panel of the sqrt kernel is D's value");
    check(eil::nm::approxEqualNumbers(h4, 0.24737085714545659, 1.0e-15, 0.0),
          "gaussquad: 4-point panel of the sqrt kernel is D's value");
    std::printf("     sqrt panels [0,0.25]: %.17g %.17g\n", h3, h4);

    // D at tol=1.0e-8 over [0,1]: 0.78539816339088597 after 91 calls.
    eil::nm::number v2 = eil::nm::gaussquad::integrate<cfun2, eil::nm::number>(zero, one, 1.0e-8);
    check(eil::nm::approxEqualNumbers(v2, 0.78539816339088597, 1.0e-15, 0.0) && calls2 == 91,
          "gaussquad: tol 1e-8 arctan kernel matches D value and call count");
    std::printf("     tol 1e-8: %.17g (%u calls)\n", v2, calls2);

    std::printf("%s: nm.gaussquad unittests (%d failures)\n", failures ? "FAIL" : "PASS",
                failures);
    return failures ? 1 : 0;
}
