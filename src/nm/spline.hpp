#pragma once
// nm/spline.d -> src/nm/spline.hpp (module nm.spline).
//
// Natural cubic spline interpolation, as used by geom/gpath/xspline.d and (through
// nm/splinelsq.d) geom/gpath/xsplinelsq.d. The observable surface stays as D left
// it: public `xs`/`ys`, `operator()` for evaluation, `xmin()`/`xmax()`.
//
// Differences from the D source, all structural rather than numeric:
//
//   * Reference semantics. spline.d:16 declares `class CubicSpline`, so a D
//     variable of that type is a GC handle. The only copy path the reference tree
//     takes is spline.d:63-66 `this(ref const(CubicSpline) other)`, which
//     *rebuilds* the spline from other.xs/other.ys instead of aliasing, so a value
//     class with an ordinary deep copy reproduces every D observable (that is the
//     path geom/gpath/xspline.d:55 `new CubicSpline(other.spl)` uses). No
//     shared_ptr handle is needed here, and D's `new CubicSpline(xs, ys)` ports to
//     a direct construction or an assignment of a temporary.
//   * A default-constructed CubicSpline models D's null handle as an object with
//     zero knots, so an element read before assignment raises eil::RangeError
//     instead of D's segfault.
//   * `xs.dup()` where `xs` is a parameter (a D view) becomes the Array-from-Slice
//     construction: eil::Slice has no dup() of its own (only eil::Array does), so
//     this is the one spelling available. Reported as a substrate gap.
//   * spline.d as committed does NOT compile in the gdtk tree at e587b235: its
//     constructor instantiates nm.bbla.gaussJordanElimination, which is declared
//     @nogc but throws `new Exception` (bbla.d:705-711), and ldc2 1.40 rejects
//     that (also visible to every Wave 2 geom module that imports nm.spline). The
//     measured baseline for this module was therefore taken against a copy of the
//     D tree with that single attribute deleted, no statement changed; the exact
//     command is in probes/nm/probe_spline.d's header.

#include <cstddef>

#include "eil/array.hpp"
#include "eil/error.hpp"
#include "eil/number.hpp"

namespace eil::nm {

class CubicSpline {
    // Interpolatory cubic spline, storing coefficients for the polynomial pieces.

  public:
    eil::Array<double> xs, ys; // Interpolation points of the spline.

    // spline.d:16 `class CubicSpline` gives `CubicSpline s;` a null handle.
    CubicSpline() = default;

    // spline.d:22-61 Sets up the interpolatory cubic spline through the xi, yi
    // points: there will be n+1 knots, n segments and n-1 interior knots.
    CubicSpline(const eil::Slice<double>& xs, const eil::Slice<double>& ys);

    // spline.d:63-66 `this(ref const(CubicSpline) other)`
    CubicSpline(const CubicSpline& other);

    // Spelled out because the rebuild constructor above makes the implicit
    // assignment deprecated in C++11 terms; it is the member-wise deep copy.
    CubicSpline& operator=(const CubicSpline&) = default;

    // spline.d:68-82 Evaluates the spline at point x by first searching for the
    // relevant segment and then evaluating the local polynomial piece.
    double operator()(double x) const;

    double xmin() const { return xs[0]; }

    // spline.d:86 `return xs[$-1];`
    double xmax() const { return xs[xs.length()-1]; }

  private:
    size_t n = 0;
    eil::Array<double> a, b, c;
}; // end class CubicSpline

} // namespace eil::nm
