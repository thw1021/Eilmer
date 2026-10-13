// spline.d

#include "nm/spline.hpp"

#include <cstddef>

#include "nm/bbla.hpp"

namespace eil::nm {

CubicSpline::CubicSpline(const eil::Slice<double>& xs, const eil::Slice<double>& ys)
// Sets up the interpolatory cubic spline through the xi, yi points.
//   xs : sequence of x-coordinates
//   ys : sequence of y-coordinates
//
// There will be n+1 knots, n segments and n-1 interior knots.
{
    n = xs.length() - 1;  // number of segments
    eil_assert(ys.length() == n+1);   // spline.d:30 "Unequal xs, ys lengths."
    this->xs = eil::Array<double>(xs);   // spline.d:31 `xs.dup()`
    this->ys = eil::Array<double>(ys);   // spline.d:32 `ys.dup()`
    a.set_length(n);
    b.set_length(n);
    c.set_length(n);
    // for d, just use the y array
    eil::Array<double> h; h.set_length(n);
    for (size_t i = 0; i < n; ++i) { h[i] = xs[i+1] - xs[i]; }
    // Solve for the second-derivative at the interior knots.
    // The sigma[i], i=1...n-1 (inclusive) are the unknowns since
    // the natural end conditions are sigma[0]=sigma[n]=0.0
    size_t m = n-1; // number of unknowns (and constraint equations)
    auto A = bbla::zeros<double>(m, m+1); // Augmented matrix
    for (size_t k = 0; k < m; ++k) {
        size_t i = k + 1; // i-index as in the lecture notes
        if (k > 0) { A(k,k-1) = h[i-1]; }
        A(k,k) = 2.0*(h[i-1]+h[i]);
        if (k < m-1) { A(k,k+1) = h[i]; }
        A(k,m) = 6.0*((ys[i+1]-ys[i])/h[i] - (ys[i]-ys[i-1])/h[i-1]); // RHS elements
    }
    bbla::gaussJordanElimination<double>(A);
    // Put sigma values in place in the full array.
    eil::Array<double> sigma;
    for (size_t i = 0; i < n+1; ++i) { sigma.append(0.0); }
    for (size_t i = 0; i < m; ++i) { sigma[i+1] = A(i,m); }
    // Evaluate and store coefficients for the polynomial segments.
    for (size_t i = 0; i < n; ++i) {
        a[i] = (sigma[i+1]-sigma[i])/(6.0*h[i]);
        b[i] = sigma[i]/2.0;
        c[i] = (ys[i+1]-ys[i])/h[i] - (2.0*sigma[i] + sigma[i+1])*h[i]/6.0;
    }
} // end constructor

CubicSpline::CubicSpline(const CubicSpline& other)
    // spline.d:65 `this(other.xs, other.ys);` -- the delegation rebuilds the
    // spline from the stored knots, which is what the delegating ctor does here.
    : CubicSpline(other.xs.all(), other.ys.all())
{
}

double CubicSpline::operator()(double x) const
// Evaluates the spline at point x by first searching for the
// relevant segment and then evaluating the local polynomial piece.
{
    int i = 0;
    if (x < xs[0]) {
        i = 0; // The point is off range to the left.
    } else {
        i = static_cast<int>(n) - 1; // Start search from the right-hand end.
        while ((x < xs[i]) && (i > 0)) { i -= 1; }
    }
    // Now that we found the relevant segment, evaluate locally.
    double dx = x - xs[i];
    return ((a[i]*dx + b[i])*dx + c[i])*dx + ys[i];
}

} // namespace eil::nm
