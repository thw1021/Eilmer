// tests/test_nm_smla.cpp — the unittest block from nm/smla.d:1115-1505.
//
// The D block compiles and passes in the reference tree (`ldc2 -w -dip1008
// -preview=in -unittest -main nm/smla.d nm/bbla.d nm/stmatrix.d nm/number.d
// ntypes/complex.d`, run from src/nm so the .mtx path resolves). Each section
// below is the corresponding D section, with the ported spellings from
// src/nm/smla.hpp:
//
//   new SMatrix!T(aa, ja, ia)  -> new_smatrix<T>({aa}, {ja}, {ia})
//   m[i,j]         (read)      -> m.get(i,j)
//   m[i,j] = v     (write)     -> m.set(i,j) = v      (inserts if absent, as in D)
//   arr[] = value              -> arr.fill(value)
//   arr[] = A[] - arr[]        -> element-wise loop, same index order as D
//   `ref` array arguments      -> .all() views of the caller's eil::Array
//
// D asserts per element inside a foreach; the port folds each vector check into
// one reported line so the log stays readable, and names the expected vector.
//
// Section order and the reuse of x/x0/C1 follow D exactly, because some of those
// variables carry state from one section into the next.

#include <cstdio>
#include <string>

#include "eil/eil.hpp"
#include "nm/bbla.hpp"
#include "nm/number.hpp"
#include "nm/smla.hpp"
#include "nm/stmatrix.hpp"

namespace {

using eil::nm::number;
namespace bbla = eil::nm::bbla;
namespace smla = eil::nm::smla;
namespace stmatrix = eil::nm::stmatrix;

int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    } else {
        std::printf("ok: %s\n", what);
    }
}

// D's `foreach (i; 0 .. c.length) assert(approxEqualNumbers(c[i], expected[i]));`
bool numbers_agree(eil::Slice<number> got, eil::Slice<number> want) {
    if (got.length() != want.length()) return false;
    for (size_t i = 0; i < got.length(); ++i) {
        if (!eil::nm::approxEqualNumbers(got[i], want[i])) return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    std::string mtx_path = "test_data/b1_ss.mtx";   // smla.d:1383
    if (argc > 1) mtx_path = argv[1];

    // This example matrix is from Saad (2003), Sec. 3.4
    auto a = smla::new_smatrix<number>(
        {static_cast<number>(1.), static_cast<number>(2.), static_cast<number>(3.), static_cast<number>(4.),
         static_cast<number>(5.), static_cast<number>(6.), static_cast<number>(7.), static_cast<number>(8.),
         static_cast<number>(9.), static_cast<number>(10.), static_cast<number>(11.), static_cast<number>(12.)},
        {0, 3, 0, 1, 3, 0, 2, 3, 4, 2, 3, 4},
        {0, 2, 5, 9, 11, 12});
    // Test construction of matrix row by row
    auto b = smla::new_smatrix<number>();
    b.addRow({static_cast<number>(1.), static_cast<number>(2.)}, {0, 3});
    b.addRow({static_cast<number>(3.), static_cast<number>(4.), static_cast<number>(5.)}, {0, 1, 3});
    b.addRow({static_cast<number>(6.), static_cast<number>(7.), static_cast<number>(8.), static_cast<number>(9.)}, {0, 2, 3, 4});
    b.addRow({static_cast<number>(10.), static_cast<number>(11.)}, {2, 3});
    b.addRow({static_cast<number>(12.)}, {4});
    check(smla::approxEqualMatrix<number>(a, b), "smla: row-by-row construction equals the CRS literal");

    // Test dropping rows
    auto aa = smla::new_smatrix<number>(a);
    aa.dropRows(2);
    auto aaRef = smla::new_smatrix<number>(
        {static_cast<number>(1.), static_cast<number>(2.), static_cast<number>(3.), static_cast<number>(4.),
         static_cast<number>(5.), static_cast<number>(6.), static_cast<number>(7.), static_cast<number>(8.),
         static_cast<number>(9.)},
        {0, 3, 0, 1, 3, 0, 2, 3, 4},
        {0, 2, 5, 9});
    check(smla::approxEqualMatrix<number>(aa, aaRef), "smla: dropRows(2) removes the last two rows");

    // Test matrix multiply
    eil::Array<number> v = {static_cast<number>(1.), static_cast<number>(2.), static_cast<number>(3.),
                            static_cast<number>(4.), static_cast<number>(5.)};
    eil::Array<number> c;
    c.set_length(v.length());
    smla::multiply(a, v.all(), c.all());
    eil::Array<number> expected_c = {static_cast<number>(9.), static_cast<number>(31.), static_cast<number>(104.),
                                     static_cast<number>(74.), static_cast<number>(60.)};
    check(numbers_agree(c.all(), expected_c.all()), "smla: A.v matches Saad's expected product");

    // Test transpose
    auto cc = smla::new_smatrix<double>();
    cc.addRow({1.0, 3.0}, {0, 2});
    cc.addRow({4.0}, {2});
    cc.addRow({7.0}, {1});
    auto ccT = cc.transpose();

    double ccT_aa_exp[] = {1.0, 7.0, 3.0, 4.0};
    size_t ccT_ja_exp[] = {0, 2, 0, 1};
    size_t ccT_ia_exp[] = {0, 1, 2, 4};
    bool aa_ok = ccT.aa().length() == 4;
    for (size_t i = 0; i < ccT.aa().length() && i < 4; ++i) {
        aa_ok = aa_ok && eil::isClose(ccT.aa()[i], ccT_aa_exp[i]);
    }
    check(aa_ok, "smla: transpose aa values");
    bool ja_ok = ccT.ja().length() == 4;
    for (size_t i = 0; i < ccT.ja().length() && i < 4; ++i) {
        ja_ok = ja_ok && (ccT.ja()[i] == ccT_ja_exp[i]);
    }
    check(ja_ok, "smla: transpose ja indices");
    bool ia_ok = ccT.ia().length() == 4;
    for (size_t i = 0; i < ccT.ia().length() && i < 4; ++i) {
        ia_ok = ia_ok && (ccT.ia()[i] == ccT_ia_exp[i]);
    }
    check(ia_ok, "smla: transpose ia offsets");

    // Test decompILU0
    // This example matrix and decomposition is from Gerard and Wheatley, 6th ed, Sec. 2.4
    auto d = smla::new_smatrix<number>();
    d.addRow({static_cast<number>(-4.), static_cast<number>(2.)}, {0, 1});
    d.addRow({static_cast<number>(1.), static_cast<number>(-4.), static_cast<number>(1.)}, {0, 1, 2});
    d.addRow({static_cast<number>(1.), static_cast<number>(-4.), static_cast<number>(1.)}, {1, 2, 3});
    d.addRow({static_cast<number>(1.), static_cast<number>(-4.), static_cast<number>(1.)}, {2, 3, 4});
    d.addRow({static_cast<number>(2.), static_cast<number>(-4.)}, {3, 4});
    smla::decompILU0(d);
    auto dLU = smla::new_smatrix<number>();
    dLU.addRow({static_cast<number>(-4.), static_cast<number>(2.)}, {0, 1});
    dLU.addRow({static_cast<number>(-0.25), static_cast<number>(-3.5), static_cast<number>(1.)}, {0, 1, 2});
    dLU.addRow({static_cast<number>(-0.2857), static_cast<number>(-3.7143), static_cast<number>(1.)}, {1, 2, 3});
    dLU.addRow({static_cast<number>(-0.2692), static_cast<number>(-3.7308), static_cast<number>(1.)}, {2, 3, 4});
    dLU.addRow({static_cast<number>(-0.5361), static_cast<number>(-3.4639)}, {3, 4});
    check(smla::approxEqualMatrix<number>(d, dLU), "smla: ILU(0) matches Gerald and Wheatley Sec. 2.4");

    // Test solve.
    // Let's give the ILU(0) method a triangular matrix that is can solve exactly.
    // This example is taken from Faires and Burden (1993), Sec. 6.6, example 3.
    auto e = smla::new_smatrix<number>();
    e.addRow({static_cast<number>(2.), static_cast<number>(-1.)}, {0, 1});
    e.addRow({static_cast<number>(-1.), static_cast<number>(2.), static_cast<number>(-1.)}, {0, 1, 2});
    e.addRow({static_cast<number>(-1.), static_cast<number>(2.), static_cast<number>(-1.)}, {1, 2, 3});
    e.addRow({static_cast<number>(-1.), static_cast<number>(2.)}, {2, 3});

    eil::Array<number> B = {static_cast<number>(1.), static_cast<number>(0.), static_cast<number>(0.),
                            static_cast<number>(1.)};
    smla::solve(e, B.all());
    eil::Array<number> B_exp = {static_cast<number>(1.), static_cast<number>(1.), static_cast<number>(1.),
                                static_cast<number>(1.)};
    check(numbers_agree(B.all(), B_exp.all()), "smla: sparse LU solve of the Faires and Burden system");

    // Now let's see how we go at an approximate solve by using a non-triangular matrix.
    // This is example 2.2 from Gerard and Wheatley, 6th edition
    auto f = smla::new_smatrix<number>();
    f.addRow({static_cast<number>(3.), static_cast<number>(2.), static_cast<number>(-1.), static_cast<number>(2.)}, {0, 1, 2, 3});
    f.addRow({static_cast<number>(1.), static_cast<number>(4.), static_cast<number>(2.)}, {0, 1, 3});
    f.addRow({static_cast<number>(2.), static_cast<number>(1.), static_cast<number>(2.), static_cast<number>(-1.)}, {0, 1, 2, 3});
    f.addRow({static_cast<number>(1.), static_cast<number>(1.), static_cast<number>(-1.), static_cast<number>(3.)}, {0, 1, 2, 3});
    smla::decompILU0(f);
    eil::Array<number> C = {static_cast<number>(2.), static_cast<number>(2.), static_cast<number>(0.),
                            static_cast<number>(0.)};
    smla::solve(f, C.all());
    eil::Array<number> C_exp = {
        static_cast<number>(0.333333), static_cast<number>(0.666667), static_cast<number>(-1), static_cast<number>(-0.666667)
    };
    check(numbers_agree(C.all(), C_exp.all()), "smla: ILU(0) approximate solve to 6 decimals");

    // Let's test the ILU(p) method
    auto s = smla::new_smatrix<number>(
        {static_cast<number>(1.), static_cast<number>(1.), static_cast<number>(4.), static_cast<number>(2.),
         static_cast<number>(4.), static_cast<number>(1.), static_cast<number>(2.), static_cast<number>(1.),
         static_cast<number>(8.), static_cast<number>(2.), static_cast<number>(4.), static_cast<number>(1.),
         static_cast<number>(3.), static_cast<number>(6.), static_cast<number>(2.), static_cast<number>(1.)},
        {0, 1, 4, 1, 2, 4, 0, 1, 2, 3, 2, 3, 0, 1, 2, 4},
        {0, 3, 6, 10, 12, 16});
    int p;

    // test for ILU(p=2)
    s = smla::new_smatrix<number>(
        {static_cast<number>(1.), static_cast<number>(1.), static_cast<number>(4.), static_cast<number>(2.),
         static_cast<number>(4.), static_cast<number>(1.), static_cast<number>(2.), static_cast<number>(1.),
         static_cast<number>(8.), static_cast<number>(2.), static_cast<number>(4.), static_cast<number>(1.),
         static_cast<number>(3.), static_cast<number>(6.), static_cast<number>(2.), static_cast<number>(1.)},
        {0, 1, 4, 1, 2, 4, 0, 1, 2, 3, 2, 3, 0, 1, 2, 4},
        {0, 3, 6, 10, 12, 16});
    p = 2;
    smla::decompILUp(s, p);
    auto sol1 = smla::new_smatrix<number>(
        {static_cast<number>(1.), static_cast<number>(1.), static_cast<number>(4.), static_cast<number>(2.),
         static_cast<number>(4.), static_cast<number>(1.), static_cast<number>(2.), static_cast<number>(-0.5),
         static_cast<number>(10.), static_cast<number>(2.), static_cast<number>(-7.5), static_cast<number>(0.4),
         static_cast<number>(0.2), static_cast<number>(3.), static_cast<number>(3.), static_cast<number>(1.5),
         static_cast<number>(-0.4), static_cast<number>(4.), static_cast<number>(-27.5)},
        {0, 1, 4, 1, 2, 4, 0, 1, 2, 3, 4, 2, 3, 4, 0, 1, 2, 3, 4},
        {0, 3, 6, 11, 14, 19});
    check(smla::approxEqualMatrix<number>(s, sol1), "smla: ILU(p=2) grows the pattern to 19 entries");

    // Test GMRES on Faires and Burden problem.
    auto g = smla::new_smatrix<number>();
    g.addRow({static_cast<number>(2.), static_cast<number>(-1.)}, {0, 1});
    g.addRow({static_cast<number>(-1.), static_cast<number>(2.), static_cast<number>(-1.)}, {0, 1, 2});
    g.addRow({static_cast<number>(-1.), static_cast<number>(2.), static_cast<number>(-1.)}, {1, 2, 3});
    g.addRow({static_cast<number>(-1.), static_cast<number>(2.)}, {2, 3});
    eil::Array<number> B1 = {static_cast<number>(1.), static_cast<number>(0.), static_cast<number>(0.),
                             static_cast<number>(1.)};
    eil::Array<number> x0 = {static_cast<number>(1.2), static_cast<number>(0.8), static_cast<number>(0.9),
                             static_cast<number>(1.1)};

    auto x = smla::gmres<number>(g, B1.all(), x0.all(), 4);
    check(numbers_agree(x.all(), B_exp.all()), "smla: gmres reaches the exact solution");

    x = smla::gmres2<number>(g, B1.all(), x0.all(), 5, 1.0e-10);
    check(numbers_agree(x.all(), B_exp.all()), "smla: gmres2 reaches the exact solution");

    // Test pre-conditioned GMRES on Gerald and Wheatley problem.
    // This time we expect the exact answer. Earlier we only used
    // an incomplete LU factorisation and so the result was
    // only approximate.
    auto h = smla::new_smatrix<number>();
    h.addRow({static_cast<number>(3.), static_cast<number>(2.), static_cast<number>(-1.), static_cast<number>(2.)}, {0, 1, 2, 3});
    h.addRow({static_cast<number>(1.), static_cast<number>(4.), static_cast<number>(2.)}, {0, 1, 3});
    h.addRow({static_cast<number>(2.), static_cast<number>(1.), static_cast<number>(2.), static_cast<number>(-1.)}, {0, 1, 2, 3});
    h.addRow({static_cast<number>(1.), static_cast<number>(1.), static_cast<number>(-1.), static_cast<number>(3.)}, {0, 1, 2, 3});
    auto Ph = smla::new_smatrix<number>(h);
    smla::decompILU0<number>(Ph);
    eil::Array<number> C1 = {static_cast<number>(2.), static_cast<number>(2.), static_cast<number>(0.),
                             static_cast<number>(0.)};
    x0 = {static_cast<number>(0.2), static_cast<number>(0.5), static_cast<number>(-1.1), static_cast<number>(-0.6)};
    int maxIters = 5;
    auto gws = smla::GMRESWorkSpace<number>(x0.length(), maxIters);
    smla::rpcGMRES<number>(h, Ph, C1.all(), x0.all(), x.all(), maxIters, 1.0e-15, gws);
    eil::Array<number> C1_exp = {static_cast<number>(0.273), static_cast<number>(0.773), static_cast<number>(-1.0),
                                 static_cast<number>(-0.682)};

    check(numbers_agree(x.all(), C1_exp.all()), "smla: rpcGMRES with an ILU(0) preconditioner");

    auto Pi = smla::new_smatrix<number>();
    Pi.addRow({static_cast<number>(1.)}, {0});
    Pi.addRow({static_cast<number>(1.)}, {1});
    Pi.addRow({static_cast<number>(1.)}, {2});
    Pi.addRow({static_cast<number>(1.)}, {3});

    x = smla::fGMRES<number>(h, Pi, C1.all(), x0.all(), 5, 3);
    check(numbers_agree(x.all(), C1_exp.all()), "smla: fGMRES with an identity preconditioner");

    // Test pre-conditioned restarted GMRES on the linear system problem
    // outlined in Section III from:
    // Implementation of GMRES for chemically reacting flows by Maclean and White, 2012.
    auto mat_A = smla::new_smatrix<number>(
        {static_cast<number>(1.), static_cast<number>(2.), static_cast<number>(-1.), static_cast<number>(3.), static_cast<number>(2.), static_cast<number>(-1.),
         static_cast<number>(-2.),
         static_cast<number>(2.), static_cast<number>(3.), static_cast<number>(-2.), static_cast<number>(-1.), static_cast<number>(2.), static_cast<number>(4.),
         static_cast<number>(2.),
         static_cast<number>(-2.), static_cast<number>(1.), static_cast<number>(5.), static_cast<number>(-1.), static_cast<number>(-1.), static_cast<number>(6.),
         static_cast<number>(-2.),
         static_cast<number>(-2.), static_cast<number>(3.), static_cast<number>(-1.), static_cast<number>(-1.), static_cast<number>(-5.), static_cast<number>(4.),
         static_cast<number>(3.),
         static_cast<number>(-2.), static_cast<number>(1.), static_cast<number>(2.), static_cast<number>(1.), static_cast<number>(-1.), static_cast<number>(3.),
         static_cast<number>(4.)},
        {0, 1, 5, 0, 1, 2, 6, 1, 2, 3, 7, 2, 3, 4, 8, 3, 4, 9, 0, 5, 1, 5, 6, 7, 2, 6, 7, 8, 3, 7, 8, 9,
         4, 8, 9},
        {0, 3, 7, 11, 15, 18, 20, 24, 28, 32, 35});
    auto pc_A = smla::new_smatrix<number>(mat_A);
    smla::decompILU0<number>(pc_A);
    eil::Array<number> vec_b = {
        static_cast<number>(1.), static_cast<number>(2.), static_cast<number>(3.), static_cast<number>(4.), static_cast<number>(5.),
        static_cast<number>(6.), static_cast<number>(7.), static_cast<number>(8.), static_cast<number>(9.), static_cast<number>(10.)
    };
    eil::Array<number> vec_x0 = {
        static_cast<number>(0.), static_cast<number>(0.), static_cast<number>(0.), static_cast<number>(0.), static_cast<number>(0.),
        static_cast<number>(0.), static_cast<number>(0.), static_cast<number>(0.), static_cast<number>(0.), static_cast<number>(0.)
    };
    eil::Array<number> vec_x = {
        static_cast<number>(0.), static_cast<number>(0.), static_cast<number>(0.), static_cast<number>(0.), static_cast<number>(0.),
        static_cast<number>(0.), static_cast<number>(0.), static_cast<number>(0.), static_cast<number>(0.), static_cast<number>(0.)
    };
    eil::Array<number> vec_ref = {
        static_cast<number>(5.29051), static_cast<number>(-1.20438), static_cast<number>(4.15595), static_cast<number>(2.22681),
        static_cast<number>(0.0574555),
        static_cast<number>(1.88175), static_cast<number>(3.65341), static_cast<number>(2.60547), static_cast<number>(6.66703),
        static_cast<number>(-2.48591)
    };
    int max_iterations = 5;
    int max_restarts = 20;
    auto gws2 = smla::GMRESWorkSpace<number>(vec_x0.length(), max_iterations);
    smla::rpcGMRES<number>(mat_A, pc_A, vec_b.all(), vec_x0.all(), vec_x.all(), max_iterations, 1.0e-14, gws2, max_restarts);
    check(numbers_agree(vec_x.all(), vec_ref.all()), "smla: restarted rpcGMRES, Maclean and White 2012 system");

    // This example tests the addition of values to zero-entries
    auto z = smla::new_smatrix<number>(
        {static_cast<number>(1.), static_cast<number>(2.), static_cast<number>(3.), static_cast<number>(4.),
         static_cast<number>(5.), static_cast<number>(6.), static_cast<number>(7.), static_cast<number>(8.),
         static_cast<number>(9.), static_cast<number>(10.), static_cast<number>(11.), static_cast<number>(12.)},
        {0, 1, 2, 0, 1, 3, 0, 2, 3, 1, 2, 3},
        {0, 3, 6, 9, 12});
    z.set(0, 3) = static_cast<number>(99.0);
    z.set(1, 2) = static_cast<number>(99.0);
    z.set(2, 1) = static_cast<number>(99.0);
    z.set(3, 0) = static_cast<number>(99.0);
    auto w = smla::new_smatrix<number>(
        {static_cast<number>(1.), static_cast<number>(2.), static_cast<number>(3.), static_cast<number>(99.),
         static_cast<number>(4.), static_cast<number>(5.), static_cast<number>(99.), static_cast<number>(6.),
         static_cast<number>(7.), static_cast<number>(99.), static_cast<number>(8.), static_cast<number>(9.),
         static_cast<number>(99.), static_cast<number>(10.), static_cast<number>(11.), static_cast<number>(12.)},
        {0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3},
        {0, 4, 8, 12, 16});
    check(smla::approxEqualMatrix<number>(z, w), "smla: assigning to zero entries shuffles the CRS arrays");

    // Test initialisation from a sparse triplet matrix
    //
    // The file read is the only step here that can throw for a harness reason
    // (bad path), so it gets its own try: D's unittest would surface that as an
    // uncaught FileException from the test runner.
    try {
        auto matrix = stmatrix::readFromMatrixMarketFile<double>(mtx_path);
        auto testMat = smla::new_smatrix<double>(matrix);
        check(eil::nm::approxEqualNumbers(testMat.get(4, 0), -0.03599942, 1.0e-7),
              "smla: CRS row 4 column 0 from the triplet matrix");
        check(eil::nm::approxEqualNumbers(testMat.get(6, 6), 1.0, 1.0e-7),
              "smla: CRS row 6 column 6 from the triplet matrix");
    } catch (const eil::ExceptionBase& e) {
        std::printf("FAIL: smla: cannot read %s: %s\n", mtx_path.c_str(), e.what());
        ++failures;
    }

    // We will test the suite of block sparse matrix methods by solving a linear system using two common iterative solution methods
    size_t blk_size = 2;
    size_t nblks = 5;
    auto A = smla::new_smatrix<number>(
        {static_cast<number>(10.), static_cast<number>(2.), static_cast<number>(-1.),
         static_cast<number>(3.), static_cast<number>(20.), static_cast<number>(-1.), static_cast<number>(-2.),
         static_cast<number>(2.), static_cast<number>(30.), static_cast<number>(-2.), static_cast<number>(-1.),
         static_cast<number>(2.), static_cast<number>(40.), static_cast<number>(2.), static_cast<number>(-2.),
         static_cast<number>(1.), static_cast<number>(50.), static_cast<number>(-1.),
         static_cast<number>(-1.), static_cast<number>(60.),
         static_cast<number>(-2.), static_cast<number>(-2.), static_cast<number>(30.), static_cast<number>(-1.),
         static_cast<number>(-1.), static_cast<number>(-5.), static_cast<number>(40.), static_cast<number>(3.),
         static_cast<number>(-2.), static_cast<number>(1.), static_cast<number>(20.), static_cast<number>(1.),
         static_cast<number>(-1.), static_cast<number>(3.), static_cast<number>(40.)},
        {0, 1, 5, 0, 1, 2, 6, 1, 2, 3, 7, 2, 3, 4, 8, 3, 4, 9, 0, 5, 1, 5, 6, 7, 2, 6, 7, 8, 3, 7, 8, 9,
         4, 8, 9},
        {0, 3, 7, 11, 15, 18, 20, 24, 28, 32, 35});
    eil::Array<number> R = {
        static_cast<number>(1.), static_cast<number>(2.0), static_cast<number>(3.0), static_cast<number>(4.0), static_cast<number>(5.0), static_cast<number>(6.0),
        static_cast<number>(7.0), static_cast<number>(8.0), static_cast<number>(9.0), static_cast<number>(10.0)
    };
    eil::Array<number> sol = {
        static_cast<number>(0.0865856), static_cast<number>(0.117794), static_cast<number>(0.106302), static_cast<number>(0.111582),
        static_cast<number>(0.102159),
        static_cast<number>(0.101443), static_cast<number>(0.254665), static_cast<number>(0.201483), static_cast<number>(0.440107),
        static_cast<number>(0.219546)
    };

    // test multiplying only the block diagonal of a sparse matrix by a vector
    eil::Array<number> DdotR;
    DdotR.set_length(R.length());
    DdotR.fill(static_cast<number>(0.0));
    smla::multiply_block_diagonal(A, R.all(), DdotR.all(), nblks, blk_size);
    eil::Array<number> s0 = {
        static_cast<number>(14.0), static_cast<number>(43.0), static_cast<number>(82.0), static_cast<number>(166.0), static_cast<number>(250.0),
        static_cast<number>(360.0), static_cast<number>(202.0), static_cast<number>(285.0), static_cast<number>(190.0), static_cast<number>(427.0)
    };
    check(numbers_agree(DdotR.all(), s0.all()), "smla: block diagonal multiply");

    // test multiplying only the block lower triangular portion of a sparse matrix by a vector
    eil::Array<number> LdotR;
    LdotR.set_length(R.length());
    LdotR.fill(static_cast<number>(0.0));
    smla::multiply_block_lower_triangular(A, R.all(), LdotR.all(), nblks, blk_size);
    eil::Array<number> s1 = {
        static_cast<number>(0.0), static_cast<number>(0.0), static_cast<number>(4.0), static_cast<number>(0.0), static_cast<number>(4.0),
        static_cast<number>(-1.0), static_cast<number>(-16.0), static_cast<number>(-3.0), static_cast<number>(0.0), static_cast<number>(-5.0)
    };
    check(numbers_agree(LdotR.all(), s1.all()), "smla: block lower triangular multiply");

    // test multiplying only the block upper triangular portion of a sparse matrix by a vector
    eil::Array<number> UdotR;
    UdotR.set_length(R.length());
    UdotR.fill(static_cast<number>(0.0));
    smla::multiply_block_upper_triangular(A, R.all(), UdotR.all(), nblks, blk_size);
    eil::Array<number> s2 = {
        static_cast<number>(-6.0), static_cast<number>(-17.0), static_cast<number>(-8.0), static_cast<number>(-8.0), static_cast<number>(-10.0),
        static_cast<number>(0.0), static_cast<number>(0.0), static_cast<number>(27.0), static_cast<number>(0.0), static_cast<number>(0.0)
    };
    check(numbers_agree(UdotR.all(), s2.all()), "smla: block upper triangular multiply");

    // invert block diagonal in place => A = L + D^(-1) + U
    bbla::Matrix<number> D;
    bbla::Matrix<number> Dinv;
    D = bbla::new_matrix<number>(blk_size, blk_size);
    Dinv = bbla::new_matrix<number>(blk_size, blk_size);
    smla::invert_block_diagonal(A, D, Dinv, nblks, blk_size);

    // initialise data arrays
    eil::Array<number> rhs;
    rhs.set_length(R.length());
    eil::Array<number> x_curr;
    x_curr.set_length(R.length());
    x_curr.fill(static_cast<number>(0.0));

    // perform 8 iterations of the Jacobi method
    x_curr.fill(static_cast<number>(0.0));
    for (size_t k = 0; k < 8; ++k) {
        rhs.fill(static_cast<number>(0.0));
        smla::multiply_block_upper_triangular(A, x_curr.all(), rhs.all(), nblks, blk_size);
        smla::multiply_block_lower_triangular(A, x_curr.all(), rhs.all(), nblks, blk_size);
        for (size_t i = 0; i < rhs.length(); ++i) rhs[i] = R[i] - rhs[i];
        x_curr.fill(static_cast<number>(0.0));
        smla::multiply_block_diagonal(A, rhs.all(), x_curr.all(), nblks, blk_size);
    }
    check(numbers_agree(x_curr.all(), sol.all()), "smla: 8 block Jacobi iterations converge to sol");

    // perform 4 iterations of the Symmetric Gauss-Seidel method
    x_curr.fill(static_cast<number>(0.0));
    for (size_t k = 0; k < 4; ++k) {
        // forward sweep
        rhs.fill(static_cast<number>(0.0));
        smla::multiply_block_upper_triangular(A, x_curr.all(), rhs.all(), nblks, blk_size);
        for (size_t i = 0; i < rhs.length(); ++i) rhs[i] = R[i] - rhs[i];
        smla::block_lower_triangular_solve(A, rhs.all(), x_curr.all(), nblks, blk_size);

        // backward sweep
        rhs.fill(static_cast<number>(0.0));
        smla::multiply_block_lower_triangular(A, x_curr.all(), rhs.all(), nblks, blk_size);
        for (size_t i = 0; i < rhs.length(); ++i) rhs[i] = R[i] - rhs[i];
        smla::block_upper_triangular_solve(A, rhs.all(), x_curr.all(), nblks, blk_size);
    }
    check(numbers_agree(x_curr.all(), sol.all()), "smla: 4 symmetric Gauss-Seidel sweeps converge to sol");

    std::printf("%s: nm.smla unittests (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
