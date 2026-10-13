#pragma once
// nm/splinelsq.d -> src/nm/splinelsq.hpp (module nm.splinelsq).
//
// Least-squares-fitted cubic spline: the knot x-positions are fixed (supplied or
// spread evenly over the data), and nm/nelmin's Nelder-Mead minimiser adjusts the
// knot y-values to minimise the weighted sum of squared errors against the spline
// rebuilt from those values. Consumed by geom/gpath/xsplinelsq.d.
//
// Differences from the D source, all structural rather than numeric:
//
//   * splinelsq.d:70-80 declares the residual function `sse` as a D *nested*
//     function, which closes over the enclosing `this` (it writes the member
//     `spl` on every evaluation). C++ has no nested functions, so it becomes the
//     private member `sse` plus the one-line `Residual` adaptor that nelmin's
//     template parameter needs; the arithmetic and the order of operations inside
//     the body are unchanged.
//   * `maxElement(yd) - minElement(yd)` (splinelsq.d:67) is spelled out as the
//     same left-to-right reduction std.algorithm's minElement/maxElement perform
//     (first minimum/maximum wins on ties); with number == double there is no
//     numeric difference, but the scan order is kept identical.
//   * Class handles: `CubicSpline spl` is a D GC handle. geom/gpath/xsplinelsq.d
//     only ever assigns a freshly built object to it (splinelsq.d:87, :92) and the
//     copy constructor at splinelsq.d:95-103 rebuilds everything, so a value
//     member with a deep copy reproduces every D observable. Same reasoning as in
//     nm/spline.hpp.
//   * The parameter names keep D's shadowing of the members (xd, yd, wd, xs):
//     inside the constructor body D reads the *parameters* for xd/yd/wd and the
//     members (this.xs) once xs has been grown, and the values are the same
//     either way because the members are dups of the parameters.

#include <cstddef>

#include "eil/array.hpp"
#include "eil/error.hpp"
#include "eil/number.hpp"
#include "nm/spline.hpp"

namespace eil::nm {

class CubicSplineLsq {
    // A CubicSpline that has been fitted to the original data in a least-squares
    // error sense.

  public:
    eil::Array<double> xd, yd; // Copy of the original data.
    eil::Array<double> wd;     // Weights for the data points.
    eil::Array<double> xs;     // x-coordinates of the spline knots
    int nseg = 0;              // Number of segments in the underlying spline.

    // splinelsq.d:25-93 Construct a spline of nseg segments to approximate the
    // xd, yd points. wd is the array of weights for the data points, for use when
    // computing the sum-square error. The xs array may be used to set the
    // x-locations of the spline knots; if it is supplied, its length has to be
    // nseg+1.
    CubicSplineLsq(const eil::Slice<double>& xd, const eil::Slice<double>& yd,
                   const eil::Slice<double>& wd, const eil::Slice<double>& xs, int nseg);

    // splinelsq.d:95-103
    CubicSplineLsq(const CubicSplineLsq& other);

    // Member-wise deep copy, stated so the rebuild constructor above does not
    // deprecate the implicit assignment.
    CubicSplineLsq& operator=(const CubicSplineLsq&) = default;

    // splinelsq.d:105-109 Evaluates the underlying spline at point x.
    double operator()(double x) const { return spl(x); }

    double xmin() const { return spl.xmin(); }

    double xmax() const { return spl.xmax(); }

  private:
    CubicSpline spl;   // The underlying spline model.

    // splinelsq.d:70-80, the nested residual function. The parameter vector is
    // the vector of y-values for the spline knots.
    double sse(const eil::Slice<double>& ps);

    // The alias `sse` that nelmin's template parameter stands for.
    struct Residual {
        CubicSplineLsq* self;
        double operator()(const eil::Slice<double>& ps) const { return self->sse(ps); }
    };
}; // end class CubicSplineLsq

} // namespace eil::nm
