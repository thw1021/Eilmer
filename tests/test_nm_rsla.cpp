// tests/test_nm_rsla.cpp — the unittest block from nm/rsla.d:194-257.
//
// The D block compiles and passes in the reference tree (`ldc2 -w -dip1008
// -preview=in -unittest -main nm/rsla.d nm/number.d ntypes/complex.d`), so this is
// the same sequence of assertions. Two spelling notes:
//
//   * D's `number[8][4] A` is a static array read right-to-left -- 4 rows of 8
//     numbers -- so the port declares `number A[4][8]` (see the header note in
//     src/nm/rsla.hpp). The template argument list keeps D's order
//     <N, NDIM, NDIM2, T>.
//   * D's `assert(isSingular == 0, "Matrix `A` is singular")` becomes a check with
//     the same message text, because the harness reports rather than aborts.
//
// The same augmented matrix is reused at N=4, N=2 and N=3, exactly as D does,
// which is the point of the oversized NDIM/NDIM2 storage.

#include <cstdio>

#include "eil/eil.hpp"
#include "nm/number.hpp"
#include "nm/rsla.hpp"

namespace {

using eil::nm::number;
namespace rsla = eil::nm::rsla;

int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    } else {
        std::printf("ok: %s\n", what);
    }
}

} // namespace

int main() {
    int isSingular;

    number A[4][8] = {{static_cast<number>(0.0),  static_cast<number>(2.0),  static_cast<number>(0.0),  static_cast<number>(1.0),
                       static_cast<number>(1.0), static_cast<number>(0.0), static_cast<number>(0.0), static_cast<number>(0.0)},
                      {static_cast<number>(2.0),  static_cast<number>(2.0),  static_cast<number>(3.0),  static_cast<number>(2.0),
                       static_cast<number>(0.0), static_cast<number>(1.0), static_cast<number>(0.0), static_cast<number>(0.0)},
                      {static_cast<number>(4.0), static_cast<number>(-3.0),  static_cast<number>(0.0),  static_cast<number>(1.0),
                       static_cast<number>(0.0), static_cast<number>(0.0), static_cast<number>(1.0), static_cast<number>(0.0)},
                      {static_cast<number>(6.0),  static_cast<number>(1.0), static_cast<number>(-6.0), static_cast<number>(-5.0),
                       static_cast<number>(0.0), static_cast<number>(0.0), static_cast<number>(0.0), static_cast<number>(1.0)}};
    check(eil::nm::approxEqualNumbers(rsla::normInf<4,4,8,number>(A), static_cast<number>(18.0)),
          "rsla: normInf of the 4x4 augmented matrix is 18");
    isSingular = rsla::computeInverse<4,4,8,number>(A);
    check(isSingular == 0, "Matrix `A` is singular");

    number b[4] = {static_cast<number>(0.0), static_cast<number>(-2.0), static_cast<number>(-7.0), static_cast<number>(6.0)};
    // D NaN-initialises `number[4] x;`; every element is written by
    // solveWithInverse before it is read, so zero-initialising is not observable.
    number x[4] = {};
    rsla::solveWithInverse<4,4,8,number>(A, b, x);

    check(eil::nm::approxEqualNumbers(x[0], static_cast<number>(-0.5)), "rsla: x[0] == -0.5");
    check(eil::nm::approxEqualNumbers(x[1], static_cast<number>(1.0)),  "rsla: x[1] == 1.0");
    check(eil::nm::approxEqualNumbers(x[2], static_cast<number>(1.0/3)), "rsla: x[2] == 1/3");
    check(eil::nm::approxEqualNumbers(x[3], static_cast<number>(-2.0)), "rsla: x[3] == -2.0");

    // Try same workspace with a smaller 2x2 system.
    x[0] = -0.5; x[1] = 1.0;
    A[0][0] = 0.0; A[0][1] = 2.0; A[0][2] = 1.0; A[0][3] = 0.0;
    A[1][0] = 2.0; A[1][1] = 2.0; A[1][2] = 0.0; A[1][3] = 1.0;
    b[0] = A[0][0]*x[0] + A[0][1]*x[1];
    b[1] = A[1][0]*x[0] + A[1][1]*x[1];
    check(eil::nm::approxEqualNumbers(rsla::normInf<2,4,8,number>(A), static_cast<number>(4.0)),
          "rsla: normInf of the 2x2 system is 4");

    isSingular = rsla::computeInverse<2,4,8,number>(A);
    check(isSingular == 0, "Matrix `A` is singular");

    x[0] = 0.0;
    x[1] = 0.0;
    rsla::solveWithInverse<2,4,8,number>(A, b, x);
    check(eil::nm::approxEqualNumbers(x[0], static_cast<number>(-0.5)), "rsla: 2x2 x[0] == -0.5");
    check(eil::nm::approxEqualNumbers(x[1], static_cast<number>(1.0)),  "rsla: 2x2 x[1] == 1.0");

    // and again, with a 3x3 system.
    x[0] = -0.5; x[1] = 1.0; x[2] = 1.0/3;
    A[0][0] = 0.0; A[0][1] = 2.0; A[0][2] = 0.0;
    A[1][0] = 2.0; A[1][1] = 2.0; A[1][2] = 3.0;
    A[2][0] = 4.0; A[2][1] = -3.0; A[2][2] = 0.0;
    A[0][3] = 1.0; A[0][4] = 0.0; A[0][5] = 0.0;
    A[1][3] = 0.0; A[1][4] = 1.0; A[1][5] = 0.0;
    A[2][3] = 0.0; A[2][4] = 0.0; A[2][5] = 1.0;
    b[0] = A[0][0]*x[0] + A[0][1]*x[1] + A[0][2]*x[2];
    b[1] = A[1][0]*x[0] + A[1][1]*x[1] + A[1][2]*x[2];
    b[2] = A[2][0]*x[0] + A[2][1]*x[1] + A[2][2]*x[2];
    check(eil::nm::approxEqualNumbers(rsla::normInf<3,4,8,number>(A), static_cast<number>(7.0)),
          "rsla: normInf of the 3x3 system is 7");

    isSingular = rsla::computeInverse<3,4,8,number>(A);
    check(isSingular == 0, "Matrix `A` is singular");

    x[0] = 0.0; x[1] = 0.0; x[2] = 0.0;
    rsla::solveWithInverse<3,4,8,number>(A, b, x);
    check(eil::nm::approxEqualNumbers(x[0], static_cast<number>(-0.5)), "rsla: 3x3 x[0] == -0.5");
    check(eil::nm::approxEqualNumbers(x[1], static_cast<number>(1.0)),  "rsla: 3x3 x[1] == 1.0");
    check(eil::nm::approxEqualNumbers(x[2], static_cast<number>(1.0/3)), "rsla: 3x3 x[2] == 1/3");

    std::printf("%s: nm.rsla unittests (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
