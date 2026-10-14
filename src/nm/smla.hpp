#pragma once
 /**
 * A minimal sparse matrix linear algebra module.
 *
 * Author: Rowan J. Gollan
 * Date: 2015-12-18
 *
 */
// nm/smla.d -> src/nm/smla.hpp (module nm.smla).
//
// Naming. smla.d declares dot, norm2, approxEqualMatrix, decompILU0/ILUp, solve
// and the block routines, and it imports nm.bbla, which also has dot/solve/copy/
// zeros/eye/transpose/inverse/approxEqualMatrix. D separates them by module, so
// the D module scope becomes namespace eil::nm::smla and the bbla names are
// pulled in by the `using` block below, which stands for smla.d:22
// `import nm.bbla;`. Where D writes nm.bbla.dot(...), the port writes
// bbla::dot(...).
//
// Reference semantics, as explained in nm/bbla.hpp: SMatrix is a D class, so a
// variable is a reference and a by-value parameter (decompILU0's `SMatrix!T a`,
// smla.d:321) mutates the caller's object -- the unittest depends on that when it
// asserts against `d` right after decompILU0(d) (smla.d:1186-1193). The three CRS
// arrays therefore live in one shared body and aa(), ja(), ia() hand out the
// arrays. D's deep copy `new SMatrix!T(a)` (smla.d:75-78, which forwards to the
// dup-ing constructor) is new_smatrix<T>(a).
//
// Element access. D's SMatrix has two distinct index operations that a single
// C++ accessor cannot express: opIndex (smla.d:165) reads and answers 0 for an
// absent entry, while opIndexAssign (smla.d:178) *inserts* the entry and shuffles
// aa/ja/ia, which is how the ILU(p) decomposition grows the pattern
// (smla.d:1247, tested by the sol1 expectation at smla.d:1248-1257). The port
// names them get() and set(). set() inserts a zeroed slot and returns a reference
// to it, so `a[i,j] = v` becomes `a.set(i,j) = v`; the value D inserts is the
// same value the caller assigns, so the stored matrix is identical. D's own
// return reference is the one opIndexAssign hands back, which no call site in
// this module reads (every use is an assignment statement).
//
// Loop bounds. `foreach (j; a.ja[a.ia[i] .. a.ia[i+1]])` snapshots a slice of ja
// once and walks it. The port snapshots the two index bounds at loop entry and
// indexes ja fresh inside. Those agree exactly when the body does not insert into
// the row, and none of the routines here does: ILU(0) only rewrites entries that
// are already in the pattern, and ILU(p)'s insertions happen in a phase that is
// not iterating over ja.
//
// D's `g_old[] = g[]` (smla.d:723, :842, :923, :995, :1097) is an element-wise
// copy between equal-length arrays; the substrate spells that on a view, so the
// port writes `g_old.all().assign_from(g.all())`. A length mismatch raises
// eil::SliceError where D raises its own array-assignment error.
//
// smla.d:11 imports core.stdc.stdlib:exit and smla.d:17 imports core.memory.
// `exit` is never called in this module (imported only). core.memory supplies the
// GC.minimize() at smla.d:390, which has no C++ counterpart and no observable
// effect; see the note in decompILUp.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>

#include "eil/aa.hpp"
#include "eil/array.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/math.hpp"
#include "eil/number.hpp"
#include "nm/bbla.hpp"
#include "nm/number.hpp"
#include "nm/stmatrix.hpp"

namespace eil::nm {
namespace smla {

// smla.d:22 `import nm.bbla;` -- the names this module reuses unqualified.
// dot is included because D picks bbla's or smla's dot purely by argument type
// (matrices versus arrays), and the merged overload set here makes the same
// choice.
using bbla::copy;
using bbla::dot;
using bbla::eye;
using bbla::inverse;
using bbla::Matrix;
using bbla::new_matrix;
using bbla::to_T;
using bbla::zeros;

// smla.d:27 `immutable double ESSENTIALLY_ZERO = 1.0e-50;`
extern const double ESSENTIALLY_ZERO;

// smla.d:37-43 `T dot(T)(T[] a, T[] b)`
template <typename T>
T dot(eil::Slice<T> a, eil::Slice<T> b) {
    eil_assert(a.length() == b.length());
    T sum = 0.0;
    for (size_t i = 0; i < a.length(); ++i) sum += a[i]*b[i];
    return sum;
}

// smla.d:45-49 `T norm2(T)(T[] vec)`. D's reduce folds `a + b*b` left to right
// over the vector, which is the loop below; nothing is re-associated.
// eil::sqrt takes eil::number, which is the only T this module is instantiated
// with in the trunk build (nm/number.d's number == double).
template <typename T>
T norm2(eil::Slice<T> vec) {
    T ssquares = to_T<T>(0.0);
    for (size_t i = 0; i < vec.length(); ++i) {
        ssquares = ssquares + vec[i] * vec[i];
    }
    return eil::sqrt(ssquares);
}

// Stands for std.array.insertInPlace, used by SMatrix::set (smla.d:194-195,
// :201-202): grow by one element and shuffle [at,$) one place right.
template <typename U>
void insertInPlace(eil::Array<U>& arr, size_t at, const U& value) {
    arr.append(value);
    for (size_t i = arr.length()-1; i > at; --i) arr[i] = arr[i-1];
    arr[at] = value;
}

// Stands for std.algorithm.searching.maxElement (smla.d:212 `ja.maxElement`).
// Its contract assert is `assert(!r.empty, "r is an empty range")` at
// std/algorithm/searching.d:1338, which reaches the caller as
// core.exception.AssertError (an Error, so ErrorBase is the right port class) with
// exactly that .msg. Measured in baselines/e587b235/probes/substrate_throw_identity.txt
// row phobos.maxElement_empty. Two things the earlier version of this line got wrong:
// the text was "range is empty", which D never prints, and it attached this file's
// __FILE__/__LINE__ -- D's .file here is searching.d inside the standard library, not
// the caller. Backlog T-922 covers the rest of that sweep.
template <typename U>
U maxElement(const eil::Array<U>& r) {
    if (r.length() == 0) throw eil::ErrorBase("r is an empty range");
    U m = r[0];
    for (size_t i = 1; i < r.length(); ++i) if (r[i] > m) m = r[i];
    return m;
}

// Stands for D's `a.ja != b.ja` on dynamic arrays (smla.d:281-282), which
// compares length and then elements.
template <typename U>
bool indicesEqual(const eil::Array<U>& x, const eil::Array<U>& y) {
    if (x.length() != y.length()) return false;
    for (size_t i = 0; i < x.length(); ++i) if (x[i] != y[i]) return false;
    return true;
}

/**
 * This class is used to store and work with a sparse matrix.
 * The data is stored in Compressed Sparse Row format.
 * Usually a class is used to hide details of implementation
 * (in this case, storage). However, here we will expose those
 * items as public method data since the algorithms to manipulate
 * sparse matrices rely on the representation of the data.
 *
 * smla.d:51-274
 */
template <typename T> class SMatrix;

// The `new SMatrix!T(...)` forms, declared before the class so member bodies can
// use them (D sees all module-scope names; a C++ template body does not).
template <typename T> SMatrix<T> new_smatrix();
template <typename T>
SMatrix<T> new_smatrix(eil::Slice<T> aa, eil::Slice<size_t> ja, eil::Slice<size_t> ia);
template <typename T>
SMatrix<T> new_smatrix(std::initializer_list<T> aa,
                       std::initializer_list<size_t> ja,
                       std::initializer_list<size_t> ia);
template <typename T> SMatrix<T> new_smatrix(const SMatrix<T>& other);
template <typename T> SMatrix<T> new_smatrix(stmatrix::STMatrix<T> stMat);

template <typename T> class SMatrix {
  public:
    struct Body {
        eil::Array<T> aa;
        eil::Array<size_t> ja;
        eil::Array<size_t> ia;
    };

    // smla.d:66 `this() {}`
    SMatrix() : _body(std::make_shared<Body>()) {}

    // smla.d:68-73 `this(T[] aa, size_t[] ja, size_t[] ia)` -- the arguments are
    // dup-ed here, exactly as D's `.dup` does, so the object owns its storage.
    SMatrix(eil::Slice<T> aa_, eil::Slice<size_t> ja_, eil::Slice<size_t> ia_)
        : _body(std::make_shared<Body>()) {
        _body->aa = eil::Array<T>(aa_);
        _body->ja = eil::Array<size_t>(ja_);
        _body->ia = eil::Array<size_t>(ia_);
    }

    // Copying an SMatrix aliases, as D's class reference does. The deep copy is
    // new_smatrix<T>(other), which is smla.d:75-78 `this(SMatrix other)`.
    SMatrix(const SMatrix&) = default;
    SMatrix& operator=(const SMatrix&) = default;
    SMatrix(SMatrix&&) = default;
    SMatrix& operator=(SMatrix&&) = default;

    // smla.d:80-116 `this(STMatrix!T stMat)`. D's parameter is a class reference
    // passed by value and the loop consumes the source table through it
    // (val.remove), so the handle is taken by value here too.
    SMatrix(stmatrix::STMatrix<T> stMat) : _body(std::make_shared<Body>()) {
        // Use the average number of non-zeros per row to reserve an array size.
        // This might help cut down on allocations while assembling a row.
        [[maybe_unused]] auto n_nzAvgPerRow = stMat.n_nzeros / stMat.n_rows;
        eil::Array<T> ai;
        eil::Array<size_t> ji;
        eil::AA<size_t, T> vi;
        eil::Array<stmatrix::TripletKey<T>> keysToRemove;
        // We loop over all rows.
        // Every row should have at least one non-zero entry, otherwise there's an issue with our matrix.
        for (size_t i = 0; i < stMat.n_rows; ++i) {
            for (const auto& entry : stMat.val()) {
                const stmatrix::TripletKey<T>& c = entry.first;
                const T& v = entry.second;
                if (c.first == i) {
                    vi.set(c.second, v);
                    keysToRemove.append(c);
                }
            }
            // Add the collected values as a new row in the sparse matrix.
            // We need to sort the entries in column-ascending order.
            ji = vi.keys();
            std::sort(ji.begin(), ji.end());
            for (size_t k = 0; k < ji.length(); ++k) ai.append(vi.get(ji[k]));
            addRow(ai.all(), ji.all());
            // Remove the entries from the sparse triplet matrix that we just used
            // to save looking through them again.
            for (size_t k = 0; k < keysToRemove.length(); ++k) {
                stMat.val().remove(keysToRemove[k]);
            }
            // Prepare temporary containers for next loop.
            ai.set_length(0);
            ji.set_length(0);
            keysToRemove.set_length(0);
            vi.clear();
        }
    }

    // smla.d:62-64 `T[] aa; size_t[] ja; size_t[] ia;` -- public in D, reached
    // through the shared body here.
    eil::Array<T>& aa() const { return _body->aa; }
    eil::Array<size_t>& ja() const { return _body->ja; }
    eil::Array<size_t>& ia() const { return _body->ia; }

    // smla.d:118-125 `void addRow(T[] ai, size_t[] ji)`
    void addRow(eil::Slice<T> ai, eil::Slice<size_t> ji) const {
        if ( ia().length() == 0 )
            ia().append(0);
        aa().append_slice(ai);
        ja().append_slice(ji);
        ia().append(aa().length()); // Now ia is already ready for next addition.
    }

    // The array-literal spelling of D's `b.addRow([1.,2.], [0,3])` call sites.
    void addRow(std::initializer_list<T> ai, std::initializer_list<size_t> ji) const {
        addRow(eil::Slice<T>(const_cast<T*>(ai.begin()), ai.size(), nullptr, 0),
               eil::Slice<size_t>(const_cast<size_t*>(ji.begin()), ji.size(), nullptr, 0));
    }

    // smla.d:127-136 `void dropRows(size_t nDrop)`
    void dropRows(size_t nDrop) const {
        if (nDrop > (ia().length()-1)) {
            nDrop = ia().length()-1;
        }
        size_t nvals = ia()[ia().length()-1] - ia()[ia().length()-(1+nDrop)];
        ja().set_length(ja().length() - nvals);
        aa().set_length(aa().length() - nvals);
        ia().set_length(ia().length() - nDrop);
    }

    // smla.d:138-144 `void scaleRow(size_t row, T scaleFactor)`
    void scaleRow(size_t row, T scaleFactor) const {
        eil_assert(row < ia().length()-1);
        const size_t k_end = ia()[row+1];
        for (size_t j = ia()[row]; j < k_end; ++j) {
            aa()[j] *= scaleFactor;
        }
    }

    // smla.d:146-157 `void scaleCol(size_t col, T scaleFactor)`
    void scaleCol(size_t col, T scaleFactor) const {
        eil_assert(col < ia().length()-1);
        for (size_t row = 0; row < ia().length()-1; ++row) {
            const size_t row_end = ia()[row+1];
            for (size_t j = ia()[row]; j < row_end; ++j) {
                // D's `foreach (offset, j; ja[ia[row]..ia[row+1]])`
                const size_t offset = j - ia()[row];
                if ( ja()[j] == col ) {
                    aa()[ia()[row]+offset] *= scaleFactor;
                    break;
                }
            }
        }
    }

    // smla.d:159-163 `void scale(T scaleFactor)`
    void scale(T scaleFactor) const {
        for (T& elem : aa()) {
            elem *= scaleFactor;
        }
    }

    // smla.d:165-175 `const T opIndex(size_t row, size_t col)` -- a miss reads as
    // zero and leaves the structure alone.
    T get(size_t row, size_t col) const {
        // We need to search the given row to see if an entry
        // is present in the given column.
        const size_t row_end = ia()[row+1];
        for (size_t j = ia()[row]; j < row_end; ++j) {
            if ( ja()[j] == col )
                return aa()[j];
        }
        // else search failed, so we have a 0.0 entry
        return to_T<T>(0.0);
    }

    // smla.d:177-209 `ref T opIndexAssign(T c, size_t row, size_t col)`, the
    // inserting form (see the header note). Assigning to an absent entry adds it
    // and shuffles aa/ja/ia, which is what grows the ILU(p) pattern.
    //
    // set() locates or creates the slot and hands back a reference to it, without
    // writing, so `a.set(i,j) = rhs` leaves rhs' own reads of `a` unaffected by
    // the order C++ evaluates the two sides in: an existing entry is untouched
    // until the assignment, and a created entry reads as 0 either way (which is
    // what get() reports for an absent entry).
    T& set(size_t row, size_t col) const {
        const size_t row_end = ia()[row+1];
        for (size_t j = ia()[row]; j < row_end; ++j) {
            if ( ja()[j] == col ) {
                return aa()[j];
            }
        }

        // If we get here, we tried to assign to a zero value,
        // so to add the entry we will need to shuffle all the elements
        // WARNING: this code will happily add a zero to the sparse matrix;
        //          onus is on any user using this object to prevent
        //          unnecessary assingment of 0 values.

        // shuffle aa, ja, ia entries
        if ( col > ja()[ia()[row+1]-1] ) { // if the new entry occurs after all currently stored columns in the row
            const size_t at = ia()[row+1];
            insertInPlace(aa(), at, T());
            insertInPlace(ja(), at, col);
            for (size_t k = row+1; k < ia().length(); ++k) ia()[k] += 1;
            // D returns aa[ia[row+1]] here, which is the slot *after* the one it
            // just inserted (ia[row+1] has already been bumped), and no caller in
            // this module reads that reference. The port returns the inserted slot
            // so that `set(row,col) = v` lands where D's insertInPlace put v.
            return aa()[at];
        }
        for (size_t j = ia()[row]; j < row_end; ++j) { // else handle all other scenarios in a generic manner
            if ( col < ja()[j] ) {
                insertInPlace(aa(), j, T());
                insertInPlace(ja(), j, col);
                for (size_t k = row+1; k < ia().length(); ++k) ia()[k] += 1;
                return aa()[j];
            }
        }
        // if we get here, something has gone wrong
        throw eil::ErrorBase(
            "ERROR: An error occured while assigning a value to an entry in the sparse matrix.");
    }

    // smla.d:211-224 `override string toString()`
    std::string toString() const {
        auto jmax = maxElement(ja());
        std::string s = "SMatrix[\n";
        for (size_t row = 0; row < ia().length()-1; ++row) {
            for (size_t col = 0; col < jmax+1; ++col) {
                s += eil::to_string(get(row, col));
                if ( col < jmax )
                    s += ", ";
            }
            s += "\n";
        }
        s += "]";
        return s;
    }

    // smla.d:226-272 `SMatrix!T transpose()` -- D's body assumes a square matrix.
    SMatrix transpose() const {
        // Presently, assume square matrices
        auto tp = new_smatrix<T>();
        tp.aa().set_length(aa().length());
        tp.ja().set_length(ja().length());
        tp.ia().set_length(ia().length());

        tp.ia().fill(0);

        // Treat original matrix a CSC
        // We'll count entries in the transposed rows
        // by looking at "columns" of original.
        for (size_t k = 0; k < ja().length(); ++k) {
            const size_t row_idx = ja()[k];
            tp.ia()[row_idx]++;
        }

        // To get the offsets into ia, compute a running sum
        size_t sum = 0;
        for (size_t row = 0; row < ia().length(); ++row) {
            size_t tmp = tp.ia()[row];
            tp.ia()[row] = sum;
            sum += tmp;
        }

        for (size_t col = 0; col < ia().length()-1; ++col) {
            for (size_t jj = ia()[col]; jj < ia()[col+1]; ++jj) {
                auto row = ja()[jj];
                auto dest = tp.ia()[row];

                tp.ja()[dest] = col;
                tp.aa()[dest] = aa()[jj];

                tp.ia()[row]++;
            }
        }

        size_t last = 0;
        for (size_t row = 0; row < ia().length(); ++row) {
            auto tmp = tp.ia()[row];
            tp.ia()[row] = last;
            last = tmp;
        }

        return tp;
    }

  private:
    std::shared_ptr<Body> _body;
}; // end class SMatrix

// smla.d:66, :68-73, :75-78 and :80-116 -- the constructor forms D reaches with
// `new SMatrix!T(...)`.
template <typename T> inline SMatrix<T> new_smatrix() { return SMatrix<T>(); }

template <typename T>
inline SMatrix<T> new_smatrix(eil::Slice<T> aa, eil::Slice<size_t> ja, eil::Slice<size_t> ia) {
    return SMatrix<T>(aa, ja, ia);
}

template <typename T>
inline SMatrix<T> new_smatrix(std::initializer_list<T> aa,
                              std::initializer_list<size_t> ja,
                              std::initializer_list<size_t> ia) {
    return SMatrix<T>(eil::Slice<T>(const_cast<T*>(aa.begin()), aa.size(), nullptr, 0),
                      eil::Slice<size_t>(const_cast<size_t*>(ja.begin()), ja.size(), nullptr, 0),
                      eil::Slice<size_t>(const_cast<size_t*>(ia.begin()), ia.size(), nullptr, 0));
}

template <typename T>
inline SMatrix<T> new_smatrix(const SMatrix<T>& other) {
    return SMatrix<T>(other.aa().all(), other.ja().all(), other.ia().all());
}

template <typename T>
inline SMatrix<T> new_smatrix(stmatrix::STMatrix<T> stMat) {
    return SMatrix<T>(stMat);
}

// smla.d:276-288 `bool approxEqualMatrix(T)(SMatrix!T a, SMatrix!T b)`
template <typename T>
bool approxEqualMatrix(SMatrix<T> a, SMatrix<T> b) {
    if ( a.aa().length() != b.aa().length() ) return false;
    if ( a.ia().length() != b.ia().length() ) return false;
    // Test equality in terms of non-zero positions in matrix
    if ( !indicesEqual(a.ja(), b.ja()) ) return false;
    if ( !indicesEqual(a.ia(), b.ia()) ) return false;
    // Then test individual non-zero elements.
    for (size_t i = 0; i < a.aa().length(); ++i) {
        if ( !approxEqualNumbers(a.aa()[i], b.aa()[i]) ) return false;
    }
    return true;
}

// smla.d:290-308 `void multiply(T)(SMatrix!T a, T[] b, T[] c, size_t nrows=0, size_t ncols=0)`
template <typename T>
void multiply(SMatrix<T> a, eil::Slice<T> b, eil::Slice<T> c,
              size_t nrows = 0, size_t ncols = 0) {
    size_t k0, k1;
    if (nrows == 0) { // just multiply using every row in matrix
        nrows = a.ia().length()-1;
    }
    if (nrows > a.ia().length()-1) { // don't fail, but reset to maximum available rows
        nrows = a.ia().length()-1;
    }
    for (size_t i = 0; i < nrows; ++i) {
        k0 = a.ia()[i];
        k1 = a.ia()[i+1];
        c[i] = 0.0;
        for (size_t k = k0; k < k1; ++k) {
            if (ncols >= 1 && a.ja()[k] >= ncols) continue;
            c[i] += a.aa()[k] * b[a.ja()[k]];
        }
    }
}

/**
 * An ILU(0) decomposition.
 *
 * This algorithm changes the matrix a in place leaving
 * an L/U matrix. The algorithm is an IKJ variant that
 * is useful for row contiguous data storage as is
 * used here.
 *
 * This implements Algorithm 10.4 in Saad (2003)
 *
 * smla.d:310-336
 */
template <typename T>
void decompILU0(SMatrix<T> a) {
    size_t n = a.ia().length()-1;
    for (size_t i = 1; i < n; ++i) { // Begin from 2nd row
        const size_t row_end = a.ia()[i+1];
        for (size_t kk = a.ia()[i]; kk < row_end; ++kk) {
            const size_t k = a.ja()[kk];
            // Only work on columns k < i-1
            if ( k >= i ) break;
            a.set(i,k) = a.get(i,k)/a.get(k,k);
            const size_t inner_end = a.ia()[i+1];
            for (size_t jj = a.ia()[i]; jj < inner_end; ++jj) {
                const size_t j = a.ja()[jj];
                //Only work with columns j >= k+1
                if ( j <= k ) continue;
                a.set(i,j) = a.get(i,j) - a.get(i,k)*a.get(k,j);
            }
        }
    }
}

/**
 * An ILU(p) decomposition.
 *
 * This algorithm changes the matrix a in place leaving
 * an L/U matrix. The algorithm is an IKJ variant that
 * is useful for row contiguous data storage as is
 * used here.
 *
 * This implements Algorithm 10.5 in Saad (2003)
 *
 * smla.d:338-394
 */
template <typename T>
void decompILUp(SMatrix<T> a, int k) {
    // NB. This pre-conditioner uses a sparse matrix, however some stored entries will be zero,
    // this is a result of entries starting as non-zero in the original matrix, and then becoming
    // zero during the factorisation.

    int n = static_cast<int>(a.ia().length())-1;
    eil::Array<eil::Array<int>> lev; // fill levels
    lev.set_length(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) lev[static_cast<size_t>(i)].set_length(static_cast<size_t>(n));

    // assign initial fill levels
    for (int i = 0; i < n; ++i) {
        const size_t ii = static_cast<size_t>(i);
        for (int j = 0; j < n; ++j) {
            const size_t jj = static_cast<size_t>(j);
            if (std::fabs(a.get(ii, jj)) <= ESSENTIALLY_ZERO) lev[ii][jj] = n-1;
            else lev[ii][jj] = 0;
        }
    }

    // symbolic phase
    for (int i = 1; i < n; ++i) { // Begin from 2nd row
        const size_t ii = static_cast<size_t>(i);
        for (int p = 0; p < i; ++p) {
            const size_t pp = static_cast<size_t>(p);
            if (lev[ii][pp] <= k) {
                for (int j = p; j < n; ++j) {
                    const size_t jj = static_cast<size_t>(j);
                    if (lev[ii][jj] > ESSENTIALLY_ZERO && lev[ii][jj] > (lev[ii][pp]+lev[pp][jj]+1) ) {
                        lev[ii][jj] = lev[ii][pp]+lev[pp][jj]+1;
                    }
                } // end foreach
            } // end if
        } // end foreach
    } // end foreach

    // modify a matrix nonzero pattern
    for (int i = 0; i < n; ++i) {
        const size_t ii = static_cast<size_t>(i);
        for (int j = 0; j < n; ++j) {
            const size_t jj = static_cast<size_t>(j);
            if (lev[ii][jj] <= k && std::fabs(a.get(ii, jj)) < ESSENTIALLY_ZERO) { a.set(ii, jj) = to_T<T>(0.0); }
        }
    }

    // clear the fill level matrix from memory
    //
    // D's destroy(lev) (smla.d:389) finalises POD elements and leaves collection
    // to the GC; GC.minimize() (core.memory, imported at smla.d:17) then asks the
    // collector to shrink. Neither affects the result, so the port releases the
    // storage and has no counterpart for the collector hint.
    lev = eil::Array<eil::Array<int>>();
    // GC.minimize();

    // factorise phase (using ILU0 algorithm on new sparsity pattern)
    decompILU0<T>(a);
}

// smla.d:396-440 `void solve(T)(SMatrix!T LU, T[] b)`
template <typename T>
void solve(SMatrix<T> LU, eil::Slice<T> b) {
    /*
    Solve a linear system using a prefactored sparse LU decomposition.

    Refactor by NNG, 18/08/23, Tangalooma QLD
    */

    // Note that ia has an extra element at the end, so the number of rows
    // n is actually ia.length-1. Meanwhile nn is the length of the under
    // -lying data array LU.aa, i.e. the number of nonzero entries in the matrix
    size_t n = LU.ia().length()-1;
    size_t nn = LU.ja().length();
    eil_assert(b.length() == n);

    // Forward elimination
    for (size_t i = 1; i < n; ++i) {
        for (size_t ii = LU.ia()[i]; ii < LU.ia()[i+1]; ++ii) {
            // Only work up to i
            size_t j = LU.ja()[ii];
            if ( j >= i ) break;
            T multiplier = LU.aa()[ii];
            b[i] -= multiplier * b[j];
        }
    }

    // Back substitution
    b[n-1] /= LU.aa()[nn-1];
    // D counts down with an `int` so the i == 0 pass runs; the port keeps an int
    // counter for the same reason.
    for (int i = static_cast<int>(n-2); i >= 0; --i) {
        const size_t ii = static_cast<size_t>(i);
        T sum = b[ii];
        size_t diag_index = 0;
        for (size_t k = LU.ia()[ii]; k < LU.ia()[ii+1]; ++k) {
            size_t j = LU.ja()[k];
            // If we run across the diagonal element keep a note of where it is
            if ( j == ii ) {
                diag_index = k;
                continue;
            // Otherwise only work with j >= i+1
            } else if( j > ii ) {
                sum -= LU.aa()[k] * b[j];
            }
        }
        b[ii] = sum/LU.aa()[diag_index];
    }
}

/**
 * This algorithm inverts the diagonal blocks of the sparse matrix A
 * the inverted blocks are stored in place, i.e. A = L + D^(-1) U
 * this can be useful for some of the iterative linear solvers such as Jacobi and Gauss-Seidel
 *
 * smla.d:443-471
 **/
template <typename T>
void invert_block_diagonal(SMatrix<T> A, Matrix<T> D, Matrix<T> Dinv,
                           size_t nblks, size_t blk_size) {
    for (size_t k = 0; k < nblks; ++k) {
        // grab the current diagonal block
        for (size_t i = 0; i < blk_size; ++i) {
            size_t idx = k*blk_size + i;
            for (size_t j = 0; j < blk_size; ++j) {
                size_t jdx = k*blk_size + j;
                D(i,j) = A.get(idx,jdx);
            }
        }
        // invert the block matrix
        inverse(D, Dinv);
        // replace the diagonal block in the sparse matrix with its inverse
        for (size_t i = 0; i < blk_size; ++i) {
            size_t idx = k*blk_size + i;
            for (size_t j = 0; j < blk_size; ++j) {
                size_t jdx = k*blk_size + j;
                A.set(idx,jdx) = Dinv(i,j);
            }
        }
    }
} // end invert_block_diagonal()

/**
 * This algorithm multiplies only the block diagonal portion of the
 * sparse matrix (A) with a vector (b) and stores the result in (c)
 * this can be useful for some of the iterative linear solvers such as Jacobi and Gauss-Seidel
 *
 * smla.d:473-502
 **/
template <typename T>
void multiply_block_diagonal(SMatrix<T> a, eil::Slice<T> b, eil::Slice<T> c,
                             size_t nblks, size_t blk_size) {
    eil_assert(a.ia().length()-1 == c.length());
    // Some faith is given that the user provides an appropriate length
    // input vector b, since in CRS we cannot easily determine the number of columns.
    size_t i0, i1, j0, j1;
    for (size_t nb = 0; nb < nblks; ++nb) {
        i0 = nb*blk_size;
        i1 = nb*blk_size+blk_size;
        for (size_t i = i0; i < i1; ++i) {
            j0 = a.ia()[i];
            j1 = a.ia()[i+1];
            for (size_t jj = j0; jj < j1; ++jj) {
                // only work on entries that are in the diagonal block
                const size_t j = a.ja()[jj];
                if ( j < i0 )
                    continue;
                if ( j > i1-1 )
                    break;
                c[i] += a.get(i,j) * b[j];
            }
        }
    }
} // end multiply_block_diagonal()

/**
 * This algorithm multiplies only the block lower triangular portion of the
 * sparse matrix (A) with a vector (b) and stores the result in (c)
 * this is useful for some of the iterative linear solvers such as Jacobi and Gauss-Seidel
 *
 * smla.d:504-531
 **/
template <typename T>
void multiply_block_lower_triangular(SMatrix<T> a, eil::Slice<T> b, eil::Slice<T> c,
                                     size_t nblks, size_t blk_size) {
    eil_assert(a.ia().length()-1 == c.length());
    size_t i0, i1, j0, j1;
    for (size_t nb = 0; nb < nblks; ++nb) {
        i0 = nb*blk_size;
        i1 = nb*blk_size+blk_size;
        for (size_t i = i0; i < i1; ++i) {
            j0 = a.ia()[i];
            j1 = a.ia()[i+1];
            for (size_t jj = j0; jj < j1; ++jj) {
                // only work on entries where j < diagonal block
                const size_t j = a.ja()[jj];
                if ( j >= i0 )
                    break;
                c[i] += a.get(i,j) * b[j];
            }
        }
    }
} // end multiply_block_lower_triangular()

/**
 * This algorithm performs a block lower triangular solve
 * the routine assumes that the sparse matrix is in the form A = L + D^(-1) + U
 * this method can be used for iterative solvers such as Gauss-Seidel
 *
 * smla.d:533-575
 **/
template <typename T>
void block_lower_triangular_solve(SMatrix<T> a, eil::Slice<T> b, eil::Slice<T> c,
                                  size_t nblks, size_t blk_size) {
    eil_assert(a.ia().length()-1 == c.length());
    size_t i0, i1, j0, j1;
    for (size_t nb = 0; nb < nblks; ++nb) {
        i0 = nb*blk_size;
        i1 = nb*blk_size+blk_size;
        // move the up to date entries to the right hand side
        for (size_t i = i0; i < i1; ++i) {
            j0 = a.ia()[i];
            j1 = a.ia()[i+1];
            for (size_t jj = j0; jj < j1; ++jj) {
                // only work on entries where j < diagonal block
                const size_t j = a.ja()[jj];
                if ( j >= i0 )
                    break;
                b[i] -= a.get(i,j) * c[j];
            }
        }
        // multiply by inverted diagonal block
        for (size_t i = i0; i < i1; ++i) {
            c[i] = 0.0;
            j0 = a.ia()[i];
            j1 = a.ia()[i+1];
            for (size_t jj = j0; jj < j1; ++jj) {
                // only work on entries that are in the diagonal block
                const size_t j = a.ja()[jj];
                if ( j < i0 )
                    continue;
                if ( j > i1-1 )
                    break;
                c[i] += a.get(i,j) * b[j];
            }
        }
    }
} // end block_lower_triangular_solve()

/**
 * This algorithm multiplies only the block upper triangular portion of the
 * sparse matrix (A) with a vector (b) and stores the result in (c)
 * this is useful for some of the iterative linear solvers such as Jacobi and Gauss-Seidel
 *
 * smla.d:577-604
 **/
template <typename T>
void multiply_block_upper_triangular(SMatrix<T> a, eil::Slice<T> b, eil::Slice<T> c,
                                     size_t nblks, size_t blk_size) {
    eil_assert(a.ia().length()-1 == c.length());
    size_t i0, i1, j0, j1;
    for (size_t nb = 0; nb < nblks; ++nb) {
        i0 = nb*blk_size;
        i1 = nb*blk_size+blk_size;
        for (size_t i = i0; i < i1; ++i) {
            j0 = a.ia()[i];
            j1 = a.ia()[i+1];
            for (size_t jj = j0; jj < j1; ++jj) {
                // only work on entries where j > diagonal block
                const size_t j = a.ja()[jj];
                if ( j <= i1-1 )
                    continue;
                c[i] += a.get(i,j) * b[j];
            }
        }
    }
} // multiply_block_upper_triangular()

/**
 * This algorithm performs a block upper triangular solve
 * the routine assumes that the sparse matrix is in the form A = L + D^(-1) + U
 * this method can be used for iterative solvers such as Gauss-Seidel
 *
 * smla.d:606-648
 **/
template <typename T>
void block_upper_triangular_solve(SMatrix<T> a, eil::Slice<T> b, eil::Slice<T> c,
                                  size_t nblks, size_t blk_size) {
    eil_assert(a.ia().length()-1 == c.length());
    size_t i0, i1, j0, j1;
    for (int nb = static_cast<int>(nblks)-1; nb >= 0; --nb) {
        i0 = static_cast<size_t>(nb)*blk_size;
        i1 = static_cast<size_t>(nb)*blk_size+blk_size;
        // move the up to date entries to the right hand side
        for (size_t i = i0; i < i1; ++i) {
            j0 = a.ia()[i];
            j1 = a.ia()[i+1];
            for (size_t jj = j0; jj < j1; ++jj) {
                // only work on entries where j > diagonal block
                const size_t j = a.ja()[jj];
                if ( j <= i1-1 )
                    continue;
                b[i] -= a.get(i,j) * c[j];
            }
        }
        // multiply by inverted diagonal block
        for (size_t i = i0; i < i1; ++i) {
            c[i] = 0.0;
            j0 = a.ia()[i];
            j1 = a.ia()[i+1];
            for (size_t jj = j0; jj < j1; ++jj) {
                // only work on entries that are in the diagonal block
                const size_t j = a.ja()[jj];
                if ( j < i0 )
                    continue;
                if ( j > i1-1 )
                    break;
                c[i] += a.get(i,j) * b[j];
            }
        }
    }
} // end block_upper_triangular_solve()

// smla.d:650-744 `T[] gmres(T)(SMatrix!T A, T[] b, T[] x0, int m)`
template <typename T>
eil::Array<T> gmres(SMatrix<T> A, eil::Slice<T> b, eil::Slice<T> x0, int m) {
    eil_assert(A.ia().length()-1 == b.length());
    eil_assert(b.length() == x0.length());
    eil_assert(m >= 1);
    const double ZERO_TOL = 1.0e-15;
    // 0. Allocate working matrices and vectors
    size_t n = b.length();
    eil::Array<T> Ax0, r0, v, w, g_old, g, x;
    Ax0.set_length(n);
    r0.set_length(n);
    v.set_length(n);
    w.set_length(n);
    x.set_length(n);
    g_old.set_length(static_cast<size_t>(m)+1);
    g.set_length(static_cast<size_t>(m)+1);
    auto H = new_matrix<T>(static_cast<size_t>(m)+1, static_cast<size_t>(m));
    H.zeros();
    auto Hold = new_matrix<T>(static_cast<size_t>(m)+1, static_cast<size_t>(m));
    auto Gamma = new_matrix<T>(static_cast<size_t>(m)+1, static_cast<size_t>(m)+1);
    auto V = new_matrix<T>(n, static_cast<size_t>(m)+1);

    // 1. Compute r0, beta, v1
    multiply(A, x0, Ax0.all());
    for (size_t i = 0; i < n; ++i) {
        r0[i] = b[i] - Ax0[i];
    }

    auto beta = norm2(r0.all());
    for (size_t i = 0; i < n; ++i) {
        v[i] = r0[i]/beta;
        V(i,0) = v[i];
    }

    // 2. Do 'm' iterations of update
    for (int j = 0; j < m; ++j) {
        const size_t jj = static_cast<size_t>(j);
        multiply(A, v.all(), w.all());
        for (int i = 0; i < j+1; ++i) {
            const size_t ii = static_cast<size_t>(i);
            v = V.getColumn(ii);
            H(ii,jj) = dot(w.all(), v.all());
            for (size_t k = 0; k < n; ++k) {
                w[k] -= H(ii,jj)*v[k];
            }
        }
        H(jj+1,jj) = norm2(w.all());
        if ( H(jj+1,jj) <= ZERO_TOL ) {
            m = j + 1;
            break;
        }
        for (size_t i = 0; i < n; ++i) {
            v[i] = w[i]/H(jj+1,jj);
            V(i,jj+1) = v[i];
        }
    }

    // Use the plane rotations method to compute solution
    // and residual at each step.
    for (size_t idx = 0; idx < g_old.length(); ++idx) g_old[idx] = 0.0;
    g_old[0] = beta;
    copy(H, Hold);
    for (int i = 0; i < m; ++i) {
        const size_t ii = static_cast<size_t>(i);
        // D leaves these NaN-initialised; each is assigned before it is read.
        T c_i = T(), s_i = T(), denom = T();
        denom = eil::sqrt(H(ii,ii)*H(ii,ii) + H(ii+1,ii)*H(ii+1,ii));
        s_i = H(ii+1,ii)/denom;
        c_i = H(ii,ii)/denom;
        Gamma.eye();
        Gamma(ii,ii) = c_i; Gamma(ii,ii+1) = s_i;
        Gamma(ii+1,ii) = -s_i; Gamma(ii+1,ii+1) = c_i;
        bbla::dot(Gamma, Hold, H);
        bbla::dot(Gamma, g_old.all(), g.all());
        // Get Qold and g_old ready for next step
        g_old.all().assign_from(g.all());
        copy(H, Hold);
    }

    [[maybe_unused]] auto resid = std::fabs(g[static_cast<size_t>(m)]);
    auto R = H.sliceDup(0, static_cast<size_t>(m), 0, static_cast<size_t>(m));
    auto gm = g.slice(0, static_cast<size_t>(m));
    // At end H := R
    //        g := gm
    bbla::upperSolve<T>(R, gm);
    auto Vm = V.sliceDup(0, n, 0, static_cast<size_t>(m));
    bbla::dot<T>(Vm, gm, x.all());

    for (size_t i = 0; i < n; ++i) x[i] += x0[i];

    multiply<T>(A, x.all(), Ax0.all());
    for (size_t i = 0; i < n; ++i) {
        r0[i] = b[i] - Ax0[i];
    }

    return x.dup();
}

// smla.d:746-862 `T[] gmres2(T)(SMatrix!T A, T[] b, T[] x0, int maxIters, double residTol)`
template <typename T>
eil::Array<T> gmres2(SMatrix<T> A, eil::Slice<T> b, eil::Slice<T> x0,
                     int maxIters, double residTol) {
    eil_assert(A.ia().length()-1 == b.length());
    eil_assert(b.length() == x0.length());
    eil_assert(maxIters >= 1);
    // D leaves resid NaN-initialised (smla.d:753), but every pass of the j loop
    // assigns it before the tolerance test and the loop always runs, so the
    // starting value is not observable.
    double resid = 0.0;
    // 0. Allocate working matrices and vectors
    size_t n = b.length();
    size_t m = static_cast<size_t>(maxIters);
    eil::Array<T> Ax0, r0, v, w, x;
    Ax0.set_length(n);
    r0.set_length(n);
    v.set_length(n);
    w.set_length(n);
    x.set_length(n);
    auto V = new_matrix<T>(n, m+1);
    auto H0 = new_matrix<T>(m+1, m); H0.zeros();
    auto H1 = new_matrix<T>(m+1, m); H1.zeros();
    auto Gamma = new_matrix<T>(m+1, m+1); Gamma.eye();
    auto Q0 = new_matrix<T>(m+1, m+1);
    auto Q1 = new_matrix<T>(m+1, m+1);
    eil::Array<T> g0, g1;
    g0.set_length(m+1); for (size_t idx = 0; idx < g0.length(); ++idx) { g0[idx] = 0.0; }
    g1.set_length(m+1); for (size_t idx = 0; idx < g1.length(); ++idx) { g1[idx] = 0.0; }
    eil::Array<T> h, hR;
    h.set_length(m+1);
    hR.set_length(m+1);

    // 1. Compute r0, beta, v1
    multiply<T>(A, x0, Ax0.all());
    for (size_t i = 0; i < n; ++i) {
        r0[i] = b[i] - Ax0[i];
    }

    auto beta = norm2(r0.all());
    g0[0] = beta;
    for (size_t i = 0; i < n; ++i) {
        v[i] = r0[i]/beta;
        V(i,0) = v[i];
    }

    // 2. Do 'm' iterations of update
    for (size_t j = 0; j < m; ++j) {
        multiply(A, v.all(), w.all());
        for (size_t i = 0; i < j+1; ++i) {
            v = V.getColumn(i);
            H0(i,j) = dot<T>(w.all(), v.all());
            for (size_t k = 0; k < n; ++k) {
                w[k] -= H0(i,j)*v[k];
            }
        }
        H0(j+1,j) = norm2(w.all());

        for (size_t i = 0; i < n; ++i) {
            v[i] = w[i]/H0(j+1,j);
            V(i,j+1) = v[i];
        }

        // Build rotated Hessenberg progressively
        if ( j != 0 ) {
            // Extract final column in H
            for (size_t i = 0; i < j+1; ++i) h[i] = H0(i,j);
            // Rotate column by previous rotations (stored in Q0)
            bbla::dot<T>(Q0, j+1, j+1, h.all(), hR.all());
            // Place column back in H
            for (size_t i = 0; i < j+1; ++i) H0(i,j) = hR[i];
        }
        // Now form new Gamma
        Gamma.eye();
        T c_j = T(), s_j = T(), denom = T();
        denom = eil::sqrt(H0(j,j)*H0(j,j) + H0(j+1,j)*H0(j+1,j));
        s_j = H0(j+1,j)/denom;
        c_j = H0(j,j)/denom;
        Gamma(j,j) = c_j; Gamma(j,j+1) = s_j;
        Gamma(j+1,j) = -s_j; Gamma(j+1,j+1) = c_j;
        // Apply rotations
        bbla::dot<T>(Gamma, j+2, j+2, H0, j+1, H1);
        bbla::dot<T>(Gamma, j+2, j+2, g0.all(), g1.all());
        // Accumulate Gamma rotations in Q.
        if ( j == 0 ) {
            copy(Gamma, Q1);
        }
        else {
            bbla::dot<T>(Gamma, j+2, j+2, Q0, j+2, Q1);
        }
        // Get residual
        resid = eil::re(std::fabs(g1[j+1]));
        if ( resid <= residTol ) {
            m = j+1;
            break;
        }

        // Prepare for next step
        copy(H1, H0);
        g0.all().assign_from(g1.all());
        copy(Q1, Q0);
    }

    auto R = H1.sliceDup(0, m, 0, m);
    auto gm = g1.slice(0, m);
    // At end H := R
    //        g := gm
    bbla::upperSolve<T>(R, gm);
    auto Vm = V.sliceDup(0, n, 0, m);
    bbla::dot<T>(Vm, gm, x.all());

    for (size_t i = 0; i < n; ++i) x[i] += x0[i];

    multiply<T>(A, x.all(), Ax0.all());
    for (size_t i = 0; i < n; ++i) {
        r0[i] = b[i] - Ax0[i];
    }

    return x.dup();
}

// smla.d:864-890 `struct GMRESWorkSpace(T)`. D's struct holds slices, so a D copy
// would alias them; every use in this module and in the unittests passes it as
// `ref GMRESWorkSpace!T`, which the port keeps as a reference, so no copy occurs.
template <typename T> struct GMRESWorkSpace {
    size_t n, m;
    eil::Array<T> Ax0, r0, v, w, Pv, g0, g1, h, hR;
    Matrix<T> V, H0, H1, Gamma, Q0, Q1;

    // smla.d:869-889 `this(size_t n, int maxIters)`
    GMRESWorkSpace(size_t n_, int maxIters) {
        n = n_;
        m = static_cast<size_t>(maxIters);
        // Allocate arrays
        Ax0.set_length(n);
        r0.set_length(n);
        v.set_length(n);
        w.set_length(n);
        Pv.set_length(n);
        g0.set_length(m+1);
        g1.set_length(m+1);
        h.set_length(m+1);
        hR.set_length(m+1);
        // Allocate matrices
        V = new_matrix<T>(n, m+1);
        H0 = new_matrix<T>(m+1, m);
        H1 = new_matrix<T>(m+1, m);
        Gamma = new_matrix<T>(m+1, m+1);
        Q0 = new_matrix<T>(m+1, m+1);
        Q1 = new_matrix<T>(m+1, m+1);
    }
};

/**
 * A restarted GMRES iterative solver with right preconditioning.
 *
 * Params:
 *    A =           coefficient matrix
 *    P =           pre-conditioning matrix in LU form
 *    b =           RHS vector
 *    x0 =          initial guess for solution
 *    x =           solution vector place in x
 *    maxIters =    maximum number of iterations
 *    residTol =    stop iterations when residual below this tolerance
 *    gws =         pre-allocated workspace
 *    maxRestarts = maximum number of restarts
 *
 * smla.d:892-1015
 */
template <typename T>
void rpcGMRES(SMatrix<T> A, SMatrix<T> P, eil::Slice<T> b, eil::Slice<T> x0, eil::Slice<T> x,
              int maxIters, double residTol, GMRESWorkSpace<T>& gws, int maxRestarts = 1) {
    eil_assert(A.ia().length()-1 == b.length());
    eil_assert(b.length() == x0.length());
    eil_assert(maxIters >= 1);
    T resid = T();   // as in gmres2: assigned on every pass before it is read
    size_t n = b.length();
    size_t m = static_cast<size_t>(maxIters);

    // Perform restarts
    for (size_t r = 0; r < static_cast<size_t>(maxRestarts); ++r) {

        // 0. Initialise the values in some of the pre-allocated storage
        for (size_t idx = 0; idx < gws.g0.length(); ++idx) gws.g0[idx] = 0.0;
        for (size_t idx = 0; idx < gws.g1.length(); ++idx) gws.g1[idx] = 0.0;
        gws.H0.zeros();
        gws.H1.zeros();
        gws.Gamma.eye();

        // 1. Compute r0, beta, v1
        multiply<T>(A, x0, gws.Ax0.all());
        for (size_t i = 0; i < n; ++i) {
            gws.r0[i] = b[i] - gws.Ax0[i];
        }

        auto beta = norm2(gws.r0.all());
        gws.g0[0] = beta;
        //writefln("beta: %.16e", beta);
        for (size_t i = 0; i < n; ++i) {
            gws.v[i] = gws.r0[i]/beta;
            gws.V(i,0) = gws.v[i];
        }

        // 2. Do 'm' iterations of update
        for (size_t j = 0; j < m; ++j) {
            gws.Pv.all().assign_from(gws.v.all());
            solve<T>(P, gws.Pv.all());
            multiply<T>(A, gws.Pv.all(), gws.w.all());
            for (size_t i = 0; i < j+1; ++i) {
                for (size_t k = 0; k < n; ++k ) gws.v[k] = gws.V(k,i); // Extract column 'i'
                gws.H0(i,j) = dot(gws.w.all(), gws.v.all());
                for (size_t k = 0; k < n; ++k) gws.w[k] -= gws.H0(i,j)*gws.v[k];
            }
            gws.H0(j+1,j) = norm2(gws.w.all());

            for (size_t k = 0; k < n; ++k) {
                gws.v[k] = gws.w[k]/gws.H0(j+1,j);
                gws.V(k,j+1) = gws.v[k];
            }

            // Build rotated Hessenberg progressively
            if ( j != 0 ) {
                // Extract final column in H
                for (size_t i = 0; i < j+1; ++i) gws.h[i] = gws.H0(i,j);
                // Rotate column by previous rotations (stored in Q0)
                bbla::dot<T>(gws.Q0, j+1, j+1, gws.h.all(), gws.hR.all());
                // Place column back in H
                for (size_t i = 0; i < j+1; ++i) gws.H0(i,j) = gws.hR[i];
            }
            // Now form new Gamma
            gws.Gamma.eye();
            auto denom = eil::sqrt(gws.H0(j,j)*gws.H0(j,j) + gws.H0(j+1,j)*gws.H0(j+1,j));
            auto s_j = gws.H0(j+1,j)/denom;
            auto c_j = gws.H0(j,j)/denom;
            gws.Gamma(j,j) = c_j; gws.Gamma(j,j+1) = s_j;
            gws.Gamma(j+1,j) = -s_j; gws.Gamma(j+1,j+1) = c_j;
            // Apply rotations
            bbla::dot<T>(gws.Gamma, j+2, j+2, gws.H0, j+1, gws.H1);
            bbla::dot<T>(gws.Gamma, j+2, j+2, gws.g0.all(), gws.g1.all());
            // Accumulate Gamma rotations in Q.
            if ( j == 0 ) {
                copy(gws.Gamma, gws.Q1);
            }
            else {
                bbla::dot<T>(gws.Gamma, j+2, j+2, gws.Q0, j+2, gws.Q1);
            }
            // Get residual
            resid = std::fabs(gws.g1[j+1]);
            //writefln("restart: %d, iteration: %d, residual: %.8e", r, j, resid);
            if ( resid <= residTol ) {
                m = j+1;
                break;
            }

            // Prepare for next iteration
            copy(gws.H1, gws.H0);
            gws.g0.all().assign_from(gws.g1.all());
            copy(gws.Q1, gws.Q0);
        }

        // At end H := R up to row m
        //        g := gm up to row m
        bbla::upperSolve<T>(gws.H1, static_cast<int>(m), gws.g1.all());
        bbla::dot<T>(gws.V, n, m, gws.g1.all(), x);
        solve<T>(P, x);

        for (size_t i = 0; i < n; ++i) x[i] += x0[i];

        if ( resid <= residTol || r+1 == static_cast<size_t>(maxRestarts) ) {
            break;
        }

        // Else, prepare for a restart by setting the inital
        // guess to the current best estimate of the solution
        for (size_t i = 0; i < n; ++i) x0[i] = x[i];
    }
}

// smla.d:1017-1113 `T[] fGMRES(T)(SMatrix!T A, SMatrix!T P, T[] b, T[] x0, int mOuter, int mInner)`
template <typename T>
eil::Array<T> fGMRES(SMatrix<T> A, SMatrix<T> P, eil::Slice<T> b, eil::Slice<T> x0,
                     int mOuter, int mInner) {
    eil_assert(A.ia().length()-1 == b.length());
    eil_assert(b.length() == x0.length());
    eil_assert(mOuter >= 1);
    const double ZERO_TOL = 1.0e-15;
    // 0. Allocate working matrices and vectors
    size_t n = b.length();
    eil::Array<T> Ax0, r0, v, w, g_old, g, x, z, x0_inner;
    Ax0.set_length(n);
    r0.set_length(n);
    v.set_length(n);
    z.set_length(n);
    w.set_length(n);
    x.set_length(n);
    x0_inner.set_length(n);
    for (size_t idx = 0; idx < x0_inner.length(); ++idx) x0_inner[idx] = 0.0;
    g_old.set_length(static_cast<size_t>(mOuter)+1);
    g.set_length(static_cast<size_t>(mOuter)+1);
    auto H = new_matrix<T>(static_cast<size_t>(mOuter)+1, static_cast<size_t>(mOuter));
    H.zeros();
    auto Hold = new_matrix<T>(static_cast<size_t>(mOuter)+1, static_cast<size_t>(mOuter));
    auto Gamma = new_matrix<T>(static_cast<size_t>(mOuter)+1, static_cast<size_t>(mOuter)+1);
    auto V = new_matrix<T>(n, static_cast<size_t>(mOuter)+1);
    auto Z = new_matrix<T>(n, static_cast<size_t>(mOuter)+1);

    // 1. Compute r0, beta, v1
    multiply<T>(A, x0, Ax0.all());
    for (size_t i = 0; i < n; ++i) {
        r0[i] = b[i] - Ax0[i];
    }

    auto beta = norm2(r0.all());
    for (size_t i = 0; i < n; ++i) {
        v[i] = r0[i]/beta;
        V(i,0) = v[i];
    }

    // 2. Do 'mOuter' iterations of update
    for (int j = 0; j < mOuter; ++j) {
        const size_t jj = static_cast<size_t>(j);
        // D rebinds z to the freshly returned array; the port's assignment copies
        // it, and only z[k] is read afterwards, so the values are the same.
        z = gmres<T>(P, v.all(), x0_inner.all(), mInner);
        // Save z vector in Z
        for (size_t k = 0; k < n; ++k ) Z(k,jj) = z[k];
        multiply<T>(A, z.all(), w.all());
        for (int i = 0; i < j+1; ++i) {
            const size_t ii = static_cast<size_t>(i);
            v = V.getColumn(ii);
            H(ii,jj) = dot(w.all(), v.all());
            for (size_t k = 0; k < n; ++k) {
                w[k] -= H(ii,jj)*v[k];
            }
        }
        H(jj+1,jj) = norm2(w.all());
        if ( H(jj+1,jj) <= ZERO_TOL ) {
            mOuter = j;
            break;
        }
        for (size_t i = 0; i < n; ++i) {
            v[i] = w[i]/H(jj+1,jj);
            V(i,jj+1) = v[i];
        }
    }

    // Use the plane rotations method to compute solution
    // and residual at each step.
    for (size_t idx = 0; idx < g_old.length(); ++idx) g_old[idx] = 0.0;
    g_old[0] = beta;
    copy(H, Hold);
    for (int i = 0; i < mOuter; ++i) {
        const size_t ii = static_cast<size_t>(i);
        T c_i = T(), s_i = T(), denom = T();
        denom = eil::sqrt(H(ii,ii)*H(ii,ii) + H(ii+1,ii)*H(ii+1,ii));
        s_i = H(ii+1,ii)/denom;
        c_i = H(ii,ii)/denom;
        Gamma.eye();
        Gamma(ii,ii) = c_i; Gamma(ii,ii+1) = s_i;
        Gamma(ii+1,ii) = -s_i; Gamma(ii+1,ii+1) = c_i;
        bbla::dot(Gamma, Hold, H);
        bbla::dot(Gamma, g_old.all(), g.all());
        // Get Qold and g_old ready for next step
        g_old.all().assign_from(g.all());
        copy(H, Hold);
    }

    [[maybe_unused]] auto resid = std::fabs(g[static_cast<size_t>(mOuter)]);
    auto R = H.sliceDup(0, static_cast<size_t>(mOuter), 0, static_cast<size_t>(mOuter));
    auto gm = g.slice(0, static_cast<size_t>(mOuter));
    // At end H := R
    //        g := gm
    bbla::upperSolve<T>(R, gm);
    auto Zm = Z.sliceDup(0, n, 0, static_cast<size_t>(mOuter));
    bbla::dot<T>(Zm, gm, x.all());

    for (size_t i = 0; i < n; ++i) x[i] += x0[i];

    return x.dup();
}

} // namespace smla
} // namespace eil::nm
