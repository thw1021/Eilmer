// splinelsq.d

#include "nm/splinelsq.hpp"

#include <cstddef>

#include "nm/nelmin.hpp"

namespace eil::nm {

CubicSplineLsq::CubicSplineLsq(const eil::Slice<double>& xd, const eil::Slice<double>& yd,
                               const eil::Slice<double>& wd, const eil::Slice<double>& xs,
                               int nseg)
// Construct a spline of nseg segments to approximate the xd, yd points.
// wd is the array of weights for the data points,
// for use when computing the sum-square error.
// xs array may be used to set the x-locations of the spline knots.
// If it is supplied, it's length has to be nseg+1.
{
    // Check for reasonable arrays coming in.
    this->xd = eil::Array<double>(xd);   // xd.dup()
    size_t nd = xd.length();
    eil_assert(nd > static_cast<size_t>(nseg+1));   // splinelsq.d:35 "Too few data points."
    this->yd = eil::Array<double>(yd);   // yd.dup()
    eil_assert(this->yd.length() == nd); // "yd array not same length as xd."
    this->wd = eil::Array<double>(wd);   // wd.dup()
    if (this->wd.length() == 0) {
        this->wd.set_length(nd);
        for (size_t i = 0; i < nd; ++i) { this->wd[i] = 1.0; }
    } else {
        eil_assert(this->wd.length() == nd);   // "Inconsistent length for wd."
    }
    if (xs.length() > 0) {
        eil_assert(xs.length() == static_cast<size_t>(nseg+1));   // "Inconsistent length for xs."
    }
    this->xs = eil::Array<double>(xs);   // xs.dup()
    this->nseg = nseg;
    eil_assert(this->nseg > 1);          // "Too few segments."
    //
    // Set up an initial guess for the spline as a straight line.
    eil::Array<double> ys;
    if (this->xs.length() == 0) {
        for (int i = 0; i < nseg+1; ++i) {
            double frac = static_cast<double>(i)/nseg;
            // xd[0]*(1.0-frac) + xd[$-1]*frac, with $ -> xd.length()
            this->xs.append(xd[0]*(1.0-frac) + xd[xd.length()-1]*frac);
            ys.append(yd[0]*(1.0-frac) + yd[yd.length()-1]*frac);
        }
    } else {
        for (int i = 0; i < nseg+1; ++i) {
            double frac = (this->xs[i]-xd[0])/(xd[xd.length()-1]-xd[0]);
            ys.append(yd[0]*(1.0-frac) + yd[yd.length()-1]*frac);
        }
    }
    // Use the range of the y data to estimate a suitable perturbation.
    // splinelsq.d:67 uses std.algorithm.searching's maxElement/minElement, which
    // scan left to right and keep the first extreme on ties; written out here
    // because no substrate helper exists yet. The operand of the scan is D's
    // `yd`, which inside this constructor is the parameter (same values as the
    // member, since the member is its dup).
    double max_el = yd[0];
    double min_el = yd[0];
    for (size_t j = 1; j < yd.length(); ++j) {
        if (yd[j] > max_el) max_el = yd[j];
        if (yd[j] < min_el) min_el = yd[j];
    }
    double dy = max_el - min_el; dy *= 0.1;
    eil::Array<double> delys;
    for (int i = 0; i < nseg+1; ++i) { delys.append(dy); }
    // Optimize the spline by adjusting the ys values.
    double f;
    int nfe, nres;
    double tol = 1.0e-6;
    int P = 2;
    int nfe_max = 1200;
    bool conv_flag = nelmin::minimize<Residual, double>(Residual{this}, ys, f, nfe, nres,
                                                        delys.all(), tol, P, nfe_max);
    // writeln("ys= ", ys, " f= ", f);
    // writeln("convergence-flag= ", conv_flag);
    // writeln("number-of-fn-evaluations= ", nfe);
    // writeln("number-of-restarts= ", nres);
    // D leaves those four writes commented out, so the fit's counters are
    // deliberately discarded here too; the (void) casts only silence -Wall.
    (void)conv_flag; (void)f; (void)nfe; (void)nres;
    spl = CubicSpline(this->xs.all(), ys.all());
}

CubicSplineLsq::CubicSplineLsq(const CubicSplineLsq& other)
{
    spl = CubicSpline(other.spl);
    nseg = other.nseg;
    xs = other.xs.dup();
    xd = other.xd.dup();
    yd = other.yd.dup();
    wd = other.wd.dup();
}

double CubicSplineLsq::sse(const eil::Slice<double>& ps)
// splinelsq.d:70-80, D's nested residual function.
// The parameter vector is the vector of y-values for the spline knots.
{
    eil_assert(ps.length() == this->xs.length());   // "Unexpected length for parameter vector."
    spl = CubicSpline(this->xs.all(), ps);
    double sumerr = 0.0;
    for (size_t j = 0; j < xd.length(); ++j) {
        double e = yd[j] - spl(xd[j]);
        sumerr += wd[j]*e*e;
    }
    return sumerr;
}

} // namespace eil::nm
