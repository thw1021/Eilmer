// tests/test_nm_bbla.cpp — the five unittest blocks from nm/bbla.d.
//
//   bbla.d:666-697  @("test_basic_operations")
//   bbla.d:733-757  @("test_elimination")
//   bbla.d:902-919  @("test_decomp_and_inverse")
//   bbla.d:960-974  @("test_lsqsolve")
//   bbla.d:1078-1108 @("test_Crout_decomp_and_solve")
//
// The D blocks compile and pass in the reference tree with the makefile's own
// flags (`ldc2 -w -dip1008 -preview=in -unittest -main`), so every assertion here
// is the D assertion, unchanged, with the ported spellings from nm/bbla.hpp:
// `m[i,j]` -> m(i,j) for bbla's dense matrix, `new Matrix!T(...)` ->
// new_matrix<T>(...), `x[]` views -> x.all(), and `to!number(v)` ->
// static_cast<number>(v) (nm/number.d's number is double in the trunk build).

#include <cstdio>

#include "eil/eil.hpp"
#include "nm/bbla.hpp"
#include "nm/number.hpp"

namespace {

using eil::nm::number;
namespace bbla = eil::nm::bbla;

int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    } else {
        std::printf("ok: %s\n", what);
    }
}

// bbla.d:666-697
void test_basic_operations() {
    bbla::Matrix<number> a = bbla::eye<number>(3);
    check(bbla::approxEqualMatrix<number>(a, bbla::new_matrix<number>({{1, 0, 0}, {0, 1, 0}, {0, 0, 1}})),
          "bbla: eye(3) equals the integer-literal identity");
    bbla::Matrix<number> b = bbla::zeros<number>(2, 3);
    bbla::Matrix<number> c = bbla::transpose<number>(b);
    b(1, 2) = static_cast<number>(99.0);
    c(0, 0) = static_cast<number>(1.0);
    c(1, 1) = static_cast<number>(1.0);
    check(bbla::approxEqualMatrix<number>(b, bbla::new_matrix<number>({{0, 0, 0}, {0, 0, 99}})),
          "bbla: writing b[1,2] after transposing leaves b as expected");
    check(bbla::approxEqualMatrix<number>(c, bbla::new_matrix<number>({{1, 0}, {0, 1}, {0, 0}})),
          "bbla: transpose view is independent of b");

    bbla::Matrix<number> e = bbla::new_matrix<number>({1.0, 2.0, 3.0});
    check(bbla::approxEqualMatrix<number>(e, bbla::new_matrix<number>({{1}, {2}, {3}})),
          "bbla: column vector constructor");
    bbla::Matrix<number> e2 = bbla::new_matrix<number>({1, 2, 3}, "row");
    check(bbla::approxEqualMatrix<number>(e2, bbla::new_matrix<number>({{1, 2, 3}})),
          "bbla: row vector constructor");

    bbla::Matrix<number> f = bbla::new_matrix<number>({{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}});
    bbla::Matrix<number> g = bbla::dot<number>(f, c);
    check(bbla::approxEqualMatrix<number>(g, bbla::new_matrix<number>({{1, 2}, {4, 5}})),
          "bbla: dot(f,c) returns the product");
    g.zeros();
    check(bbla::approxEqualMatrix<number>(g, bbla::new_matrix<number>({{0, 0}, {0, 0}})),
          "bbla: zeros() clears in place");
    bbla::dot<number>(f, c, g);
    check(bbla::approxEqualMatrix<number>(g, bbla::new_matrix<number>({{1, 2}, {4, 5}})),
          "bbla: dot(f,c,g) writes through the third argument");

    g.eye();
    check(bbla::approxEqualMatrix<number>(g, bbla::new_matrix<number>({{1, 0}, {0, 1}})),
          "bbla: eye() on a 2x2");
}

// bbla.d:733-757
void test_elimination() {
    bbla::Matrix<number> A = bbla::new_matrix<number>({
        {0.0, 2.0, 0.0, 1.0},
        {2.0, 2.0, 3.0, 2.0},
        {4.0, -3.0, 0.0, 1.0},
        {6.0, 1.0, -6.0, -5.0}
    });
    bbla::Matrix<number> b = bbla::new_matrix<number>({0.0, -2.0, -7.0, 6.0}, "column");
    bbla::Matrix<number> Ab = bbla::hstack<number>({A, b});
    bbla::Matrix<number> Aonly = Ab.sliceDup(0, 4, 0, 4);
    bbla::Matrix<number> bonly = Ab.sliceDup(0, 4, 4, 5);
    bbla::gaussJordanElimination<number>(Ab);
    check(bbla::approxEqualMatrix<number>(Ab, bbla::new_matrix<number>({
                {1, 0, 0, 0, -0.5},
                {0, 1.0, 0, 0, 1},
                {0, 0, 1, 0, 1.0 / 3},
                {0, 0, 0, 1, -2.0}
    })), "bbla: Gauss-Jordan reduces [A|b] to [I|x]");
    eil::Array<number> x = Ab.getColumn(4);
    bbla::Matrix<number> new_rhs = bbla::dot<number>(Aonly, bbla::new_matrix<number>(x.all()));
    check(bbla::approxEqualMatrix<number>(new_rhs, bonly), "bbla: A.x recovers the original b");
    bbla::Matrix<number> residual = new_rhs - b;
    check(bbla::approxEqualMatrix<number>(residual, bbla::new_matrix<number>({0, 0, 0, 0})),
          "bbla: residual against b is zero");
}

// bbla.d:902-919
void test_decomp_and_inverse() {
    auto A = bbla::new_matrix<number>({
        {0.0, 2.0, 0.0, 1.0}, {2.0, 2.0, 3.0, 2.0}, {4.0, -3.0, 0.0, 1.0}, {
            6.0, 1.0, -6.0, -5.0
        }
    });
    auto b = bbla::new_matrix<number>({0.0, -2.0, -7.0, 6.0}, "column");
    auto c = bbla::new_matrix<number>(A);
    auto perm = bbla::decomp<number>(c);
    auto x = bbla::new_matrix<number>(b);
    bbla::solve<number>(c, x, perm.all());
    auto residual = b - bbla::dot<number>(A, x);
    check(bbla::approxEqualMatrix<number>(residual, bbla::new_matrix<number>({0, 0, 0, 0})),
          "bbla: decomp/solve residual is zero");

    auto y = bbla::inverse<number>(A);
    check(bbla::approxEqualMatrix<number>(bbla::dot<number>(A, y), bbla::eye<number>(4)),
          "bbla: A . inverse(A) is the identity");
}

// bbla.d:960-974
void test_lsqsolve() {
    auto A = bbla::new_matrix<number>({
        {0.0, 2.0, 0.0, 1.0},
        {2.0, 2.0, 3.0, 2.0},
        {4.0, 4.0, 6.0, 4.0},
        {4.0, -3.0, 0.0, 1.0},
        {4.0, -3.0, 0.0, 1.0},
        {6.0, 1.0, -6.0, -5.0}
    });
    auto b = bbla::new_matrix<number>({{0.0}, {-2.0}, {-4.0}, {-7.0}, {-7.0}, {6.0}});
    auto xx = bbla::lsqsolve<number>(A, b);
    auto expected_xx = bbla::new_matrix<number>({-0.5, 1, 1.0 / 3, -2}, "column");
    check(bbla::approxEqualMatrix<number>(xx, expected_xx),
          "bbla: least-squares solve of the over-constrained system");
}

// bbla.d:1078-1108
void test_Crout_decomp_and_solve() {
    auto A = bbla::new_matrix<number>({
        {0.0, 2.0, 0.0, 1.0},
        {2.0, 2.0, 3.0, 2.0},
        {4.0, -3.0, 0.0, 1.0},
        {6.0, 1.0, -6.0, -5.0}
    });
    eil::Array<number> b = {static_cast<number>(0.0), static_cast<number>(-2.0),
                            static_cast<number>(-7.0), static_cast<number>(6.0)};
    eil::Array<int> pivot;
    pivot.set_length(4);
    eil::Array<number> x;
    x.set_length(4);

    bbla::LUDecomp<number>(A, pivot);
    bbla::LUSolve<number>(A, pivot.all(), b.all(), x);

    // Reset A since it was converted to LU format.
    A = bbla::new_matrix<number>({
        {0.0, 2.0, 0.0, 1.0},
        {2.0, 2.0, 3.0, 2.0},
        {4.0, -3.0, 0.0, 1.0},
        {6.0, 1.0, -6.0, -5.0}
    });

    eil::Array<number> Ax;
    Ax.set_length(4);
    bbla::dot(A, x.all(), Ax.all());

    check(eil::nm::approxEqualNumbers(b.all(), Ax.all()),
          "bbla: Crout LU solve reproduces b through A.x");
}

} // namespace

int main() {
    test_basic_operations();
    test_elimination();
    test_decomp_and_inverse();
    test_lsqsolve();
    test_Crout_decomp_and_solve();

    std::printf("%s: nm.bbla unittests (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
