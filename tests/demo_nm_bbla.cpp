// tests/demo_nm_bbla.cpp — port of nm/bbla_demo.d.
//
// bbla_demo.d
//
// Comparison basis: built with the same D sources the gdtk makefile uses
// (`ldc2 -g -dip1008 bbla_demo.d bbla.d ntypes/complex.d`) and diffed against
// /tmp/d_bbla_out.txt, byte for byte.
//
// Two spelling differences, both forced by C++ rather than chosen:
//   * D's `writeln("a=", a)` on a class calls the class's toString() through
//     to!string. eil::writeln only renders numbers, arrays and strings, so the
//     Matrix forms call a.toString() at the print site — the emitted bytes are
//     the same, since D's `to!string(matrix)` is exactly `matrix.toString()`.
//   * D's `new Matrix!double(...)` constructor overloads are the new_matrix<T>()
//     factory (see the note at the top of src/nm/bbla.hpp).

#include <cstdio>
#include <string>

#include "eil/eil.hpp"
#include "nm/bbla.hpp"
#include "nm/number.hpp"

namespace bbla = eil::nm::bbla;

int main() {
    eil::writeln("Bare-bones linear algebra demo...");
    eil::writeln("First, some matrix handling.");
    auto a = bbla::eye<double>(3);
    eil::writeln("a=", a.toString());
    auto b = bbla::zeros<double>(2, 3);
    auto c = bbla::transpose<double>(b);
    b(1, 2) = 99.0;
    c(0, 0) = 1.0; c(1, 1) = 1.0;
    eil::writeln("b=", b.toString());
    eil::writeln("c=", c.toString());

    auto e = bbla::new_matrix<double>({1.0, 2.0, 3.0});
    eil::writeln("e= ", e.toString());
    auto e2 = bbla::new_matrix<double>({1, 2, 3}, "row");
    eil::writeln("e2= ", e2.toString());
    auto f = bbla::new_matrix<double>({{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}});
    eil::writeln("f= ", f.toString());
    eil::writeln("2*f= ", (2*f).toString());
    eil::writeln("f+2= ", (f+2).toString());
    eil::writeln("vstack([f,2*f,3+f])= ", bbla::vstack<double>({f, 2*f, 3+f}).toString());

    auto g = bbla::dot<double>(f, c);
    eil::writeln("g= ", g.toString());

    eil::writeln("Now, try some linear algebra.");
    eil::writeln("Gauss-Jordan elimination");
    auto A = bbla::new_matrix<double>({{0.0,  2.0,  0.0,  1.0},
                                       {2.0,  2.0,  3.0,  2.0},
                                       {4.0, -3.0,  0.0,  1.0},
                                       {6.0,  1.0, -6.0, -5.0}});
    b = bbla::new_matrix<double>({{0.0},
                                  {-2.0},
                                  {-7.0},
                                  {6.0}});
    eil::writeln("A= ", A.toString(), " b= ", b.toString());
    auto Ab = bbla::hstack<double>({A, b});
    eil::writeln("before elimination Ab= ", Ab.toString());
    auto Aonly = Ab.sliceDup(0, 4, 0, 4);
    auto bonly = Ab.sliceDup(0, 4, 4, 5);
    bbla::gaussJordanElimination<double>(Ab);
    eil::writeln("after elimination Ab= ", Ab.toString());
    eil::Array<double> x = Ab.getColumn(4);
    eil::writeln("x= ", x);
    auto rhs = bbla::dot<double>(Aonly, bbla::new_matrix<double>(x.all()));
    eil::writeln("rhs= ", rhs.toString());
    auto residual = rhs - bonly;
    eil::writeln("residual= ", residual.toString());

    eil::writeln("LU decomposition");
    c = bbla::new_matrix<double>(A);
    auto perm = bbla::decomp<double>(c);
    eil::writeln("after decomp, c= ", c.toString());
    eil::writeln("perm= ", perm);
    bbla::solve<double>(c, b, perm.all());
    eil::writeln("solution vector= ", b.toString());

    auto y = bbla::inverse<double>(A);
    eil::writeln("dot(A,inverse(A))= ", bbla::dot<double>(A, y).toString());

    eil::writeln("Least-squares solve of an over-constrained system.");
    A = bbla::new_matrix<double>({{0.0,  2.0,  0.0,  1.0},
                                  {2.0,  2.0,  3.0,  2.0},
                                  {4.0,  4.0,  6.0,  4.0},
                                  {4.0, -3.0,  0.0,  1.0},
                                  {4.0, -3.0,  0.0,  1.0},
                                  {6.0,  1.0, -6.0, -5.0}});
    b = bbla::new_matrix<double>({{0.0},
                                  {-2.0},
                                  {-4.0},
                                  {-7.0},
                                  {-7.0},
                                  {6.0}});
    eil::writeln("A= ", A.toString(), " b= ", b.toString());
    auto xx = bbla::lsqsolve<double>(A, b);
    eil::writeln("xx=", xx.toString());

    eil::writeln("Done.");
    return 0;
}
