#pragma once
/**
 * A simple container for a sparse triplet matrix.
 *
 * Author: Rowan J. Gollan
 * Date: 2020-03-25
 *
 */
// nm/stmatrix.d -> src/nm/stmatrix.hpp (module nm.stmatrix).
//
// D module scope -> namespace eil::nm::stmatrix.
//
// Type mapping. D's key type is `Tuple!(size_t, size_t)` (std.typecons), used as
// an associative-array key and indexed as `c[0]`/`c[1]`. std::pair<size_t,size_t>
// is the same data, ordered (which is what eil::AA needs, and what
// D's hash-table iteration is not), and indexes as `.first`/`.second`.
//
// Reference semantics. STMatrix is a D class, so a variable is a reference. The
// element table is held behind a std::shared_ptr and exposed through val(),
// because nm/smla.d's SMatrix(STMatrix!T) constructor consumes the source matrix
// (it removes entries through stMat.val as each row is assembled,
// stmatrix-unittest path), which must show through to the caller's object.
// n_rows/n_cols/n_nzeros stay plain: nothing in D resizes them.
//
// stmatrix.d:11 imports core.stdc.stdlib:exit but the module never calls it, so
// there is no C++ counterpart to add.

#include <cerrno>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <utility>

#include "eil/aa.hpp"
#include "eil/array.hpp"
#include "eil/conv.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/number.hpp"
#include "nm/number.hpp"

namespace eil::nm {
namespace stmatrix {

// stmatrix.d:22 `T[Tuple!(size_t, size_t)] val;`
template <typename T> using TripletKey = std::pair<size_t, size_t>;
template <typename T> using TripletTable = eil::AA<TripletKey<T>, T>;

// Non-template text helpers used by the reader; defined in stmatrix.cpp.
// They stand for std.string's strip() and split() as the reference uses them:
// strip removes leading and trailing whitespace, split cuts on runs of
// whitespace and drops empty pieces.
std::string strip(const std::string& s);
eil::Array<std::string> split(const std::string& s);

// stmatrix.d:19-30 `class STMatrix(T)`
template <typename T> class STMatrix {
  public:
    // stmatrix.d:24-29 `this(size_t _n_rows, size_t _n_cols, size_t _n_nzeros)`
    STMatrix(size_t _n_rows, size_t _n_cols, size_t _n_nzeros)
        : n_rows(_n_rows), n_cols(_n_cols), n_nzeros(_n_nzeros),
          _val(std::make_shared<TripletTable<T>>()) {}

    STMatrix(const STMatrix&) = default;       // aliases, as a D class reference does
    STMatrix& operator=(const STMatrix&) = default;

    // stmatrix.d:21 `size_t n_rows, n_cols, n_nzeros;`
    size_t n_rows, n_cols, n_nzeros;

    // stmatrix.d:22, the value table. D writes `matrix.val[k] = v` to insert and
    // `matrix.val[k]` to read (throwing "Range violation" when absent); eil::AA
    // spells those val().set(k, v) and val().get(k).
    TripletTable<T>& val() const { return *_val; }

  private:
    std::shared_ptr<TripletTable<T>> _val;
}; // end class STMatrix

// stmatrix.d:32-60 `STMatrix!T readFromMatrixMarketFile(T)(string fName)`.
//
// D opens the file with std.stdio.File; C++ uses std::ifstream here because the
// substrate has no File type. The line-by-line contract is the same: discard
// comment lines until the size line, take 3 fields from it, then take
// i j v triples, converting 1-based indices to 0-based.
template <typename T>
STMatrix<T> readFromMatrixMarketFile(const std::string& fName);

// The definition needs the file-reading helpers only, so it stays in the header
// (T appears solely in `to!T`, per stmatrix.d:55).
template <typename T>
STMatrix<T> readFromMatrixMarketFile(const std::string& fName) {
    // D opens with `auto f = File(fName, "r")` (stmatrix.d:34), which raises
    // std.exception.ErrnoException on an unusable path. Measured text for an absent file:
    //   Cannot open file `/tmp/stm/does_not_exist.mtx' in mode `r' (No such file or directory)
    // The substrate has no ErrnoException yet, so -- exactly as
    // src/geom/surface/parametricsurface.cpp:58-65 does for mode `w' -- the message is
    // rebuilt from strerror and the class stays ExceptionBase. An earlier version of this
    // line threw `Failed to open "name"', a text D never prints.
    std::ifstream f(fName.c_str());
    if (!f) {
        const int saved_errno = errno;
        throw eil::ExceptionBase("Cannot open file `" + fName + "' in mode `r' ("
                                 + std::strerror(saved_errno) + ")", __FILE__, __LINE__);
    }
    // Keep throwing away comment lines until we find information about the size and number of entries.
    // D never tests for end of file here: readln() at EOF yields an empty string, and the very
    // next statement `line[0]` raises. Measured for an empty file and for a comments-only file
    // (baselines/e587b235/probes/nm_stmatrix_fails.txt):
    //   class=core.exception.ArrayIndexError  msg=[index [0] is out of bounds for array of length 0]
    // so a failed getline must fall through to the same out-of-bounds raise rather than into an
    // invented "Unexpected EOF" exception (backlog T-923).
    std::string line;
    for (;;) {
        std::getline(f, line);
        line = strip(line);
        if (line.length() == 0) throw eil::RangeError(0, 0);   // D: line[0] on ""
        if (line[0] != '%') break;
    }
    // Read numer of rows, colums and non-zero entries
    auto tks = split(line);
    auto n_rows = eil::to<size_t>(tks[0]);
    auto n_cols = eil::to<size_t>(tks[1]);
    auto n_nzeros = eil::to<size_t>(tks[2]);
    STMatrix<T> matrix(n_rows, n_cols, n_nzeros);
    // Now read all entries.
    // The MatrixMarket format allows duplicate entries for (i,j) BUT this code is specifically built for files from
    // the SuiteSparse collection. These matrices do NOT have duplicates, so we won't handle that case here.
    for (size_t k = 0; k < n_nzeros; ++k) {
        // Same as D, which does not test the stream here either: a file that declares more
        // entries than it holds makes readln() yield "", split() gives a zero-length array and
        // `tks[0]` raises. Measured for a header claiming 2 entries with 1 row present:
        //   class=core.exception.ArrayIndexError  msg=[index [0] is out of bounds for array of length 0]
        std::getline(f, line);
        line = strip(line);
        tks = split(line);
        auto i = eil::to<size_t>(tks[0]) - 1; // Change 1- to 0-offset
        auto j = eil::to<size_t>(tks[1]) - 1; // Change 1- to 0-offset
        auto v = eil::to<T>(tks[2]);
        matrix.val().set(TripletKey<T>(i, j), v);
    }
    f.close();
    return matrix;
}

} // namespace stmatrix
} // namespace eil::nm
