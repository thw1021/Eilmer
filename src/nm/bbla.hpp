#pragma once
/**
 * bbla.d
 * Bare-bones linear algebra functions.
 *
 * Author: Peter J. and Rowan G.
 * Version: 2014-06-28, just enough to get our CFD code going.
 *          2015-05-30, added least-squares solver
 *          2018-04-21, added Crout's reduction for LU decomposition
 */
// nm/bbla.d -> src/nm/bbla.hpp (module nm.bbla).
//
// Naming. D keeps `dot`, `solve`, `decomp`, `inverse`, `copy`, `zeros`, `ones`,
// `eye`, `transpose` and `approxEqualMatrix` apart by module, but nm/smla.d
// declares the same names for its sparse types. Following the convention set in
// nm/brent.hpp, the D module scope becomes namespace eil::nm::bbla: D's
// `nm.bbla.dot(...)` is `bbla::dot(...)`, and a call site D leaves unqualified
// is either qualified or pulled in with a `using`.
//
// Reference semantics. `Matrix(T)` is a D *class*, so a Matrix variable is a
// reference: `b = a` aliases; `in Matrix!T m` still allows element writes (D's
// const is shallow for classes); and a by-value parameter such as LUDecomp's
// `Matrix!T a` (bbla.d:989) mutates the caller's object -- the unittest comment
// at bbla.d:1095, "Reset A since it was converted to LU format", exists only
// because of that. Modelled by sharing the element buffer through a
// std::shared_ptr and keeping the dimensions as plain members (Matrix has no
// resize in D, so the dimensions of an object never change). Copying a Matrix
// therefore aliases, as in D; the deep copy D writes `new Matrix!T(a)` is the
// named factory `new_matrix<T>(a)`.
//
// `new Matrix!T(...)` maps to `new_matrix<T>(...)`. The forms D takes through
// constructor overloads on array literals need the factory because C++ cannot
// spell a nested array literal in a constructor call; D's separate
// `this(in float[] vec, ...)` and `this(in int[] vec, ...)` overloads
// (bbla.d:56-68) collapse into the initializer_list form, since C++ converts
// those literal elements to T at the call site with the same values.
//
// A default-constructed Matrix models D's null handle as a valid 0x0 matrix, so
// an accidental element read before assignment raises eil::RangeError where D
// raises a pointer error, rather than being undefined behaviour.
//
// D's `in (cond, "msg")` contracts become eil_assert(cond) with the contract
// text in a comment (eil_assert carries only the condition). `enforce(...)`
// becomes eil::enforce, and `throw new Error(m)` / `throw new Exception(m)`
// become throws by value of eil::ErrorBase / eil::ExceptionBase.
//
// bbla.d is templates throughout (class Matrix(T) plus template functions), so
// the definitions live in this header and bbla.cpp is the matching TU; see
// src/nm/brent.hpp for the same arrangement.

#include <array>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>

#include "eil/array.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/math.hpp"
#include "eil/number.hpp"
#include "nm/number.hpp"

// bbla.d:13-19 `import std.conv; std.algorithm; std.math; ntypes.complex;
// std.exception; nm.number;`. ntypes/complex.d is not ported yet; outside its
// version(complex_numbers) branch it collapses to eil::Complex<double>, which is
// what the guarded blocks below assume (see eil/number.hpp:29-33).

namespace eil::nm {
namespace bbla {

// D's `to!T(0.0)` for a numeric T is the value conversion. Going through
// eil::to would stringify and reparse every element of zeros()/eye(), so the
// identity is used; the results are identical for the numeric types the trunk
// build instantiates.
template <typename T> inline T to_T(double v) { return static_cast<T>(v); }

template <typename T> class Matrix;

// The `new Matrix!T(...)` forms, declared here so member bodies defined below
// can use them (D sees all module-scope names; a C++ template body does not).
template <typename T> Matrix<T> new_matrix(size_t n);
template <typename T> Matrix<T> new_matrix(size_t nrows, size_t ncols);
template <typename T> Matrix<T> new_matrix(const Matrix<T>& other);
template <typename T>
Matrix<T> new_matrix(std::initializer_list<T> vec, const std::string& orient = "column");
template <typename T>
Matrix<T> new_matrix(std::initializer_list<std::initializer_list<T>> other);
template <typename T>
Matrix<T> new_matrix(eil::Slice<T> vec, const std::string& orient = "column");

template <typename T> class Matrix {
  public:
    // D's `Matrix!T m;` is the null handle; here a valid 0x0 matrix.
    Matrix() : _nrows(0), _ncols(0), _data(std::make_shared<eil::Array<T>>(0)) {}

    // bbla.d:26-30 `this(size_t n)`
    explicit Matrix(size_t n)
        : _nrows(n), _ncols(n), _data(std::make_shared<eil::Array<T>>(n*n)) {}

    // bbla.d:32-36 `this(size_t nrows, size_t ncols)`
    Matrix(size_t nrows, size_t ncols)
        : _nrows(nrows), _ncols(ncols),
          _data(std::make_shared<eil::Array<T>>(nrows*ncols)) {}

    // Copying a D class reference aliases. The deep copy is new_matrix(other),
    // which is bbla.d:38-43 `this(in Matrix other)`.
    Matrix(const Matrix&) = default;
    Matrix& operator=(const Matrix&) = default;
    Matrix(Matrix&&) = default;
    Matrix& operator=(Matrix&&) = default;

    // bbla.d:91-92 `@property @nogc const size_t nrows()` / `ncols()`
    size_t nrows() const { return _nrows; }
    size_t ncols() const { return _ncols; }

    // Thinking that we get only one level of inlining,
    // let's do the index calculation explicitly for each
    // of the following index methods.
    //
    // bbla.d:98-105 `@nogc size_t index(size_t row, size_t col) const`
    //   in (row < _nrows, "row out of bounds")
    //   in (col < _ncols, "col out of bounds")
    size_t index(size_t row, size_t col) const {
        eil_assert(row < _nrows);   // "row out of bounds"
        eil_assert(col < _ncols);   // "col out of bounds"
        return row*_ncols + col;
    }

    // bbla.d:107-144: opIndex (bbla.d:107), opIndexAssign (bbla.d:116) and
    // opIndexOpAssign (bbla.d:127) all address `_data[row*_ncols + col]` with
    // the same contracts, so one accessor returning T& covers the three D
    // spellings; D's opIndex returns `const T` by value and its assign forms
    // return a ref.
    T& operator()(size_t row, size_t col) const {
        eil_assert(row < _nrows);   // "row out of bounds"
        eil_assert(col < _ncols);   // "col out of bounds"
        return (*_data)[row*_ncols + col];
    }

    // bbla.d:147-168 `Matrix!T opBinary(string op)(in Matrix rhs)`, written out
    // per operation the way D's static if resolves. Element-by-element: "*" is
    // the Hadamard product, not a matrix product (that is dot()).
    Matrix operator+(const Matrix& rhs) const {
        eil::enforce(_nrows == rhs._nrows && _ncols == rhs._ncols, "incompatible matrices");
        Matrix result = new_matrix<T>(_nrows, _ncols);
        for (size_t row = 0; row < _nrows; ++row) {
            for (size_t col = 0; col < _ncols; ++col) {
                size_t i = index(row, col);
                (*result._data)[i] = (*_data)[i] + (*rhs._data)[i];
            }
        }
        return result;
    }

    Matrix operator-(const Matrix& rhs) const {
        eil::enforce(_nrows == rhs._nrows && _ncols == rhs._ncols, "incompatible matrices");
        Matrix result = new_matrix<T>(_nrows, _ncols);
        for (size_t row = 0; row < _nrows; ++row) {
            for (size_t col = 0; col < _ncols; ++col) {
                size_t i = index(row, col);
                (*result._data)[i] = (*_data)[i] - (*rhs._data)[i];
            }
        }
        return result;
    }

    Matrix operator*(const Matrix& rhs) const {
        eil::enforce(_nrows == rhs._nrows && _ncols == rhs._ncols, "incompatible matrices");
        Matrix result = new_matrix<T>(_nrows, _ncols);
        for (size_t row = 0; row < _nrows; ++row) {
            for (size_t col = 0; col < _ncols; ++col) {
                size_t i = index(row, col);
                (*result._data)[i] = (*_data)[i] * (*rhs._data)[i];
            }
        }
        return result;
    }

    Matrix operator/(const Matrix& rhs) const {
        eil::enforce(_nrows == rhs._nrows && _ncols == rhs._ncols, "incompatible matrices");
        Matrix result = new_matrix<T>(_nrows, _ncols);
        for (size_t row = 0; row < _nrows; ++row) {
            for (size_t col = 0; col < _ncols; ++col) {
                size_t i = index(row, col);
                (*result._data)[i] = (*_data)[i] / (*rhs._data)[i];
            }
        }
        return result;
    }

    // bbla.d:170-189 `Matrix!T opBinary(string op)(double rhs)`
    Matrix operator+(double rhs) const {
        Matrix result = new_matrix<T>(_nrows, _ncols);
        for (size_t row = 0; row < _nrows; ++row) {
            for (size_t col = 0; col < _ncols; ++col) {
                size_t i = index(row, col);
                (*result._data)[i] = (*_data)[i] + rhs;
            }
        }
        return result;
    }

    Matrix operator-(double rhs) const {
        Matrix result = new_matrix<T>(_nrows, _ncols);
        for (size_t row = 0; row < _nrows; ++row) {
            for (size_t col = 0; col < _ncols; ++col) {
                size_t i = index(row, col);
                (*result._data)[i] = (*_data)[i] - rhs;
            }
        }
        return result;
    }

    Matrix operator*(double rhs) const {
        Matrix result = new_matrix<T>(_nrows, _ncols);
        for (size_t row = 0; row < _nrows; ++row) {
            for (size_t col = 0; col < _ncols; ++col) {
                size_t i = index(row, col);
                (*result._data)[i] = (*_data)[i] * rhs;
            }
        }
        return result;
    }

    Matrix operator/(double rhs) const {
        Matrix result = new_matrix<T>(_nrows, _ncols);
        for (size_t row = 0; row < _nrows; ++row) {
            for (size_t col = 0; col < _ncols; ++col) {
                size_t i = index(row, col);
                (*result._data)[i] = (*_data)[i] / rhs;
            }
        }
        return result;
    }

#ifdef complex_numbers
    // bbla.d:191-212 `version(complex_numbers) Matrix!T opBinary(string op)
    // (Complex!double rhs)`. Kept as written; the trunk build never defines
    // complex_numbers.
    // needs full ntypes/complex.d port (wave 12): eil::Complex<double> is the
    // re/im carrier only and has none of the arithmetic operators these bodies
    // use, so this branch does not compile today and is not compiled in trunk.
    Matrix operator+(const eil::Complex<double>& rhs) const {
        Matrix result = new_matrix<T>(_nrows, _ncols);
        for (size_t row = 0; row < _nrows; ++row) {
            for (size_t col = 0; col < _ncols; ++col) {
                size_t i = index(row, col);
                (*result._data)[i] = (*_data)[i] + rhs;
            }
        }
        return result;
    }

    Matrix operator-(const eil::Complex<double>& rhs) const {
        Matrix result = new_matrix<T>(_nrows, _ncols);
        for (size_t row = 0; row < _nrows; ++row) {
            for (size_t col = 0; col < _ncols; ++col) {
                size_t i = index(row, col);
                (*result._data)[i] = (*_data)[i] - rhs;
            }
        }
        return result;
    }

    Matrix operator*(const eil::Complex<double>& rhs) const {
        Matrix result = new_matrix<T>(_nrows, _ncols);
        for (size_t row = 0; row < _nrows; ++row) {
            for (size_t col = 0; col < _ncols; ++col) {
                size_t i = index(row, col);
                (*result._data)[i] = (*_data)[i] * rhs;
            }
        }
        return result;
    }

    Matrix operator/(const eil::Complex<double>& rhs) const {
        Matrix result = new_matrix<T>(_nrows, _ncols);
        for (size_t row = 0; row < _nrows; ++row) {
            for (size_t col = 0; col < _ncols; ++col) {
                size_t i = index(row, col);
                (*result._data)[i] = (*_data)[i] / rhs;
            }
        }
        return result;
    }
#endif

    // bbla.d:214-229 `void opOpAssign(string op)(in Matrix rhs)` for "+" and "-"
    // only. D's enforce text is "incompatible matrices".
    void operator+=(const Matrix& rhs) const {
        eil::enforce(_nrows == rhs._nrows && _ncols == rhs._ncols, "incompatible matrices");
        for (size_t row = 0; row < _nrows; ++row) {
            for (size_t col = 0; col < _ncols; ++col) {
                (*_data)[row*_ncols + col] += (*rhs._data)[row*_ncols + col];
            }
        }
        return;
    }

    void operator-=(const Matrix& rhs) const {
        eil::enforce(_nrows == rhs._nrows && _ncols == rhs._ncols, "incompatible matrices");
        for (size_t row = 0; row < _nrows; ++row) {
            for (size_t col = 0; col < _ncols; ++col) {
                (*_data)[row*_ncols + col] -= (*rhs._data)[row*_ncols + col];
            }
        }
        return;
    }

    // bbla.d:271-277 `@nogc void scale(double s)`
    void scale(double s) const {
        for (size_t row = 0; row < _nrows; ++row)
            for (size_t col = 0; col < _ncols; ++col)
                (*_data)[index(row, col)] *= s;
    }

    // bbla.d:279-287 `@nogc void add(in Matrix rhs)`
    void add(const Matrix& rhs) const {
        for (size_t row = 0; row < _nrows; ++row)
            for (size_t col = 0; col < _ncols; ++col) {
                size_t i = index(row, col);
                (*_data)[i] += (*rhs._data)[i];
            }
    }

    // bbla.d:289-302 `override string toString()`
    std::string toString() const {
        std::string s = "Matrix["; // [TODO] add string form of type T here
        for (size_t row = 0; row < _nrows; ++row) {
            s += "[";
            for (size_t col = 0; col < _ncols; ++col) {
                s += eil::to_string((*_data)[index(row, col)]);
                if (col < _ncols-1) s += ",";
            }
            s += "]";
            if (row < _nrows-1) s += ",";
        }
        s += "]";
        return s;
    }

    // bbla.d:304-308 `@nogc void swapRows(size_t i1, size_t i2)`
    void swapRows(size_t i1, size_t i2) const {
        for (size_t col = 0; col < _ncols; ++col)
            std::swap((*_data)[index(i1, col)], (*_data)[index(i2, col)]);
    }

    // bbla.d:310-315 `T[] getColumn(size_t col)`
    eil::Array<T> getColumn(size_t col) const {
        eil::Array<T> my_column;
        my_column.set_length(nrows());
        for (size_t row = 0; row < nrows(); ++row) {
            my_column[row] = (*_data)[index(row, col)];
        }
        return my_column;
    }

    // bbla.d:317-321 `void getColumn(T[] my_column, size_t col)` -- D's out
    // parameter is a view of the caller's array.
    void getColumn(eil::Slice<T> my_column, size_t col) const {
        eil_assert(my_column.length() == nrows());  // "Incorrect column length."
        for (size_t row = 0; row < nrows(); ++row) {
            my_column[row] = (*_data)[index(row, col)];
        }
        return;
    }

    // bbla.d:323-327 `void setColumn(T[] my_column, size_t col)`
    void setColumn(eil::Slice<T> my_column, size_t col) const {
        eil_assert(my_column.length() == nrows());  // "Incorrect column length."
        for (size_t row = 0; row < nrows(); ++row) {
            (*_data)[index(row, col)] = my_column[row];
        }
        return;
    }

    // bbla.d:329-334 `T[] getRow(size_t row)`
    eil::Array<T> getRow(size_t row) const {
        eil::Array<T> my_row;
        my_row.set_length(ncols());
        for (size_t col = 0; col < ncols(); ++col) my_row[col] = (*_data)[index(row, col)];
        return my_row;
    }

    // Maybe there is a way of using built-in slices here.
    //
    // bbla.d:336-349 `Matrix!T sliceDup(...)`. D's second enforce argument is
    // `new Error("invalid subrange")`, so the failure type is Error, not
    // Exception.
    Matrix sliceDup(size_t row0, size_t row1, size_t col0, size_t col1) const {
        if (!((row0 < row1) && (row1 <= this->_nrows) &&
              (col0 < col1) && (col1 <= this->_ncols))) {
            throw eil::ErrorBase("invalid subrange");
        }
        Matrix sub_matrix = new_matrix<T>(row1-row0, col1-col0);
        for (size_t row = row0; row < row1; ++row) {
            for (size_t col = col0; col < col1; ++col) {
                sub_matrix(row-row0, col-col0) = (*_data)[index(row, col)];
            }
        }
        return sub_matrix;
    }

    // bbla.d:351-361 `void sliceAssign(T c, ...)`
    void sliceAssign(T c, size_t row0, size_t row1, size_t col0, size_t col1) const {
        if (!((row0 < row1) && (row1 <= this->_nrows) &&
              (col0 < col1) && (col1 <= this->_ncols))) {
            throw eil::ErrorBase("invalid subrange");
        }
        for (size_t row = row0; row < row1; ++row) {
            for (size_t col = col0; col < col1; ++col) {
                (*_data)[index(row, col)] = c;
            }
        }
    }

    // bbla.d:363-369 `@nogc void zeros()`
    void zeros() const {
        for (size_t row = 0; row < _nrows; ++row)
            for (size_t col = 0; col < _ncols; ++col)
                (*_data)[index(row, col)] = to_T<T>(0.0);
    }

    // bbla.d:371-377 `@nogc void ones()`
    void ones() const {
        for (size_t row = 0; row < _nrows; ++row)
            for (size_t col = 0; col < _ncols; ++col)
                (*_data)[index(row, col)] = to_T<T>(1.0);
    }

    // bbla.d:379-388 `@nogc void eye()`
    void eye() const {
        for (size_t row = 0; row < _nrows; ++row) {
            for (size_t col = 0; col < _ncols; ++col) {
                (*_data)[index(row, col)] = to_T<T>(0.0);
            }
            (*_data)[index(row, row)] = to_T<T>(1.0);
        }
    }

    size_t _nrows;
    size_t _ncols;
    // bbla.d:24 `T[] _data;` -- an owned GC array, so an eil::Array held through
    // a shared_ptr to give the class its D reference semantics.
    std::shared_ptr<eil::Array<T>> _data;
}; // end class Matrix

// bbla.d:231-248 `Matrix!T opBinaryRight(string op)(double lhs)`. C++ has no
// opBinaryRight, so the three forms D declares (+, -, *; notably not /) become
// free operators with the operand order D uses: `lhs - _data[i]`.
template <typename T>
inline Matrix<T> operator+(double lhs, const Matrix<T>& m) {
    Matrix<T> result = new_matrix<T>(m._nrows, m._ncols);
    for (size_t row = 0; row < m._nrows; ++row) {
        for (size_t col = 0; col < m._ncols; ++col) {
            size_t i = m.index(row, col);
            (*result._data)[i] = lhs + (*m._data)[i];
        }
    }
    return result;
}

template <typename T>
inline Matrix<T> operator-(double lhs, const Matrix<T>& m) {
    Matrix<T> result = new_matrix<T>(m._nrows, m._ncols);
    for (size_t row = 0; row < m._nrows; ++row) {
        for (size_t col = 0; col < m._ncols; ++col) {
            size_t i = m.index(row, col);
            (*result._data)[i] = lhs - (*m._data)[i];
        }
    }
    return result;
}

template <typename T>
inline Matrix<T> operator*(double lhs, const Matrix<T>& m) {
    Matrix<T> result = new_matrix<T>(m._nrows, m._ncols);
    for (size_t row = 0; row < m._nrows; ++row) {
        for (size_t col = 0; col < m._ncols; ++col) {
            size_t i = m.index(row, col);
            (*result._data)[i] = lhs * (*m._data)[i];
        }
    }
    return result;
}

#ifdef complex_numbers
// bbla.d:250-269 `version(complex_numbers) Matrix!T opBinaryRight(string op)
// (Complex!double lhs)`.
// needs full ntypes/complex.d port (wave 12) -- see the opBinary note above;
// the trunk build never defines complex_numbers.
template <typename T>
inline Matrix<T> operator+(const eil::Complex<double>& lhs, const Matrix<T>& m) {
    Matrix<T> result = new_matrix<T>(m._nrows, m._ncols);
    for (size_t row = 0; row < m._nrows; ++row) {
        for (size_t col = 0; col < m._ncols; ++col) {
            size_t i = m.index(row, col);
            (*result._data)[i] = lhs + (*m._data)[i];
        }
    }
    return result;
}

template <typename T>
inline Matrix<T> operator-(const eil::Complex<double>& lhs, const Matrix<T>& m) {
    Matrix<T> result = new_matrix<T>(m._nrows, m._ncols);
    for (size_t row = 0; row < m._nrows; ++row) {
        for (size_t col = 0; col < m._ncols; ++col) {
            size_t i = m.index(row, col);
            (*result._data)[i] = lhs - (*m._data)[i];
        }
    }
    return result;
}

template <typename T>
inline Matrix<T> operator*(const eil::Complex<double>& lhs, const Matrix<T>& m) {
    Matrix<T> result = new_matrix<T>(m._nrows, m._ncols);
    for (size_t row = 0; row < m._nrows; ++row) {
        for (size_t col = 0; col < m._ncols; ++col) {
            size_t i = m.index(row, col);
            (*result._data)[i] = lhs * (*m._data)[i];
        }
    }
    return result;
}
#endif

// bbla.d:26-36, the two size forms.
template <typename T> inline Matrix<T> new_matrix(size_t n) { return Matrix<T>(n); }
template <typename T> inline Matrix<T> new_matrix(size_t nrows, size_t ncols) {
    return Matrix<T>(nrows, ncols);
}

// bbla.d:38-43 `this(in Matrix other)` -- the deep copy D spells `new Matrix!T(a)`.
template <typename T> inline Matrix<T> new_matrix(const Matrix<T>& other) {
    Matrix<T> out = Matrix<T>(other._nrows, other._ncols);
    for (size_t row = 0; row < out._nrows; ++row)
        for (size_t col = 0; col < out._ncols; ++col)
            (*out._data)[out.index(row, col)] = (*other._data)[other.index(row, col)];
    return out;
}

// bbla.d:45-68 `this(in T[] vec, string orient="column")` and the float[]/int[]
// variants. This is the view form D takes; a caller holding an eil::Array passes
// `.all()`, which is what D's `T[]` argument does implicitly.
template <typename T>
inline Matrix<T> new_matrix(eil::Slice<T> vec, const std::string& orient) {
    if (orient == "column") {
        Matrix<T> out(vec.length(), 1);
        for (size_t row = 0; row < out._nrows; ++row) {
            (*out._data)[out.index(row, 0)] = vec[row];
        }
        return out;
    }
    Matrix<T> out(1, vec.length());
    for (size_t col = 0; col < out._ncols; ++col) (*out._data)[col] = vec[col];
    return out;
}

template <typename T>
inline Matrix<T> new_matrix(std::initializer_list<T> vec, const std::string& orient) {
    // The view is read-only here; the initializer_list's backing array lives for
    // the whole full-expression, so it outlives the matrix built from it.
    return new_matrix<T>(eil::Slice<T>(const_cast<T*>(vec.begin()), vec.size(), nullptr, 0),
                         orient);
}

// bbla.d:70-89 `this(in T[][] other)` (and its float[][]/int[][] variants).
template <typename T>
inline Matrix<T> new_matrix(std::initializer_list<std::initializer_list<T>> other) {
    const size_t nrows = other.size();
    if (nrows == 0) throw eil::RangeError(0, 0);   // D: other[0] on an empty array
    const size_t ncols = other.begin()->size();
    Matrix<T> out(nrows, ncols);
    size_t row = 0;
    for (const std::initializer_list<T>& src_row : other) {
        // D indexes other[row][col] and would raise a RangeError on a ragged row.
        if (src_row.size() != ncols) throw eil::RangeError(ncols, src_row.size());
        size_t col = 0;
        for (const T& v : src_row) { (*out._data)[out.index(row, col)] = v; ++col; }
        ++row;
    }
    return out;
}

// bbla.d:392-406 `bool approxEqualMatrix(T)(in Matrix!T a, in Matrix!T b)`
template <typename T>
bool approxEqualMatrix(const Matrix<T>& a, const Matrix<T>& b) {
    if (a.nrows() != b.nrows()) return false;
    if (a.ncols() != b.ncols()) return false;
    bool is_equal = true;
    for (size_t row = 0; row < a.nrows(); ++row) {
        for (size_t col = 0; col < a.ncols(); ++col) {
            if (!approxEqualNumbers(a(row, col), b(row, col))) {
                is_equal = false;
                break;
            }
        }
    }
    return is_equal;
}

// bbla.d:408-417 `Matrix!T zeros(T)(size_t rows, size_t cols)`
template <typename T>
Matrix<T> zeros(size_t rows, size_t cols) {
    Matrix<T> my_matrix = new_matrix<T>(rows, cols);
    for (size_t row = 0; row < rows; ++row) {
        for (size_t col = 0; col < cols; ++col) {
            my_matrix(row, col) = to_T<T>(0.0);
        }
    }
    return my_matrix;
}

// bbla.d:419-428 `Matrix!T ones(T)(size_t rows, size_t cols)`
template <typename T>
Matrix<T> ones(size_t rows, size_t cols) {
    Matrix<T> my_matrix = new_matrix<T>(rows, cols);
    for (size_t row = 0; row < rows; ++row) {
        for (size_t col = 0; col < cols; ++col) {
            my_matrix(row, col) = to_T<T>(1.0);
        }
    }
    return my_matrix;
}

// bbla.d:430-439 `Matrix!T eye(T)(size_t n)`
template <typename T>
Matrix<T> eye(size_t n) {
    Matrix<T> ident_matrix = new_matrix<T>(n);
    for (size_t row = 0; row < n; ++row) {
        for (size_t col = 0; col < n; ++col) {
            ident_matrix(row, col) = (row == col) ? to_T<T>(1.0) : to_T<T>(0.0);
        }
    }
    return ident_matrix;
}

// bbla.d:441-450 `Matrix!T transpose(T)(in Matrix!T other)`
template <typename T>
Matrix<T> transpose(const Matrix<T>& other) {
    Matrix<T> my_matrix = new_matrix<T>(other.ncols(), other.nrows());
    for (size_t row = 0; row < other.nrows(); ++row) {
        for (size_t col = 0; col < other.ncols(); ++col) {
            my_matrix(col, row) = other(row, col);
        }
    }
    return my_matrix;
}

// bbla.d:452-475 `Matrix!T hstack(T)(in Matrix!T[] matrixList)`. D's parameter is
// a view of Matrix references; the initializer_list overload serves the
// `hstack!double([A,b])` literal form. The initializer_list's backing array
// outlives the call (it is a temporary of the enclosing full-expression) and
// every element is copied into the result, so the view does not escape.
template <typename T>
Matrix<T> hstack(eil::Slice<Matrix<T>> matrixList) {
    bool consistent = true;
    size_t nrows = matrixList[0].nrows();
    size_t ncols = 0;
    for (size_t k = 0; k < matrixList.length(); ++k) {
        const Matrix<T>& mat = matrixList[k];
        ncols += mat.ncols();
        if (nrows != mat.nrows()) consistent = false;
    }
    if (!consistent) {
        throw eil::ErrorBase("Matrices need to have the same number of rows");
    }
    Matrix<T> result = new_matrix<T>(nrows, ncols);
    size_t colStart = 0;
    for (size_t k = 0; k < matrixList.length(); ++k) {
        const Matrix<T>& mat = matrixList[k];
        for (size_t row = 0; row < mat.nrows(); ++row) {
            for (size_t col = 0; col < mat.ncols(); ++col) {
                result(row, colStart+col) = mat(row, col);
            }
        }
        colStart += mat.ncols();
    }
    return result;
}

template <typename T>
Matrix<T> hstack(std::initializer_list<Matrix<T>> matrixList) {
    // Read-only view of the literal's backing array (see the hstack note above).
    return hstack<T>(eil::Slice<Matrix<T>>(const_cast<Matrix<T>*>(matrixList.begin()),
                                           matrixList.size(), nullptr, 0));
}

// bbla.d:477-500 `Matrix!T vstack(T)(in Matrix!T[] matrixList)`
template <typename T>
Matrix<T> vstack(eil::Slice<Matrix<T>> matrixList) {
    bool consistent = true;
    size_t ncols = matrixList[0].ncols();
    size_t nrows = 0;
    for (size_t k = 0; k < matrixList.length(); ++k) {
        const Matrix<T>& mat = matrixList[k];
        nrows += mat.nrows();
        if (ncols != mat.ncols()) consistent = false;
    }
    if (!consistent) {
        throw eil::ErrorBase("Matrices need to have the same number of columns");
    }
    Matrix<T> result = new_matrix<T>(nrows, ncols);
    size_t rowStart = 0;
    for (size_t k = 0; k < matrixList.length(); ++k) {
        const Matrix<T>& mat = matrixList[k];
        for (size_t row = 0; row < mat.nrows(); ++row) {
            for (size_t col = 0; col < mat.ncols(); ++col) {
                result(rowStart+row, col) = mat(row, col);
            }
        }
        rowStart += mat.nrows();
    }
    return result;
}

template <typename T>
Matrix<T> vstack(std::initializer_list<Matrix<T>> matrixList) {
    // Read-only view of the literal's backing array (see the vstack note above).
    return vstack<T>(eil::Slice<Matrix<T>>(const_cast<Matrix<T>*>(matrixList.begin()),
                                           matrixList.size(), nullptr, 0));
}

// bbla.d:502-518 `Matrix!T dot(T)(in Matrix!T a, in Matrix!T b)`
template <typename T>
Matrix<T> dot(const Matrix<T>& a, const Matrix<T>& b) {
    if (a.ncols() != b.nrows()) {
        throw eil::ExceptionBase("incompatible matrices for dot product", __FILE__, __LINE__);
    }
    size_t nrows = a.nrows();
    size_t ncols = b.ncols();
    Matrix<T> c = zeros<T>(nrows, ncols);
    for (size_t row = 0; row < nrows; ++row) {
        for (size_t col = 0; col < ncols; ++col) {
            for (size_t i = 0; i < a.ncols(); ++i) {
                c(row, col) += a(row, i) * b(i, col);
            }
        }
    }
    return c;
}

// bbla.d:520-538 `@nogc void dot(T)(in Matrix!T a, in Matrix!T b, ref Matrix!T c)`
template <typename T>
void dot(const Matrix<T>& a, const Matrix<T>& b, Matrix<T>& c) {
    eil_assert(a.ncols() == b.nrows());
    eil_assert(a.nrows() == c.nrows());
    eil_assert(b.ncols() == c.ncols());
    size_t nrows = a.nrows();
    size_t ncols = b.ncols();
    c.zeros();
    for (size_t row = 0; row < nrows; ++row) {
        for (size_t col = 0; col < ncols; ++col) {
            for (size_t i = 0; i < a.ncols(); ++i) {
                c(row, col) += a(row, i) * b(i, col);
            }
        }
    }
}

/**
 * A Dot product that only considers the upper left portion
 * of the matrices.
 *
 * a: matrix on left of dot
 * aRow: work up to (but not including) this row number of a
 * aCol: work up to (but not including) this column number of a
 * b: matrix on right of dot
 * bCol: work up to (but not including) this column number of b
 * c: matrix where result is placed
 *
 * Notes:
 *   1. ensure that all matrices are pre-allocated in size.
 *   2. matrix c is only changed where the the new reuls is
 *      computed based on the supplied row and column range.
 *
 * bbla.d:540-577
 */
template <typename T>
void dot(const Matrix<T>& a, size_t aRow, size_t aCol,
         const Matrix<T>& b, size_t bCol, Matrix<T>& c) {
    eil_assert(aRow <= a.nrows());
    eil_assert(aCol <= a.ncols());
    eil_assert(aCol <= b.nrows());
    eil_assert(bCol <= b.ncols());
    eil_assert(aRow <= c.nrows());
    eil_assert(bCol <= c.ncols());
    for (size_t row = 0; row < aRow; ++row) {
        for (size_t col = 0; col < bCol; ++col) {
            c(row, col) = to_T<T>(0.0);
            for (size_t i = 0; i < aCol; ++i) {
                c(row, col) += a(row, i) * b(i, col);
            }
        }
    }
}

// bbla.d:579-594 `@nogc void dot(T)(in Matrix!T a, T[] b, T[] c)`
template <typename T>
void dot(const Matrix<T>& a, eil::Slice<T> b, eil::Slice<T> c) {
    eil_assert(a.ncols() == b.length());
    eil_assert(a.nrows() == c.length());
    size_t nrows = a.nrows();
    size_t ncols = a.ncols();
    for (size_t idx = 0; idx < c.length(); ++idx) c[idx] = 0.0;
    for (size_t row = 0; row < nrows; ++row) {
        for (size_t col = 0; col < ncols; ++col) {
            c[row] += a(row, col) * b[col];
        }
    }
}

// bbla.d:596-611 `@nogc void dot(T)(in Matrix!T a, size_t aRow, size_t aCol, T[] b, T[] c)`
template <typename T>
void dot(const Matrix<T>& a, size_t aRow, size_t aCol, eil::Slice<T> b, eil::Slice<T> c) {
    eil_assert(aRow <= a.nrows());
    eil_assert(aCol <= a.ncols());
    eil_assert(aCol <= b.length());
    eil_assert(aRow <= c.length());
    for (size_t idx = 0; idx < c.length(); ++idx) c[idx] = 0.0;
    for (size_t row = 0; row < aRow; ++row) {
        for (size_t col = 0; col < aCol; ++col) {
            c[row] += a(row, col) * b[col];
        }
    }
}

/*
    Some additional dot product routines that use flattened bare arrays,
    which somtimes are used as matrices in performance sensitive areas
    of the code.
    @author: NNG (July 2023)

    bbla.d:613-628
*/
template <typename T>
void dot(eil::Slice<T> a, size_t a_stride, size_t aRow, size_t aCol,
         eil::Slice<T> b, eil::Slice<T> c) {
    for (size_t idx = 0; idx < c.length(); ++idx) c[idx] = 0.0;
    for (size_t row = 0; row < aRow; ++row) {
        for (size_t col = 0; col < aCol; ++col) {
            c[row] += a[row*a_stride+col] * b[col];
        }
    }
}

// bbla.d:630-650 `@nogc void transpose_and_dot(T)(...)`
template <typename T>
void transpose_and_dot(eil::Slice<T> a, size_t a_stride, size_t aRow, size_t aCol,
                       eil::Slice<T> b, eil::Slice<T> c) {
/*
    This routine does an inplace tranpose and dot, like this:
    [a1,  a2,  a3,  a4,  a5,  a6,  a7][q]   [a1*q + b1*r + c1*z]
    [b1,  b2,  b3,  b4,  b5,  b6,  b7][r] = [a2*q + b2*r + c2*z]
    [c1,  c2,  c3,  c4,  c5,  c6,  c7][z]   [a3*q + b3*r + c3*z]
                                            [a4*q + b4*r + c4*z]
                                            [a5*q + b5*r + c5*z]
                                            [a6*q + b6*r + c6*z]
                                            [a7*q + b7*r + c7*z]
    @author: NNG (July 2023)
*/
    for (size_t idx = 0; idx < c.length(); ++idx) c[idx] = 0.0;
    for (size_t row = 0; row < aRow; ++row) {
        for (size_t col = 0; col < aCol; ++col) {
            c[col] += a[row*a_stride+col] * b[row];
        }
    }
}

// bbla.d:652-664 `@nogc void copy(T)(in Matrix!T src, ref Matrix!T tgt)`
template <typename T>
void copy(const Matrix<T>& src, Matrix<T>& tgt) {
    eil_assert(src.nrows() == tgt.nrows());
    eil_assert(src.ncols() == tgt.ncols());
    for (size_t row = 0; row < src.nrows(); ++row) {
        for (size_t col = 0; col < src.ncols(); ++col) {
            tgt(row, col) = src(row, col);
        }
    }
}

/**
 * Perform Gauss-Jordan elimination on an augmented matrix.
 * c = [A|b] such that the mutated matrix becomes [I|x]
 * where x is the solution vector(s) to A.x = b
 *
 * bbla.d:700-731
 */
template <typename T>
void gaussJordanElimination(Matrix<T>& c, double very_small_value = 1.0e-16) {
    if (c.ncols() < c.nrows()) {
        throw eil::ExceptionBase("too few columns supplied", __FILE__, __LINE__);
    }
    for (size_t j = 0; j < c.nrows(); ++j) {
        // Select pivot.
        size_t p = j;
        for (size_t i = j+1; i < c.nrows(); ++i) {
            if (std::fabs(c(i, j)) > std::fabs(c(p, j))) p = i;
        }
        if (std::fabs(c(p, j)) < very_small_value) {
            throw eil::ExceptionBase("matrix is essentially singular", __FILE__, __LINE__);
        }
        if (p != j) { c.swapRows(p, j); }
        // Scale row j to get unity on the diagonal.
        T cjj = c(j, j);
        for (size_t col = 0; col < c.ncols(); ++col) { c(j, col) /= cjj; }
        // Do the elimination to get zeros in all off diagonal values in column j.
        for (size_t i = 0; i < c.nrows(); ++i) {
            if (i == j) continue;
            T cij = c(i, j);
            for (size_t col = 0; col < c.ncols(); ++col) { c(i, col) -= cij * c(j, col); }
        }
    } // end foreach j
} // end gaussJordanElimination()

/**
 * First stage of a linear equation solver that uses LU decomposition
 * followed by a separate backsubstitution.
 *
 * Params:
 *     c: incoming matrix, outgoing LU matrices combined
 *        as described in Section 2.2 of Gerald and Wheatley
 *        section 2.2 Elimination methods.
 *     very_small_value: used in our test for singularity
 *
 * Returns:
 *     A list of row-permutation pairs (the indices of the swapped rows).
 *     Since we allow partial-pivoting by swapping rows,
 *     we need to keep a record of the row permutations
 *     to be passed into the solve function.
 *
 * bbla.d:759-805. D's `size_t[2][]` is an array of static pairs, which
 * to!string renders as "[[3, 0], [2, 1]]"; std::array<size_t,2> inside an
 * eil::Array renders the same way through eil::to_string.
 */
template <typename T>
eil::Array<std::array<size_t, 2>> decomp(Matrix<T>& c, double very_small_value = 1.0e-16) {
    if (c.ncols() != c.nrows()) {
        throw eil::ExceptionBase("require a square matrix", __FILE__, __LINE__);
    }
    eil::Array<std::array<size_t, 2>> permuteList;

    for (size_t j = 0; j < c.nrows(); ++j) {
        // Select pivot.
        size_t p = j;
        for (size_t i = j+1; i < c.nrows(); ++i) {
            if (std::fabs(c(i, j)) > std::fabs(c(p, j))) { p = i; }
        }
        if (std::fabs(c(p, j)) < very_small_value) {
            throw eil::ExceptionBase("matrix is essentially singular", __FILE__, __LINE__);
        }
        if (p != j) {
            c.swapRows(p, j);
            permuteList.append(std::array<size_t, 2>{p, j});   // `permuteList ~= [p,j]`
        }
        // Do the elimination to get zeros in column j, below the diagonal.
        // Don't disturb the previous multipliers stored in columns to the left.
        for (size_t i = j+1; i < c.nrows(); ++i) {
            T multiplier = c(i, j)/c(j, j);
            for (size_t col = j; col < c.ncols(); ++col) { c(i, col) -= multiplier * c(j, col); }
            c(i, j) = multiplier; // Save in the newly-zeroed spot.
        }
    } // end foreach j

    return permuteList;
} // end decomp()

/**
 * Second stage of the linear equation solver.
 *
 * Params:
 *     c:  decomposed matrix from the first stage.
 *         This matrix may be reused for any number of RHS vectors.
 *     rhs: on input, one or more column vectors for the right-hand side
 *         on return, these are the solution vectors.
 *     permuteList: list of row-permutation pairs from the first stage.
 *
 * bbla.d:807-848
 */
template <typename T>
void solve(const Matrix<T>& c, Matrix<T>& rhs,
           eil::Slice<std::array<size_t, 2>> permuteList) {
    size_t nrows = c.nrows();
    if (rhs.ncols() < 1 || rhs.nrows() != nrows) {
        throw eil::ExceptionBase("invalid right-hand side", __FILE__, __LINE__);
    }
    // Handle degenerate case with a single element matrix
    if ((c.ncols() == 1) && (c.nrows() == 1)) {
        rhs(0, 0) = rhs(0, 0)/c(0, 0);
        return;
    }
    // Get the right-hand side rows into final order.
    for (size_t k = 0; k < permuteList.length(); ++k) {
        const std::array<size_t, 2>& rowPair = permuteList[k];
        rhs.swapRows(rowPair[0], rowPair[1]);
    }
    // Forward elimination, using the stored multipliers.
    for (size_t i = 1; i < nrows; ++i) {
        for (size_t j = 0; j < i; ++j) {
            T multiplier = c(i, j);
            for (size_t col = 0; col < rhs.ncols(); ++col) {
                rhs(i, col) -= multiplier * rhs(j, col);
            }
        }
    }
    // Back substitution to obtain the solution vector(s).
    for (size_t col = 0; col < rhs.ncols(); ++col) {
        rhs(nrows-1, col) /= c(nrows-1, nrows-1);
        for (int i = static_cast<int>(nrows-2); i >= 0; --i) {
            const size_t ii = static_cast<size_t>(i);
            T my_sum = rhs(ii, col);
            for (size_t j = ii+1; j < nrows; ++j) { my_sum -= c(ii, j) * rhs(j, col); }
            rhs(ii, col) = my_sum/c(ii, ii);
        }
    }
} // end solve()

// bbla.d:850-861 `Matrix!T inverse(T)(in Matrix!T a)`
template <typename T>
Matrix<T> inverse(const Matrix<T>& a) {
    auto n = a.nrows();
    if (n != a.ncols() && n == 0) {
        throw eil::ExceptionBase("matrix should be square and not empty", __FILE__, __LINE__);
    }
    auto c = new_matrix<T>(a);
    auto perm = decomp<T>(c);
    auto x = eye<T>(n);
    solve<T>(c, x, perm.all());
    return x;
}

// bbla.d:863-872 `void inverse(T)(Matrix!T a, ref Matrix!T x)`. `a` is a D class
// reference passed by value, so decomp() leaves the caller's matrix in LU form
// (which the smla unittest relies on not being visible: it rewrites every block
// entry before each call).
template <typename T>
void inverse(Matrix<T> a, Matrix<T>& x) {
    auto n = a.nrows();
    if (n != a.ncols() && n == 0) {
        throw eil::ExceptionBase("matrix should be square and not empty", __FILE__, __LINE__);
    }
    auto perm = decomp<T>(a);
    x.eye();
    solve<T>(a, x, perm.all());
    (void)n;
}

// bbla.d:874-884 `void upperSolve(T)(in Matrix!T U, T[] b)`
template <typename T>
void upperSolve(const Matrix<T>& U, eil::Slice<T> b) {
    int n = static_cast<int>(U.nrows());
    // Back subsitution
    b[n-1] /= U(n-1, n-1);
    for (int i = static_cast<int>(n-2); i >= 0; --i) {
        T sum = b[i];
        for (int j = i+1; j < n; ++j) { sum -= U(i, j) * b[j]; }
        b[i] = sum/U(i, i);
    }
}

// bbla.d:886-900 `void upperSolve(T)(in Matrix!T U, int n, T[] b)`
template <typename T>
void upperSolve(const Matrix<T>& U, int n, eil::Slice<T> b) {
    eil_assert(static_cast<size_t>(n) <= U.nrows());
    eil_assert(static_cast<size_t>(n) <= U.ncols());
    eil_assert(static_cast<size_t>(n) <= b.length());
    // Back subsitution
    b[n-1] /= U(n-1, n-1);
    for (int i = static_cast<int>(n-2); i >= 0; --i) {
        T sum = b[i];
        for (int j = i+1; j < n; ++j) { sum -= U(i, j) * b[j]; }
        b[i] = sum/U(i, i);
    }
}

/**
 * Solve an over-constrained linear system of constraint equations c.x=rhs
 * in a least-squares sense.
 *
 * Params:
 *     c: Matrix of coefficients with nrows >= ncols
 *     rhs: right-hand-side vector of size nrows,ncols2
 *
 * Returns:
 *     x: solution matrix of size ncols, ncols2
 *
 * Notes:
 *     ncols2 may be larger than 1 so that several right-hand-sides
 *     can be solved in one sitting.
 *
 * bbla.d:921-958. D's `throw new Error(text("too few constraints N=", N, " m=", m))`
 * concatenates to!string of each argument, which eil::text does too.
 */
template <typename T>
Matrix<T> lsqsolve(const Matrix<T> c, const Matrix<T> rhs) {
    size_t m = c.ncols(); // number of unknowns
    size_t N = c.nrows(); // number of linear constraint equations
    if (N < m) {
        throw eil::ErrorBase(eil::text("too few constraints N=", N, " m=", m),
                             __FILE__, __LINE__);
    }
    // Prepare the normal equations A.x = b
    Matrix<T> a = zeros<T>(m, m);
    Matrix<T> x = zeros<T>(m, rhs.ncols());
    for (size_t k = 0; k < m; ++k) {
        for (size_t j = 0; j < m; ++j) {
            for (size_t i = 0; i < N; ++i) { a(k, j) += c(i, k)*c(i, j); }
        }
        for (size_t j = 0; j < rhs.ncols(); ++j) {
            for (size_t i = 0; i < N; ++i) { x(k, j) += c(i, k)*rhs(i, j); }
        }
    }
    // Solve using decomposition and solve for the usual mxm linear system.
    auto perm = decomp<T>(a);
    solve<T>(a, x, perm.all());
    return x;
} // end lsqsolve()()

/**
 * Use Crout's reduction to form an LU decomposition.
 *
 * This code is adapted from Section 2.3 in Press et al. (2007).
 * The code in Numerical Recipes, 3rd edition is written for
 * C++ and makes use of classes. The implementation does not use
 * classes, just functions.
 *
 * Press et al. (2007)
 * Numerical Recipes: The Art of Scientific Computing, 3rd edition
 * Cambridge University Press, Cambridge, UK
 *
 * bbla.d:976-1040. D's `ref int[] pivot` is a reference to the caller's dynamic
 * array (it can be resized through the reference), so it takes eil::Array<int>&;
 * the in-contracts on length are kept.
 */
template <typename T>
void LUDecomp(Matrix<T> a, eil::Array<int>& pivot, double verySmallValue = 1.0e-40) {
    eil_assert(a.nrows() == a.ncols());           // "require a square matrix"
    eil_assert(pivot.length() == a.nrows());      // "pivot array wrongly sized"

    // D default-initialises the `int iMax` local to 0, and that is
    // load-bearing: if no element of column k beats `largest`, iMax keeps its
    // previous value, so the starting value decides the pivot.
    int iMax = 0;
    T largest = T();
    T tmp = T();
    eil::Array<T> vv;

    int n = static_cast<int>(a.nrows());
    vv.set_length(static_cast<size_t>(n));

    for (int i = 0; i < n; ++i) {
        largest = 0.0;
        for (int j = 0; j < n; ++j) {
            tmp = std::fabs(a(i, j));
            if (tmp > largest) { largest = tmp; }
        }
        if (largest == 0.0)
            throw eil::ExceptionBase("Singular matrix in LUDecompCrout", __FILE__, __LINE__);
        vv[static_cast<size_t>(i)] = 1.0/largest; // saves the scaling
    }

    for (int k = 0; k < n; ++k) {
        largest = 0.0;
        for (int i = k; i < n; ++i) {
            tmp = vv[static_cast<size_t>(i)]*std::fabs(a(i, k));
            if (tmp > largest) {
                largest = tmp;
                iMax = i;
            }
        }
        if (k != iMax) { // if we need to interchange rows
            for (int j = 0; j < n; ++j) {
                tmp = a(iMax, j);
                a(iMax, j) = a(k, j);
                a(k, j) = tmp;
            }
            vv[static_cast<size_t>(iMax)] = vv[static_cast<size_t>(k)];
        }
        pivot[k] = iMax;
        if (a(k, k) == 0.0) { a(k, k) = to_T<T>(verySmallValue); }
        for (int i = k+1; i < n; ++i) {
            a(i, k) /= a(k, k);
            tmp = a(i, k);
            for (int j = k+1; j < n; ++j) {
                a(i, j) -= tmp*a(k, j);
            }
        }
    }
}

// bbla.d:1042-1076 `void LUSolve(T)(Matrix!T a, int[] pivot, T[] b, ref T[] x)`
template <typename T>
void LUSolve(Matrix<T> a, eil::Slice<int> pivot, eil::Slice<T> b, eil::Array<T>& x) {
    eil_assert(a.nrows() == a.ncols());           // "require a square matrix"
    eil_assert(pivot.length() == a.nrows());      // "pivot array wrongly sized"
    eil_assert(b.length() == a.nrows());          // "b array wrongly sized"
    eil_assert(x.length() == a.nrows());          // "x array wrongly sized"

    int ii, ip, n;
    ii = 0;
    T sum = T();
    n = static_cast<int>(a.nrows());

    for (size_t idx = 0; idx < x.length(); ++idx) { x[idx] = b[idx]; }

    for (int i = 0; i < n; ++i) {
        ip = pivot[i];
        sum = x[ip];
        x[ip] = x[i];
        if (ii != 0) {
            for (int j = ii-1; j < i; ++j) {
                sum -= a(i, j)*x[j];
            }
        } else if (sum != 0.0) {
            ii = i + 1;
        }
        x[i] = sum;
    }

    for (int i = n-1; i >= 0; i--) {
        sum = x[i];
        for (int j = i+1; j < n; ++j) { sum -= a(i, j)*x[j]; }
        x[i] = sum / a(i, i);
    }
}

} // namespace bbla
} // namespace eil::nm
