#pragma once
/**
 * rsla: real simple linear algebra
 * A linear equation solver using the text-book notation.
 *
 * PJ 2016-01-16 factored out of the gradient calculation module for eilmer
 */
// nm/rsla.d -> src/nm/rsla.hpp (module nm.rsla).
//
// Naming. rsla's `solveWithInverse`/`computeInverse` do not collide with nm/bbla
// or nm/smla, but they share the module-scope namespace convention anyway:
// namespace eil::nm::rsla.
//
// Storage. Every entry point takes D's `ref T[NDIM2][NDIM] c`, a reference to a
// static array of static arrays. In D the type `T[N][M]` reads right-to-left, so
// `T[NDIM2][NDIM]` is NDIM rows of NDIM2 elements -- `T (&c)[NDIM][NDIM2]` in
// C++, and a `T[8][4]` matrix in D is a `T[4][8]` in C++. The template parameter
// order is kept as D declares it: <N, NDIM, NDIM2, T>.
//
// D's `static if (N == 1) {...}` blocks become `if constexpr` blocks, kept as
// separate statements (not an else-if chain) so the structure matches
// rsla.d:46-110 line for line.
//
// Printing. rsla.d:130 and :139 print D static arrays (which render as
// "[[0, 1, 2], [3, 4, 5]]" -- verified against Phobos with a probe program, and
// matching the %s array rules in tools/fmt_corpus.txt). C arrays have no
// length()/operator[] for eil::to_string, so printable() mirrors the values into
// nested eil::Array objects purely to render the same text.

#include <cmath>
#include <cstddef>
#include <string>
#include <utility>

#include "eil/array.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/math.hpp"
#include "eil/number.hpp"
#include "nm/number.hpp"

// rsla.d:10-14 `import std.math; std.format; std.conv; std.stdio; ntypes.complex;`
// ntypes/complex.d is only imported to let these routines compile against
// number == Complex!double under version(complex_numbers) (wave 12).

namespace eil::nm {
namespace rsla {

namespace detail {

// Render a C 2-D array the way D renders `T[COLS][ROWS]`.
template <typename T, size_t ROWS, size_t COLS>
eil::Array<eil::Array<T>> printable(T (&c)[ROWS][COLS]) {
    eil::Array<eil::Array<T>> out;
    for (size_t i = 0; i < ROWS; ++i) {
        eil::Array<T> row;
        for (size_t j = 0; j < COLS; ++j) row.append(c[i][j]);
        out.append(std::move(row));
    }
    return out;
}

} // namespace detail

// rsla.d:16-28
template <size_t N, size_t NDIM, size_t NDIM2, typename T>
T normInf(T (&c)[NDIM][NDIM2])
// Return the max-absolute-row-sum of an augmented matrix c = [A|b]
// We are concerned with a measure of A only.
{
    T norm = 0.0;
    for (size_t i = 0; i < N; ++i) {
        T rowsum = 0.0;
        for (size_t j = 0; j < N; ++j) { rowsum += std::fabs(c[i][j]); }
        norm = std::fmax(rowsum, norm);
    }
    return norm;
} // end normInf()()

// rsla.d:30-112
template <size_t N, size_t NDIM, size_t NDIM2, typename T>
int computeInverse(T (&c)[NDIM][NDIM2], double very_small_value = 1.0e-16)
// Perform Gauss-Jordan elimination on an augmented matrix.
// c = [A|b] such that the mutated matrix becomes [I|x]
// where x is the solution vector(s) to A.x = b
// When computing an inverse, the incoming data is assumed to be c=[A|I].
//
// Returns 0 normally, -1 if the matrix is essentially singular (zero pivot).
//
// We allow a larger storage than needed because the flow solver code will
// use these functions for both 2D and 3D simulations.
// When N < NDIM, the solver just uses a subset of the available storage
// and the rest remains untouched.
{
    eil_assert(NDIM >= N);   // "Inadequate size of dimension for matrix"
    if constexpr (N == 0) {
        eil_assert(false);   // "Zero dimension linear system doesn't make sense."
    }
    if constexpr (N == 1) {
        T det = c[0][0];
        if (std::fabs(det) <= very_small_value) return -1; // singular
        c[0][1] = 1.0/det; // inverse
        c[0][0] = 1.0; // identity
    }
    if constexpr (N == 2) {
        T det = c[0][0]*c[1][1] - c[0][1]*c[1][0];
        if (std::fabs(det) <= very_small_value) return -1; // singular
        // compute inverse directly
        T one_over_det = 1.0/det;
        c[0][2] =  c[1][1]*one_over_det; c[0][3] = -c[0][1]*one_over_det;
        c[1][2] = -c[1][0]*one_over_det; c[1][3] =  c[0][0]*one_over_det;
        // overwrite original elements with identity
        c[0][0] = 1.0; c[0][1] = 0.0;
        c[1][0] = 0.0; c[1][1] = 1.0;
    }
    if constexpr (N == 3) {
        T det = c[0][0]*(c[1][1]*c[2][2] - c[1][2]*c[2][1])
            - c[0][1]*(c[1][0]*c[2][2] - c[1][2]*c[2][0])
            + c[0][2]*(c[1][0]*c[2][1] - c[1][1]*c[2][0]);
        if (std::fabs(det) <= very_small_value) return -1; // singular
        // compute inverse directly
        T one_over_det = 1.0/det;
        c[0][3] = (c[1][1]*c[2][2] - c[1][2]*c[2][1])*one_over_det;
        c[0][4] = (c[0][2]*c[2][1] - c[0][1]*c[2][2])*one_over_det;
        c[0][5] = (c[0][1]*c[1][2] - c[0][2]*c[1][1])*one_over_det;
        c[1][3] = (c[1][2]*c[2][0] - c[1][0]*c[2][2])*one_over_det;
        c[1][4] = (c[0][0]*c[2][2] - c[0][2]*c[2][0])*one_over_det;
        c[1][5] = (c[0][2]*c[1][0] - c[0][0]*c[1][2])*one_over_det;
        c[2][3] = (c[1][0]*c[2][1] - c[1][1]*c[2][0])*one_over_det;
        c[2][4] = (c[0][1]*c[2][0] - c[0][0]*c[2][1])*one_over_det;
        c[2][5] = (c[0][0]*c[1][1] - c[0][1]*c[1][0])*one_over_det;
        // overwrite original elements with identity
        c[0][0] = 1.0; c[0][1] = 0.0; c[0][2] = 0.0;
        c[1][0] = 0.0; c[1][1] = 1.0; c[1][2] = 0.0;
        c[2][0] = 0.0; c[2][1] = 0.0; c[2][2] = 1.0;
    }
    if constexpr (N > 3) {
        for (size_t j = 0; j < N; ++j) {
            // Select pivot.
            size_t p = j;
            for (size_t i = j+1; i < N; ++i) {
                if ( std::fabs(c[i][j]) > std::fabs(c[p][j]) ) p = i;
            }
            if (std::fabs(c[p][j]) <= very_small_value) return -1; // singular
            if ( p != j ) { // Swap rows
                for (size_t col = 0; col < 2*N; ++col) {
                    T tmp = c[p][col]; c[p][col] = c[j][col]; c[j][col] = tmp;
                }
            }
            // Scale row j to get unity on the diagonal.
            T cjj = c[j][j];
            for (size_t col = 0; col < 2*N; ++col) c[j][col] /= cjj;
            // Do the elimination to get zeros in all off diagonal values in column j.
            for (size_t i = 0; i < N; ++i) {
                if ( i == j ) continue;
                T cij = c[i][j];
                for (size_t col = 0; col < 2*N; ++col) c[i][col] -= cij * c[j][col];
            }
        } // end foreach j
    } // end static if N > 3
    return 0; // success
} // end computeInverse()()

// rsla.d:114-159. Not @nogc in D because it formats diagnostic text; the port
// keeps the same two messages and the same array renderings.
template <size_t N, size_t NDIM, size_t NDIM2, typename T>
int computeInverseDebug(T (&c)[NDIM][NDIM2], double very_small_value = 1.0e-16)
// Perform Gauss-Jordan elimination on an augmented matrix.
// c = [A|b] such that the mutated matrix becomes [I|x]
// where x is the solution vector(s) to A.x = b
// When computing an inverse, the incoming data is assumed to be c=[A|I].
//
// Returns 0 normally, -1 if the matrix is essentially singular (zero pivot).
//
// This debug version gives more information.
{
    eil_assert(NDIM >= N);   // "Inadequate size of dimension for matrix"
    // D default-initialises the static array to NaN; every element is written
    // below before anything reads it, so the starting value is not observable.
    T csave[N][2*N] = {};
    for (size_t j = 0; j < N; ++j) {
        for (size_t col = 0; col < 2*N; ++col) csave[j][col] = c[j][col];
    }
    eil::writeln("At start of computeInverseDebug, c=", detail::printable(c));
    for (size_t j = 0; j < N; ++j) {
        // Select pivot.
        size_t p = j;
        for (size_t i = j+1; i < N; ++i) {
            if ( std::fabs(c[i][j]) > std::fabs(c[p][j]) ) p = i;
        }
        if (std::fabs(c[p][j]) <= very_small_value) {
            std::string msg = eil::format(" matrix is essentially singular j=%d p=%d", j, p)
                + " \nc=" + eil::to_string(detail::printable(c))
                + " \ncsave=" + eil::to_string(detail::printable(csave));
            eil::writeln(msg);
            return -1; // singular
        }
        if ( p != j ) { // Swap rows
            for (size_t col = 0; col < 2*N; ++col) {
                T tmp = c[p][col]; c[p][col] = c[j][col]; c[j][col] = tmp;
            }
        }
        // Scale row j to get unity on the diagonal.
        T cjj = c[j][j];
        for (size_t col = 0; col < 2*N; ++col) c[j][col] /= cjj;
        // Do the elimination to get zeros in all off diagonal values in column j.
        for (size_t i = 0; i < N; ++i) {
            if ( i == j ) continue;
            T cij = c[i][j];
            for (size_t col = 0; col < 2*N; ++col) c[i][col] -= cij * c[j][col];
        }
    } // end foreach j
    return 0; // success
} // end computeInverseDebug()()

// rsla.d:161-191
template <size_t N, size_t NDIM, size_t NDIM2, typename T>
void solveWithInverse(T (&c)[NDIM][NDIM2], T (&rhs)[NDIM], T (&x)[NDIM])
// Multiply right-hand-side by the inverse part of the augmented matrix.
// Augmented matrix is assumed to be c=[I|Ainv]
{
    eil_assert(NDIM >= N);   // "Inadequate size of dimension for matrix"
    if constexpr (N == 0) {
        eil_assert(false);   // "Zero dimension linear system doesn't make sense."
    }
    if constexpr (N == 1) {
        x[0] = c[0][1] * rhs[0];
    }
    if constexpr (N == 2) {
        x[0] = c[0][2]*rhs[0] + c[0][3]*rhs[1];
        x[1] = c[1][2]*rhs[0] + c[1][3]*rhs[1];
    }
    if constexpr (N == 3) {
        x[0] = c[0][3]*rhs[0] + c[0][4]*rhs[1] + c[0][5]*rhs[2];
        x[1] = c[1][3]*rhs[0] + c[1][4]*rhs[1] + c[1][5]*rhs[2];
        x[2] = c[2][3]*rhs[0] + c[2][4]*rhs[1] + c[2][5]*rhs[2];
    }
    if constexpr (N > 3) {
        for (size_t i = 0; i < N; ++i) {
            x[i] = 0.0;
            for (size_t j = 0; j < N; ++j) {
                x[i] += c[i][N+j] * rhs[j];
            }
        }
    } // end static if N > 3
} // end solveWithInverse()()

} // namespace rsla
} // namespace eil::nm
