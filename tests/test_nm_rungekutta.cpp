// tests/test_nm_rungekutta.cpp — the unittest block from nm/rungekutta.d:103-129.
//
// The D block compiles and passes in the reference tree (`ldc2 -unittest -main`
// over rungekutta.d, number.d, ntypes/complex.d reports "1 modules passed
// unittests"). It takes one RKF45 step of size 0.2 from t=0 on a 3-variable linear
// system and compares the result against the analytic solution to 1.0e-5.
//
// Translation notes:
//   * `number[] testSystem1(number t, number[] x)` becomes
//     eil::Array<eil::nm::number>(*)(eil::nm::number, eil::Slice<eil::nm::number>):
//     the parameter is D's `T[]` view, the returned array literal allocates (see
//     the header note in src/nm/rungekutta.hpp). The unittest declares it inside the
//     block, so it is hoisted to namespace scope for C++ to be able to name it.
//   * `ref T[] y1` / `ref T[] err` are ported as `eil::Slice<T>` by value: the
//     stepper writes elements through the view and never rebinds or resizes the
//     caller's array, which is the only thing D's `ref` would have bought.
//   * `allocate_rk45_workspace!number(3)` returns `T[][]` -> eil::Array<eil::Array<T>>,
//     7 rows of 3 columns, and is passed as a reference to the caller's object.
//   * exp() is glibc's here and Phobos' in D. Phobos' double exp is 1 ULP off the
//     correctly rounded value on some arguments (measured over k/10, k=1..30: 9 of
//     90 values differ, always by exactly 1 ULP, and at exp(1) and exp(-0.6) it is
//     D that is high). That only touches the analytic solution used as a comparison
//     target, never the stepper: over the 10-step march below the stepper's own
//     state and error components are bit-identical, and the few `exact` values that
//     differ are at most 2 ULP apart (max relative 2.99e-16, from the cancellation
//     in the solution formula). The assertions use D's printed bits and pass
//     exactly.
//
// The extra checks pin down the parts a residual history would expose: the
// workspace shape (7 rows), the exact stage-result values from the D run at
// %.17g, that repeated stepping feeds y1 back into y0 in the right order (10-step
// march), and the message text of the length-mismatch Exception.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "eil/eil.hpp"
#include "nm/number.hpp"   // the D unittest's `import nm.number;`
#include "nm/rungekutta.hpp"

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

using eil::nm::number;

// rungekutta.d:107-112 (`t` is unused by this autonomous system, as in D)
eil::Array<number> testSystem1(number t, eil::Slice<number> x) {
    (void)t;
    number dx0dt = -8.0 / 3.0 * x[0] - 4.0 / 3.0 * x[1] + x[2] + 12.0;
    number dx1dt = -17.0 / 3.0 * x[0] - 4.0 / 3.0 * x[1] + x[2] + 29.0;
    number dx2dt = -35.0 / 3.0 * x[0] + 14.0 / 3.0 * x[1] - 2.0 * x[2] + 48.0;
    return eil::Array<number>{dx0dt, dx1dt, dx2dt};
}

// rungekutta.d:114-119
eil::Array<number> solution1(number t) {
    number x = std::exp(-3.0 * t) / 6.0 * (6.0 - 50.0 * std::exp(t) + 10.0 * std::exp(2.0 * t) + 34.0 * std::exp(3.0 * t));
    number y = std::exp(-3.0 * t) / 6.0 * (12.0 - 125.0 * std::exp(t) + 40.0 * std::exp(2.0 * t) + 73.0 * std::exp(3.0 * t));
    number z = std::exp(-3.0 * t) / 6.0 * (14.0 - 200.0 * std::exp(t) + 70.0 * std::exp(2.0 * t) + 116.0 * std::exp(3.0 * t));
    return eil::Array<number>{x, y, z};
}

} // namespace

int main() {
    // rungekutta.d:121-127
    eil::Array<number> x0 = {0.0, 0.0, 0.0};
    eil::Array<number> x1 = x0.dup();
    eil::Array<number> err = x0.dup();
    auto work = eil::nm::rungekutta::allocate_rk45_workspace<number>(3);
    number t0 = 0.0;
    number h = 0.2;
    number t1 = eil::nm::rungekutta::rkf45_step<testSystem1, number>(
                    t0, h, x0.all(), x1.all(), err.all(), work);
    eil::Array<number> sol = solution1(t1);
    // rungekutta.d:128
    check(eil::nm::approxEqualNumbers(x1.all(), sol.all(), 1.0e-5),
          "rungekutta: one RKF45 step matches the analytic solution");
    std::printf("     t1=%.17g x1=%.17g %.17g %.17g\n", t1, x1[0], x1[1], x1[2]);
    std::printf("     err=%.17g %.17g %.17g\n", err[0], err[1], err[2]);

    check(t1 == 0.20000000000000001 && x1[0] == 1.9940202940170939
          && x1[1] == 4.7574902632478615 && x1[2] == 7.8217772786324797,
          "rungekutta: stage return and one-step result are D's bits");
    check(err[0] == 2.9538461537997884e-06 && err[0] >= 0.0
          && err[1] == 6.7343589743557031e-05 && err[2] == 0.00021314871794858581,
          "rungekutta: error estimates match D and are the abs() of them");

    // The workspace is 7 rows of n, in D's order (ytmp first, then k1..k6).
    check(work.length() == 7 && work[0].length() == 3 && work[6].length() == 3,
          "rungekutta: workspace is 7 arrays of length n");

    // Repeated stepping: y1 becomes the next y0. D reference is the same 10-step
    // march with h=0.1, values at %.17g from the D run.
    eil::Array<number> y = {0.0, 0.0, 0.0};
    eil::Array<number> yn = y.dup();
    eil::Array<number> e2 = y.dup();
    auto w2 = eil::nm::rungekutta::allocate_rk45_workspace<number>(3);
    number t = 0.0;
    for (int step = 0; step < 10; ++step) {
        t = eil::nm::rungekutta::rkf45_step<testSystem1, number>(t, 0.1, y.all(),
                                                             yn.all(), e2.all(), w2);
        for (size_t i = 0; i < 3; ++i) y[i] = yn[i];
    }
    check(t == 0.99999999999999989 && y[0] == 5.2017924536510503
          && y[1] == 11.89928640224495 && y[2] == 19.230255916465381,
          "rungekutta: 10-step march reproduces D's accumulated state");
    std::printf("     march t=%.17g y=%.17g %.17g %.17g err=%.17g %.17g %.17g\n",
                t, y[0], y[1], y[2], e2[0], e2[1], e2[2]);

    // rungekutta.d:60-62 -- the length-mismatch guard, on D's exact message.
    bool caught = false;
    try {
        eil::Array<number> bad = {0.0, 0.0};
        eil::Array<number> b1 = bad.dup();
        eil::Array<number> be = bad.dup();
        eil::nm::rungekutta::rkf45_step<testSystem1, number>(0.0, 0.2, bad.all(),
                                                             b1.all(), be.all(), work);
    } catch (const eil::ExceptionBase& ex) {
        caught = std::strcmp(ex.what(), "Array lengths don't match the workspace and.") == 0;
    }
    check(caught, "rungekutta: mismatched array length throws D's Exception text");

    std::printf("%s: nm.rungekutta unittests (%d failures)\n", failures ? "FAIL" : "PASS",
                failures);
    return failures ? 1 : 0;
}
