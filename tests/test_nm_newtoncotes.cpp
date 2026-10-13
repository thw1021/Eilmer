// tests/test_nm_newtoncotes.cpp — the unittest block from nm/newtoncotes.d:47-64.
//
// The D block compiles and passes in the reference tree (`ldc2 -unittest -main`
// over newtoncotes.d, number.d, ntypes/complex.d reports "1 modules passed
// unittests"). Both assertions integrate a quarter-circle kernel and an arctan
// kernel on [0, 1] with the `number` scalar and compare against PI/4 to 1.0e-6.
//
// Translation notes (shared by this cluster):
//   * D's unittest declares fun1/fun2 and the `const number one/zero` locals
//     inside the block. A C++ auto non-type template argument has to name an
//     entity at namespace scope, so they are hoisted unchanged into an anonymous
//     namespace -- the same shape tests/test_nm_ridder.cpp uses for the
//     closure-delegate case.
//   * `to!number(PI / 4)` converts a real to a double in D, and std.math.PI is a
//     genuine 80-bit real in this ldc2 (it prints as 0x1.921fb54442d1846ap+1). The
//     port uses the nearest double, 0x1.921fb54442d18p+1. The two differ by
//     3.06e-17 in the comparison target, against the 1e-6 *relative* tolerance of
//     approxEqualNumbers, so the assertion cannot distinguish them.
//   * `abs` and `sqrt` are std.math's; eil::sqrt carries the NaN-sign
//     canonicalization in substrate/include/eil/math.hpp.
//
// The extra checks below the unittests are behaviour, not tolerance: the
// function-call counts at tol=1.0e-8 are the shape of the split recursion, so they
// only reproduce if the panel arithmetic and the split test agree bit for bit.
// Reference values are the D program's own output at %.17g.

#include <cmath>
#include <cstdio>

#include "eil/eil.hpp"
#include "nm/newtoncotes.hpp"
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
const eil::nm::number one = 1.0;
const eil::nm::number zero = 0.0;

// newtoncotes.d:54-56
eil::nm::number fun1(eil::nm::number x) {
    return std::fabs(x) < one ? eil::sqrt(one - x * x) : zero;
}

// newtoncotes.d:58-60
eil::nm::number fun2(eil::nm::number x) {
    return 1.0 / (1.0 + x * x);
}

// Counting wrappers for the split-shape checks.
unsigned calls1 = 0;
unsigned calls2 = 0;
eil::nm::number cfun1(eil::nm::number x) { ++calls1; return fun1(x); }
eil::nm::number cfun2(eil::nm::number x) { ++calls2; return fun2(x); }

} // namespace

int main() {
    // newtoncotes.d:62
    eil::nm::number r1 = eil::nm::newtoncotes::integrate<fun1, eil::nm::number>(zero, one);
    check(eil::nm::approxEqualNumbers(PI / 4, r1, 1.0e-6),
          "newtoncotes: integrate of sqrt(1 - x^2) on [0,1] is pi/4");
    // newtoncotes.d:63
    eil::nm::number r2 = eil::nm::newtoncotes::integrate<fun2, eil::nm::number>(zero, one);
    check(eil::nm::approxEqualNumbers(PI / 4, r2, 1.0e-6),
          "newtoncotes: integrate of 1/(1 + x^2) on [0,1] is pi/4");
    std::printf("     pi/4 estimates: %.17g %.17g\n", r1, r2);

    // D at tol=1.0e-8: 0.78539816339717627 / 0.78539816339808088, 1725 and 305 calls.
    // maxAbsDiff is passed as 0.0 here (unlike the unittest assertions above, which
    // keep D's default 1.0e-5) so that these are really 1e-15 relative comparisons,
    // i.e. about 4 ULP, rather than a 1e-5 absolute test.
    eil::nm::number t8 = 1.0e-8;
    eil::nm::number v1 = eil::nm::newtoncotes::integrate<cfun1, eil::nm::number>(zero, one, t8);
    eil::nm::number v2 = eil::nm::newtoncotes::integrate<cfun2, eil::nm::number>(zero, one, t8);
    check(eil::nm::approxEqualNumbers(v1, 0.78539816339717627, 1.0e-15, 0.0) && calls1 == 1725,
          "newtoncotes: tol 1e-8 quarter circle matches D value and call count");
    check(eil::nm::approxEqualNumbers(v2, 0.78539816339808088, 1.0e-15, 0.0) && calls2 == 305,
          "newtoncotes: tol 1e-8 arctan kernel matches D value and call count");
    std::printf("     tol 1e-8: %.17g (%u calls) %.17g (%u calls)\n",
                v1, calls1, v2, calls2);

    // D at tol=1.0e-8 on [0.75, 1.0], where the sqrt endpoint forces deepest
    // splitting: f1 0.11332793849351917, f2 0.1418970546039845.
    eil::nm::number s1 = eil::nm::newtoncotes::integrate<fun1, eil::nm::number>(0.75, 1.0, t8);
    eil::nm::number s2 = eil::nm::newtoncotes::integrate<fun2, eil::nm::number>(0.75, 1.0, t8);
    check(eil::nm::approxEqualNumbers(s1, 0.11332793849351917, 1.0e-15, 0.0),
          "newtoncotes: split-heavy segment [0.75,1.0] matches D");
    check(eil::nm::approxEqualNumbers(s2, 0.1418970546039845, 1.0e-15, 0.0),
          "newtoncotes: smooth segment [0.75,1.0] matches D");
    std::printf("     segment [0.75,1.0]: %.17g %.17g\n", s1, s2);

    std::printf("%s: nm.newtoncotes unittests (%d failures)\n", failures ? "FAIL" : "PASS",
                failures);
    return failures ? 1 : 0;
}
