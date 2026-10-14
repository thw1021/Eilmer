#pragma once
/** nelmin.d
 *
 * Nelder-Mead simplex minimization of a nonlinear (multivariate) function.
 *
 * Author: Peter J.
 * Version: 2020-07-25 made more like the Python 2020 version.
 *
 * This D version has been adapted from the C++ version 2014-06-14.
 * The C++ version had been adpated from the Python version in Jan 2006.
 * The Python code had been adapted from the C-coded nelmin.c which was
 * adapted from the Fortran-coded nelmin.f which was, in turn, adapted
 * from the papers
 *
 *     J.A. Nelder and R. Mead (1965)
 *     A simplex method for function minimization.
 *     Computer Journal, Volume 7, pp 308-313.
 *
 *     R. O'Neill (1971)
 *     Algorithm AS47. Function minimization using a simplex algorithm.
 *     Applied Statistics, Volume 20, pp 338-345.
 *
 * For the 2020 update, we make some of the stepping closer to
 * the description given in the paper:
 *
 *    Donghoon Lee and Matthew Wiswall (2007)
 *    A parallel implementation of the simplec function minimization routine.
 *    Computational Economics 30:171-187
 *
 * and some examples are in
 *
 *    D.M. Olsson and L.S. Nelson (1975)
 *    The Nelder-Mead Simplex procedure for function minimization.
 *    Technometrics, Volume 17 No. 1, pp 45-51.
 *
 * For a fairly recent and popular incarnation of this minimizer,
 * see the amoeba function in the famous "Numerical Recipes" text.
 * The programming interface is via the minimize() function; see below.
 */
// nm/nelmin.d -> src/nm/nelmin.hpp (module nm.nelmin).
//
// D module scope -> namespace eil::nm::nelmin, because nm/linesearch.d declares a
// `minimize` too and D keeps the two apart by module (the note at the top of
// nm/linesearch.hpp anticipates this).
//
// nelmin.d is templated on the objective function and the scalar type, so the
// definitions live in this header and nelmin.cpp is the matching TU, exactly as in
// nm/bbla.hpp. Nothing in the loop structure was reordered: the reflection,
// extension, contraction and restart arithmetic and every nfe increment happen on
// the same statement as in D, because the iteration and evaluation counts are part
// of the observable output (geom/grid/sgrid.d's determine_rs_grids and
// geom/gpath/gpath_utils.d both read them).
//
// Translation notes, none of them numeric:
//
//   * `alias f` (nelmin.d:77, :127) has no C++ equivalent. Both minimize() and
//     NMMinimizer take the callable *type* as their first template parameter and
//     an instance as the first argument, so `minimize!(sse,double)(...)` ports to
//     `nelmin::minimize<Sse, double>(Sse{...}, ...)`. D's template constraint
//     (`is(typeof(f([0.0,0.0])) == double)`) becomes the static_assert below; the
//     Complex!double arm of it is the version(complex_numbers) axis and is kept
//     uncompiled, see the guard at the bottom.
//   * `out T f_min, out int n_fe, out int n_restart` (nelmin.d:78) are initialised
//     to their .init by the compiler before the body runs, which matters because
//     f_min is only assigned inside the while loop. The port zeroes all three at
//     entry so a caller whose budget is exhausted before the first check reads the
//     same 0 that D's caller reads.
//   * `ref T[] x` (nelmin.d:78) is a reference to the caller's dynamic array; only
//     its elements are written (nelmin.d:98), which is what Array<T>& gives.
//     `T[] dx` is a *view* and nelmin.d:89-90 rebinds it locally, so the caller's
//     array is untouched either way; the port takes Slice<T> by value and works on
//     a local Array copy, which nelmin.d:140 would dup anyway.
//   * `vertices[i].x = x_ext[]` (nelmin.d:278, :282, :304) assigns a slice, so in D
//     the vertex then aliases that local buffer for the rest of its GC life. Each
//     of those buffers (`x_ext`, `x_refl`, `x_con`) is a fresh dup written only
//     before the assignment, and no other vertex ever shares it, so the deep copy
//     C++ performs has the same values. `vertices[i+1].x[] = x_new[]`
//     (nelmin.d:192) is the other spelling: element-wise into the existing buffer,
//     which is assign_from().
//   * `vertices.sort!(less)` is NOT std::sort. std.algorithm.sorting's introsort
//     reaches Phobos' `shortSort`/`sort5` for the simplex sizes nelmin actually
//     uses, and those are fixed comparison networks that permute equal keys in a
//     specific order. That is load-bearing: nelmin.d's own testFunction1 is
//     symmetric in the coordinates, so the initial simplex has several vertices
//     with EXACTLY equal fx, and which one the sort leaves as "worst" decides the
//     whole trajectory. Measured: with std::sort three of the probe's sixteen
//     cases come out with different coordinates or a different evaluation count
//     (tf1_far_start, ridged, tf1_short_dx). The slice of std.algorithm.sorting
//     that D's call resolves to is therefore ported below, in sort_detail, and
//     verified row-for-row against baselines/e587b235/probes/nm_nelmin_sort.txt
//     (6889 tie patterns at n=2..64) by tests/test_nm_nelmin_sort.cpp.
//     Two D-side details of that code are not reproduced because they cannot fire
//     for a correct implementation: the internal `assert(isHeap(r))` in
//     buildHeap, and `sort`'s closing `assert(isSorted ...)`. The partition guard
//     `assert(lessI <= greaterI, "sort: invalid comparison function.")` *is* kept,
//     because a comparator that is not a strict weak ordering reaches it, and the
//     objective functions in this codebase can return NaN (nelmin.d:87 says
//     "Might have to check for NaNs as well").
//   * `(vtx.fx - mean)^^2` (nelmin.d:207) is std.math.pow, not a multiply: it
//     becomes eil::pow(), which reproduces the measured D bits (see
//     substrate/include/eil/math.hpp and probes/substrate/probe_pow.d).

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <type_traits>

#include "eil/array.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/math.hpp"
#include "eil/number.hpp"

namespace eil::nm {
namespace nelmin {

//-----------------------------------------------------------------------
// Keep the data tidy and conveniently accessible...

template <typename T> struct Vertex {
    // Stores the coordinates as an array and the associated function value.
    eil::Array<T> x;
    T fx;
};

// Following function used when sorting vertices into ascending order.
template <typename T> bool less(const Vertex<T>& a, const Vertex<T>& b) {
    return a.fx < b.fx;
}

namespace sort_detail {
// The part of std.algorithm.sorting that D's `vertices.sort!(less)` reaches, ported
// so the permutation of equal fx values is D's and not merely a sorted one. See the
// header note; the transcription targets are sorting.d:1653 (getPivot), :1678
// (shortSort), :1805 (sort5), :1963 (sort's dispatch to quickSortImpl), :2163
// (quickSortImpl) and :2236 (HeapOps), as shipped with ldc 1.40 / dmd 2.110.
//
// D's `move`/`moveEmplace` of a Vertex!T copy a slice header and leave the source's
// bits in place, while eil::Array's move takes the buffer away. Nothing here reads
// a slot after moving out of it without writing it back, but the insertion pass
// would leave r[i] empty when no shift is needed, so the shifts are plain
// assignments: the multiset of (x, fx) values the sort produces is the same either
// way, and the vertex buffers are uniquely owned in both languages.

template <typename T> inline void swap_at(Vertex<T>* p, size_t i, size_t j) {
    std::swap(p[i], p[j]);
}

template <typename T> inline bool pred_less(const Vertex<T>& a, const Vertex<T>& b) {
    return less<T>(a, b);
}

// sorting.d:1805, "sorts the first 5 elements exactly".
template <typename T> void sort5(Vertex<T>* r) {
    // 1. Sort first two pairs
    if (pred_less<T>(r[1], r[0])) swap_at(r, 0, 1);
    if (pred_less<T>(r[3], r[2])) swap_at(r, 2, 3);

    // 2. Arrange first two pairs by the largest element
    if (pred_less<T>(r[3], r[1])) {
        swap_at(r, 0, 2);
        swap_at(r, 1, 3);
    }

    // 3. Insert 4 into [0, 1, 3]
    if (pred_less<T>(r[4], r[1])) {
        swap_at(r, 3, 4);
        swap_at(r, 1, 3);
        if (pred_less<T>(r[1], r[0])) swap_at(r, 0, 1);
    } else if (pred_less<T>(r[4], r[3])) {
        swap_at(r, 3, 4);
    }

    // 4. Insert 2 into [0, 1, 3, 4] (note: we already know the last is greater)
    if (pred_less<T>(r[2], r[1])) {
        swap_at(r, 1, 2);
        if (pred_less<T>(r[1], r[0])) swap_at(r, 0, 1);
    } else if (pred_less<T>(r[3], r[2])) {
        swap_at(r, 2, 3);
    }
    // 7 comparisons, 0-9 swaps
}

// sorting.d:1678, "optimized for short ranges... uses insertion sort going
// downward". r has n elements, n <= 5 handled by the switch, n >= 6 by sort5 on the
// last five plus the downward insertion.
template <typename T> void short_sort(Vertex<T>* r, size_t n) {
    switch (n) {
      case 0: case 1:
        return;
      case 2:
        if (pred_less<T>(r[1], r[0])) swap_at(r, 0, 1);
        return;
      case 3:
        if (pred_less<T>(r[2], r[0])) {
            if (pred_less<T>(r[0], r[1])) {
                swap_at(r, 0, 1);
                swap_at(r, 0, 2);
            } else {
                swap_at(r, 0, 2);
                if (pred_less<T>(r[1], r[0])) swap_at(r, 0, 1);
            }
        } else {
            if (pred_less<T>(r[1], r[0])) {
                swap_at(r, 0, 1);
            } else {
                if (pred_less<T>(r[2], r[1])) swap_at(r, 1, 2);
            }
        }
        return;
      case 4:
        if (pred_less<T>(r[1], r[0])) swap_at(r, 0, 1);
        if (pred_less<T>(r[3], r[2])) swap_at(r, 2, 3);
        if (pred_less<T>(r[2], r[0])) swap_at(r, 0, 2);
        if (pred_less<T>(r[3], r[1])) swap_at(r, 1, 3);
        if (pred_less<T>(r[2], r[1])) swap_at(r, 1, 2);
        return;
      default:
        sort5<T>(r + n - 5);   // D: sort5!pred(r[r.length - 5 .. r.length])
        if (n == 5) return;
        break;
    }

    /* The last 5 elements of the range are sorted. Proceed with expanding the
    sorted portion downward. */
    for (size_t i = n - 6; ; --i) {
        size_t j = i + 1;
        Vertex<T> temp = r[i];
        if (pred_less<T>(r[j], temp)) {
            do {
                r[j - 1] = r[j];
                ++j;
            } while (j < n && pred_less<T>(r[j], temp));
            r[j - 1] = temp;
        }
        if (i == 0) break;
    }
}

// sorting.d:4339 medianOf, the k == 3 arm.
template <typename T> void median_of3(Vertex<T>* r, size_t a, size_t b, size_t c) {
    if (pred_less<T>(r[c], r[a]))   // c < a
    {
        if (pred_less<T>(r[a], r[b]))   // c < a < b
        {
            swap_at(r, a, b);
            swap_at(r, a, c);
        } else   // c < a, b <= a
        {
            swap_at(r, a, c);
            if (pred_less<T>(r[b], r[a])) swap_at(r, a, b);
        }
    } else   // a <= c
    {
        if (pred_less<T>(r[b], r[a]))   // b < a <= c
        {
            swap_at(r, a, b);
        } else   // a <= c, a <= b
        {
            if (pred_less<T>(r[c], r[b])) swap_at(r, b, c);
        }
    }
}

// sorting.d:4339 medianOf, the k == 5 arm ("Credit: Teppo Niinimaki").
template <typename T> void median_of5(Vertex<T>* r, size_t a, size_t b, size_t c,
                                     size_t d, size_t e) {
    if (pred_less<T>(r[c], r[a])) swap_at(r, a, c);
    if (pred_less<T>(r[d], r[b])) swap_at(r, b, d);
    if (pred_less<T>(r[d], r[c])) {
        swap_at(r, c, d);
        swap_at(r, a, b);
    }
    if (pred_less<T>(r[e], r[b])) swap_at(r, b, e);
    if (pred_less<T>(r[e], r[c])) {
        swap_at(r, c, e);
        if (pred_less<T>(r[c], r[a])) swap_at(r, a, c);
    } else {
        if (pred_less<T>(r[c], r[b])) swap_at(r, b, c);
    }
}

// sorting.d:1653 getPivot.
template <typename T> size_t get_pivot(Vertex<T>* r, size_t n) {
    size_t mid = n / 2;
    if (n < 512) {
        if (n >= 32) median_of3<T>(r, size_t(0), mid, n - 1);
        return mid;
    }
    // The plan is to take the median of five by taking five elements in the array,
    // segregate around their median, and return the position of the third.
    size_t quarter = n / 4;
    median_of5<T>(r, size_t(0), mid - quarter, mid, mid + quarter, n - 1);
    return mid;
}

// sorting.d:2236 HeapOps, the heapsort that guards against depth exhaustion.
template <typename T> void sift_down(Vertex<T>* r, size_t parent, const size_t end) {
    for (;;) {
        size_t child = (parent + 1) * 2;
        if (child >= end) {
            // Leftover left child?
            if (child == end && pred_less<T>(r[parent], r[--child])) swap_at(r, parent, child);
            break;
        }
        size_t leftChild = child - 1;
        if (pred_less<T>(r[child], r[leftChild])) child = leftChild;
        if (!pred_less<T>(r[parent], r[child])) break;
        swap_at(r, parent, child);
        parent = child;
    }
}

template <typename T> void build_heap(Vertex<T>* r, size_t n) {
    const size_t end = n;
    for (size_t i = n / 2; i-- > 0; ) sift_down<T>(r, i, end);
}

template <typename T> void percolate(Vertex<T>* r, size_t parent, const size_t end) {
    const size_t root = parent;
    // Sift down
    for (;;) {
        size_t child = (parent + 1) * 2;
        if (child >= end) {
            if (child == end) {
                // Leftover left node.
                --child;
                swap_at(r, parent, child);
                parent = child;
            }
            break;
        }
        size_t leftChild = child - 1;
        if (pred_less<T>(r[child], r[leftChild])) child = leftChild;
        swap_at(r, parent, child);
        parent = child;
    }
    // Sift up
    for (size_t child = parent; child > root; child = parent) {
        parent = (child - 1) / 2;
        if (!pred_less<T>(r[parent], r[child])) break;
        swap_at(r, parent, child);
    }
}

template <typename T> void heap_sort(Vertex<T>* r, size_t n) {
    if (n < 2) return;
    build_heap<T>(r, n);
    for (size_t i = n - 1; i > 0; --i) {
        swap_at(r, 0, i);
        percolate<T>(r, 0, i);
    }
}

// sorting.d:2163 quickSortImpl.
template <typename T> void quick_sort_impl(Vertex<T>* r, size_t n, size_t depth) {
    // Elem.sizeof for Vertex!double is 24 in D (a 16-byte slice plus a double), so
    // max(32, 1024/24) = 42: the whole simplex fits in shortSort for any problem
    // with fewer than 42 coordinates.
    const size_t short_sort_gets_better = std::max<size_t>(32, 1024 / (2*sizeof(size_t) + sizeof(T)));

    // partition
    while (n > short_sort_gets_better) {
        if (depth == 0) {
            heap_sort<T>(r, n);
            return;
        }
        depth = depth >= std::numeric_limits<size_t>::max() / 2 ? (depth / 3) * 2
                                                                : (depth * 2) / 3;

        const size_t pivotIdx = get_pivot<T>(r, n);
        Vertex<T> pivot = r[pivotIdx];

        swap_at(r, pivotIdx, n - 1);
        size_t lessI = std::numeric_limits<size_t>::max(), greaterI = n - 1;

        bool outer_done = false;
        for (;;) {
            while (pred_less<T>(r[++lessI], pivot)) {}
            if (lessI > greaterI) {
                throw eil::ErrorBase("sort: invalid comparison function.");
            }
            for (;;) {
                if (greaterI == lessI) { outer_done = true; break; }
                if (!pred_less<T>(pivot, r[--greaterI])) break;
            }
            if (outer_done) break;
            if (lessI > greaterI) {
                throw eil::ErrorBase("sort: invalid comparison function.");
            }
            if (lessI == greaterI) break;
            swap_at(r, lessI, greaterI);
        }

        swap_at(r, n - 1, lessI);
        Vertex<T>* left = r;          size_t left_n = lessI;
        Vertex<T>* right = r + lessI + 1; size_t right_n = n - lessI - 1;
        if (right_n > left_n) {
            std::swap(left, right);
            std::swap(left_n, right_n);
        }
        quick_sort_impl<T>(right, right_n, depth);
        r = left;
        n = left_n;
    }
    // residual sort
    short_sort<T>(r, n);
}

// sorting.d:1963, `sort!(less)(r)` for the unstable SwapStrategy.
template <typename T> void sort_vertices(eil::Array<Vertex<T>>& v) {
    quick_sort_impl<T>(v.data(), v.length(), v.length());
    // D closes sort() with `assert(isSorted!lessFun(r))`, which cannot fail for a
    // correct partition; not reproduced.
}

} // namespace sort_detail

template <typename F, typename T>
class NMMinimizer {
    //  Stores the (nonlinear) simplex and some stepping parameters.

  public:
    NMMinimizer(F f, size_t N, eil::Slice<T> dx, int P, double Kreflect, double Kextend,
                double Kcontract)
    // In an N-dimensional problem, each vertex has an array of N coordinates
    // and the simplex consists of N+1 vertices.
    // and the set of step-sizes dx.
    // f is a user-specified objective function f(x).
    {
        // D injects the objective function through `alias f` (nelmin.d:127), which
        // has no C++ equivalent, so the callable arrives as this extra first
        // argument and minimize() forwards its own `f` here. Nothing else about the
        // constructor changes.
        this->f = f;
        eil_assert(dx.length() == N);   // nelmin.d:139 "Incorrect length for dx."
        // D's `dx.dup` on the parameter view. eil::Slice has no dup() of its own
        // (only eil::Array does), so the Array-from-Slice construction is the
        // spelling; reported as a substrate gap.
        this->dx = eil::Array<T>(dx);
        this->N = N;
        this->P = static_cast<size_t>(P);
        this->Kreflect = Kreflect;
        this->Kextend = Kextend;
        this->Kcontract = Kcontract;
        this->nrestarts = 0;
        this->nfe = 0;
    }

    void build_initial_simplex(eil::Slice<T> x)
    // Set up the vertices about the user-specified point, x,
    {
        eil_assert(x.length() == N);   // nelmin.d:153 "Incorrect length for x."
        for (size_t i = 0; i < N+1; ++i) {
            auto x_new = eil::Array<T>(x);   // x.dup(), see the note above
            if (i >= 1) x_new[i-1] += dx[i-1];
            auto f_new = f(x_new.all());
            vertices.append(Vertex<T>{x_new, f_new});
        }
        sort_detail::sort_vertices(vertices);
        this->nfe = static_cast<int>(N+1);   // nelmin.d:161 `to!int(N+1)`
        return;
    }

    void take_steps(int nsteps)
    // Take a few steps, updating the simplex.
    // Returns a sorted simplex, with the best point at index 0.
    {
        for (int istep = 0; istep < nsteps; ++istep) {
            sort_detail::sort_vertices(vertices);
            bool any_success = false;
            for (size_t i = 0; i < P; ++i) {
                bool success = replace_vertex(N-i);
                if (success) any_success = true;
            }
            if (!any_success) {
                contract_about_zero_point();
            }
        }
        return;
    }

    void rescale(double ratio)
    // Rebuild the simplex about the lowest point for a restart.
    {
        sort_detail::sort_vertices(vertices);
        for (auto& elem : dx) { elem *= ratio; }
        auto vtx = vertices[0];
        for (size_t i = 0; i < N; ++i) {
            auto x_new = vtx.x.dup();
            x_new[i] += dx[i];
            // nelmin.d:192 `vertices[i+1].x[] = x_new[]` is element-wise into the
            // existing buffer, not a re-alias.
            vertices[i+1].x.all().assign_from(x_new.all());
            vertices[i+1].fx = f(x_new.all()); ++nfe;
        }
        sort_detail::sort_vertices(vertices);
        nrestarts += 1;
        return;
    }

    std::array<T, 2> f_statistics()
    // Returns mean and standard deviation of the vertex fn values.
    {
        T mean = 0.0;
        for (const auto& vtx : vertices) { mean += vtx.fx; }
        mean /= vertices.length();
        T ss = 0.0;
        for (const auto& vtx : vertices) { ss += eil::pow(vtx.fx - mean, 2); }
        T std_dev = eil::sqrt(ss/(vertices.length()-1));
        return {mean, std_dev};
    }

    eil::Array<T> centroid(size_t imax)
    // Returns the centroid of the subset of vertices up to and including imax.
    {
        eil::Array<T> xmid = vertices[0].x.dup();
        imax = std::min(imax, N);
        for (size_t i = 1; i < imax+1; ++i) {
            // nelmin.d:217 `xmid[] += vertices[i].x[]`, element-wise.
            for (size_t j = 0; j < xmid.length(); ++j) xmid[j] += vertices[i].x[j];
        }
        for (auto& elem : xmid) { elem /= (imax+1); }
        return xmid;
    }

    void contract_about_zero_point()
    // Contract the simplex about the vertex[0].
    {
        eil::Array<T> x_con = vertices[0].x.dup();
        for (size_t i = 1; i < N+1; ++i) {
            for (size_t j = 0; j < vertices[i].x.length(); ++j) {
                vertices[i].x[j] = 0.5*vertices[i].x[j] + 0.5*x_con[j];
            }
            vertices[i].fx = f(vertices[i].x.all()); ++nfe;
        }
        sort_detail::sort_vertices(vertices);
        return;
    }

    bool test_for_minimum(double delta)
    // Look around vertex 0 to see if it is a local minimum.
    // This is expensive, so we don't want to do it often.
    {
        bool is_minimum = true;
        auto f_min = vertices[0].fx;
        for (size_t j = 0; j < N; ++j) {
            auto x_new = vertices[j].x.dup();
            x_new[j] += dx[j] * delta;
            auto f_new = f(x_new.all()); ++nfe;
            if (f_new < f_min) { is_minimum = false; break; }
            x_new[j] -= dx[j] * delta * 2.0;
            f_new = f(x_new.all()); ++nfe;
            if (f_new < f_min) { is_minimum = false; break; }
        }
        return is_minimum;
    }

    bool replace_vertex(size_t i)
    // Try to replace the worst point, i, in the simplex.
    // Returns true is there was a successful replacement.
    {
        auto f_min = vertices[0].fx;
        if (i <= (N-P)) {
            throw eil::ExceptionBase(eil::format("i=%d seems not to be in the high points", i),
                                     __FILE__, __LINE__);
        }
        auto x_high = vertices[i].x.dup();
        auto f_high = vertices[i].fx;
        // Centroid of simplex excluding point(s) that we are replacing.
        auto x_mid = centroid(N-P);
        //
        // First, try moving away from worst point by
        // reflection through centroid.
        auto x_refl = x_mid.dup();
        for (size_t j = 0; j < x_refl.length(); ++j) {
            x_refl[j] += Kreflect*(x_refl[j] - x_high[j]);
        }
        auto f_refl = f(x_refl.all()); ++nfe;
        if (f_refl < f_min) {
            // The reflection through the centroid is good,
            // try to extend in the same direction.
            auto x_ext = x_mid.dup();
            for (size_t j = 0; j < x_ext.length(); ++j) {
                x_ext[j] += Kextend*(x_refl[j] - x_mid[j]);
            }
            auto f_ext = f(x_ext.all()); ++nfe;
            if (f_ext < f_refl) {
                // Keep the extension because it's best.
                vertices[i].x = x_ext;   // D: `vertices[i].x = x_ext[]`
                vertices[i].fx = f_ext;
                return true;
            } else {
                // Settle for the original reflection.
                vertices[i].x = x_refl;  // D: `vertices[i].x = x_refl[]`
                vertices[i].fx = f_refl;
                return true;
            }
        } else {
            // The reflection is not going in the right direction, it seems.
            // See how many vertices are worse than the reflected point.
            size_t count = 0;
            for (size_t j = 0; j < N+1; ++j) {
                if (vertices[j].fx > f_refl) count += 1;
            }
            if (count <= 1) {
                // Not too many points are higher than the original reflection.
                // Try a contraction on the reflection-side of the centroid.
                auto x_con = x_mid.dup();
                for (size_t j = 0; j < x_con.length(); ++j) {
                    x_con[j] = (1.0-Kcontract)*x_con[j] + Kcontract*x_high[j];
                }
                auto f_con = f(x_con.all()); ++nfe;
                if (f_con < f_high) {
                    // At least we haven't gone uphill; accept.
                    vertices[i].x = x_con;   // D: `vertices[i].x = x_con[]`
                    vertices[i].fx = f_con;
                    return true;
                }
            } else {
                // Retain the original reflection because there are many
                // original vertices with higher values of the objective function.
                vertices[i].x = x_refl;   // D: `vertices[i].x = x_refl[]`
                vertices[i].fx = f_refl;
                return true;
            }
        }
        // If we arrive here, we have not replaced the highest point.
        return false;
    } // end replace_vertex()

    // In Python style, we have the following attributes of the simplex public
    // because we want to be able to access them from the minimize function.
    eil::Array<T> dx;
    size_t N = 0;
    size_t P = 0;
    double Kreflect = 0.0;
    double Kextend = 0.0;
    double Kcontract = 0.0;
    int nfe = 0;
    int nrestarts = 0;
    eil::Array<Vertex<T>> vertices;

  private:
    // D's `alias f` (nelmin.d:127) is compile-injected, so every body above spells
    // the objective function as plain `f(x)`. The port stores the instance the
    // constructor was given and calls it the same way; the only visible difference
    // is the `.all()` that turns an Array into the D view `f` expects.
    F f;
}; // end class NMMinimizer

//-----------------------------------------------------------------------

/**
 * Locate a minimum of the objective function, f.
 *
 * Input:
 *     f       : user-supplied multivariate function double f(double x[])
 *     x       : vector of N coordinates
 *     dx      : vector of N increments to apply to x when forming
 *               the initial simplex.  Their magnitudes determine the size
 *               and shape of the initial simplex.
 *     tol     : the terminating limit for the standard-deviation
 *               of the simplex function values.
 *     P       : number of points to replace in parallel, each step.
 *     maxfe   : maximum number of function evaluations that we will allow
 *     n_check : number of steps between convergence checks
 *     delta   : magnitude of the perturbations for checking a local minimum
 *               and for the scale reduction when restarting
 *     Kreflect, Kextend, Kcontract: coefficients for locating the new vertex
 *
 * Output:
 *     Returns a flag to indicate if convergence was achieved.
 *     On return x contains the coordinates for the best x location,
 *     corresponding to min(f(x)),
 *     f_min      : the function value at that point,
 *     n_fe      : the number of function evaluations and
 *     n_restart : the number of restarts (with scale reduction).
 */
template <typename F, typename T>
bool minimize(F f, eil::Array<T>& x, T& f_min, int& n_fe, int& n_restart,
              eil::Slice<T> dx, double tol = 1.0e-6, int P = 1, int max_fe = 400,
              int n_check = 20, double delta = 0.001, double Kreflect = 1.0,
              double Kextend = 2.0, double Kcontract = 0.5)
{
    // D's template constraint, nelmin.d:82-83 (the Complex!double arm of it
    // belongs to the version(complex_numbers) build).
    static_assert(std::is_same<decltype(f(std::declval<eil::Slice<T>>())), double>::value,
                  "nelmin: the objective function must map Slice<T> to double");

    // `out` parameters start at their .init, and f_min is only written inside the
    // loop below; a budget-limited caller must still read 0, as it does in D.
    f_min = T();
    n_fe = 0;
    n_restart = 0;

    bool converged = false;
    size_t N = x.length();
    // Might have to check for NaNs as well.
    eil::Array<T> dxa(dx);
    if (dxa.length() != x.length()) {
        dxa = x.dup();
        for (auto& elem : dxa) elem = T(0.1);
    }
    NMMinimizer<F, T> smplx(f, N, dxa.all(), P, Kreflect, Kextend, Kcontract);
    smplx.build_initial_simplex(x.all());
    while (!converged && (smplx.nfe < max_fe)) {
        smplx.take_steps(n_check);
        sort_detail::sort_vertices(smplx.vertices);
        // Pick out the current best vertex.
        x.all().assign_from(smplx.vertices[0].x.all());
        f_min = smplx.vertices[0].fx;
        // Check the scatter of vertex values to see if we are
        // close enough to call it quits.
        auto mean_std_dev = smplx.f_statistics();
        if (mean_std_dev[1] < tol) {
            // All of the points are close together but we need to test more carefully.
            converged = smplx.test_for_minimum(delta);
            if (!converged) smplx.rescale(delta);
        }
    } // end while !converged
    n_fe = smplx.nfe;
    n_restart = smplx.nrestarts;
    return converged;
} // end minimize()

} // namespace nelmin
} // namespace eil::nm
