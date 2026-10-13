#pragma once
// eil/range.hpp — std.range.iota, with Phobos' element VALUES.
//
// Two properties of D's floating iota are load-bearing and both were measured
// (probes/substrate/probe_iota.d, replayed by tests/test_substrate_iota.cpp):
//
//   1. Element n is `begin + n*step`, NOT the previous element plus step.
//      Walking by accumulation gives iota(0.0,1.0,0.1)[6] = 0.59999999999999998
//      while D gives 0.60000000000000009 (and [7], [8], [9] differ too).
//      geom/surface/beziertrianglepatch.d and geom/grid/sgrid.d sweep parameter
//      spaces with iota, so those last bits feed area and centroid sums.
//   2. The far bound is strict: iota(0.0, 1.0, 0.25) stops at 0.75, dropping the
//      exact hit at 1.0. A port that writes `v <= end` produces one extra point.
//
// Negative steps mirror rule 2 (`v > end`), which is why D's iota(5, 0, -1) yields
// 5,4,3,2,1 and excludes 0.
//
// The result is materialised rather than lazy. Sizes here are per-Bernstein-index
// loops and parameter sweeps, so the copy is not a cost the gate measures, and an
// Array keeps `.length`/`[]` working exactly like the D range did at the call sites.

#include "eil/array.hpp"

namespace eil {

template <typename T> Array<T> iota(T begin, T end, T step) {
    Array<T> out;
    for (long n = 0;; ++n) {
        const T v = static_cast<T>(begin + static_cast<T>(n) * step);
        const bool in_range = (step > T(0)) ? (v < end) : (v > end);
        if (!in_range) break;
        out.append(v);
    }
    return out;
}

template <typename T> Array<T> iota(T begin, T end) { return iota(begin, end, T(1)); }
template <typename T> Array<T> iota_count(T end) { return iota(T(0), end, T(1)); }

} // namespace eil
