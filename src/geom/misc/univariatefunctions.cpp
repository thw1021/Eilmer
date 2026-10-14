// geom/misc/univariatefunctions.d -> src/geom/misc/univariatefunctions.cpp
//
// Definitions in D source order (D:19 through D:766). Every operand order, loop
// bound, guard, message string and default argument is copied from the D text; where
// D's own text looks wrong it is still copied, with the line number noted (see the
// "preserved D oddities" list at the bottom of this header comment and the probe rows
// in baselines/e587b235/probes/geom_misc_univariatefunctions.txt).
//
// Constants: the two Gaussian-ish classes use their OWN `pi = 3.1415926535` (D:345,
// :445) — the 11-digit literal that belongs to the Abramowitz & Stegun erf fit — so
// EIL_PI must not be substituted there. Everywhere else a power is std.math.pow, which
// is eil::pow (and D's pow with an int exponent is the libm path too, measured 80/80
// rows equal to pow(x,(double)n)).
//
// Preserved D oddities (all measured, none "fixed"):
//   * D:103  QuadraticFunction.toString labels its `reverse` flag "t1".
//   * D:132  RobertsFunction writes `reverse = 0;` for the (!end0 && end1) case, i.e.
//            false, while the (end0 && !end1) case above it writes true. The asymmetry
//            is what produces the probe's roberts.e0e1 rows, so it is kept (written as
//            false, which 0 converts to identically).
//   * D:85-89, :324-331, :409-419  copy constructors that omit fields -> dup() yields
//            NaN-returning objects (header note).
//   * D:584  VinokurFunction's B<1 message says "VinokurGeomHybridFunction".
//   * D:681  VinokurGeomHybridFunction prints `vn` where the label says "number
//            geometric layers at end" (n1 is the value the label describes).
//   * D:583, :687  the B guard is `B<1.0`, not `<=`, and B==1.0 exactly (equal end
//            sizes with n*end-size == 1) leaves no root for brent to bracket, so the
//            Vinokur constructor throws an Exception instead (measured: probe row
//            "err vinokur.Beq1").
//   * D:312-318  GaussianFunction's centre-fitting Newton loop runs to its full 100
//            iterations without reaching |err| < 1e-15 for every m except 0.5
//            (measured: "iter GaussianFunction m=0.1 gauss=100"), so the cluster centre
//            is only approximately at m. The loop is not a while-with-break, it is a
//            foreach with a break, so the value it stops on is what the port uses.
//   * D:253  lambertW throws after 99 iterations at tol=1e-15; the probe shows it fires
//            for ordinary-looking parameters (a=0.005, r=1.1, N=21).
//   * D:59   kdtree's unused nPos — different module, noted there.
//
// Transcendental fidelity (measured 2026-10-14, not a code defect but it moves bits):
// the std::tanh/std::sinh/std::exp calls below are the port's standing mapping of D's
// std.math functions onto libm (wave-02-brief section 3, eil/math.hpp). Phobos does NOT
// call libm for those four: from inside D, std.math.exp disagreed with
// core.stdc.math.exp on 569 of 4001 arguments on [0,2] (1 ULP each), sinh on 1933, cosh
// on 699, tanh on 1286, and double(glibc expl(x)) reproduces none of them. So this
// module's Vinokur rows (tanh in opCall, sinh inside brent) and its two erf/exp rows
// land up to 21 ULP (relative 3.15e-15) from the captured corpus, and D's non-symmetric
// tanh gives f(0) = -1.1102230246251575e-15 where libm gives exactly 0. Counted and
// gated in tests/test_geom_misc_univariatefunctions.cpp; closing it means porting
// Phobos' exp/tanh/sinh into the substrate, which changes every module that
// transcends, so it is not decided here.
#include "geom/misc/univariatefunctions.hpp"

#include <cmath>
#include <cstddef>
#include <memory>
#include <string>

#include "eil/array.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/math.hpp"
#include "eil/number.hpp"
#include "nm/brent.hpp"

namespace eil {
namespace geom {

// ---------------------------------------------------------------------------
// D:17-39 base class
// ---------------------------------------------------------------------------
double UnivariateFunction::operator()(double t) const {
    return t; // expect this to be overridden
}

std::string UnivariateFunction::toString() const { return "UnivariateFunction()"; }

// Returns an array of parameter values, distributed from t0 though t1.
// The subclass determines the form of distribution.
eil::Array<double> UnivariateFunction::distribute_parameter_values(size_t n, double t0,
                                                                  double t1) const {
    eil::Array<double> tv;
    double dt = 1.0 / (n - 1); // size_t subtraction, exactly as D: n==1 divides by zero
    for (size_t i = 0; i < n; ++i) {
        double t = (*this)(dt * i); // first mapping is done by the specific function
        tv.append((1.0 - t) * t0 + t * t1); // map to specified range and save
    }
    return tv;
} // end distribute_parameter_values()

// ---------------------------------------------------------------------------
// D:41-67 LinearFunction
// ---------------------------------------------------------------------------
LinearFunction::LinearFunction(double t0, double t1) {
    this->t0 = t0;
    this->t1 = t1;
}

LinearFunction::LinearFunction(const LinearFunction& other) {
    t0 = other.t0;
    t1 = other.t1;
}

std::shared_ptr<LinearFunction> LinearFunction::dup() const {
    return std::make_shared<LinearFunction>(*this); // D: new LinearFunction(this)
}

double LinearFunction::operator()(double t) const { return (1.0 - t) * t0 + t * t1; }

std::string LinearFunction::toString() const {
    return "LinearFunction(t0=" + eil::to_string(t0) + ", t1=" + eil::to_string(t1) + ")";
}

// ---------------------------------------------------------------------------
// D:69-108 QuadraticFunction
// ---------------------------------------------------------------------------
QuadraticFunction::QuadraticFunction(double ratio, bool reverse) {
    this->ratio = ratio;
    this->reverse = reverse;
    this->a = (ratio - 1.0) / (ratio + 1.0);
    this->b = 2 / (ratio + 1.0);
}

QuadraticFunction::QuadraticFunction(const QuadraticFunction& other) {
    // D:85-89 — ratio and reverse only. a and b keep their double.init NaN, which is
    // why dup()(t) is NaN (measured: "dup QuadraticFunction ... dup=nan dupIsNaN=true").
    ratio = other.ratio;
    reverse = other.reverse;
}

std::shared_ptr<QuadraticFunction> QuadraticFunction::dup() const {
    return std::make_shared<QuadraticFunction>(*this);
}

double QuadraticFunction::operator()(double t) const {
    if (reverse) t = 1.0 - t;
    double y = a * t * t + b * t;
    if (reverse) y = 1.0 - y;
    return y;
}

std::string QuadraticFunction::toString() const {
    // D:103 prints `reverse` under the label "t1"; kept as written.
    return "QuadraticFunction(ratio=" + eil::to_string(ratio) + ", t1=" +
           eil::to_string(reverse) + ")";
}

// ---------------------------------------------------------------------------
// D:110-166 RobertsFunction
// ---------------------------------------------------------------------------
RobertsFunction::RobertsFunction(bool end0, bool end1, double beta) {
    this->end0 = end0;
    this->end1 = end1;
    this->beta = beta;
    if (!end0 && !end1) cluster = false;
    if (beta <= 1.0) cluster = false;
    if (end0 && end1) alpha = 0.5;
    if (end0 && !end1) {
        reverse = true;
        alpha = 0.0;
    }
    if (!end0 && end1) {
        reverse = false; // D:132 writes `reverse = 0;`
        alpha = 0.0;
    }
}

RobertsFunction::RobertsFunction(const RobertsFunction& other) {
    end0 = other.end0;
    end1 = other.end1;
    beta = other.beta;
    alpha = other.alpha;
    reverse = other.reverse;
    cluster = other.cluster;
}

std::shared_ptr<RobertsFunction> RobertsFunction::dup() const {
    return std::make_shared<RobertsFunction>(*this);
}

double RobertsFunction::operator()(double t) const {
    double tbar = eil::d_init<number>(); // D declares `double tbar;` -> NaN until assigned
    if (reverse) t = 1.0 - t;
    if (cluster) {
        tbar = roberts_original(t, alpha, beta);
    } else {
        tbar = t;
    }
    if (reverse) tbar = 1.0 - tbar;
    return tbar;
}

std::string RobertsFunction::toString() const {
    return "RobertsFunction(end0=" + eil::to_string(end0) + ", end1=" + eil::to_string(end1) +
           ", beta=" + eil::to_string(beta) + ")";
}

// ---------------------------------------------------------------------------
// D:168-181 roberts_original
//   eta   : unstretched coordinate, 0 <= eta <= 1.0
//   alpha : location of stretching
//           alpha = 0.5 : clustering of nodes at both extremes of eta
//           alpha = 0.0 : nodes will be clustered near eta=1.0
//   beta  : stretching factor (more stretching as beta --> 1.0)
//   Returns stretched coordinate, 0 <= roberts <= 1.0
// ---------------------------------------------------------------------------
double roberts_original(double eta, double alpha, double beta) {
    double lambda = (beta + 1.0) / (beta - 1.0);
    lambda = eil::pow(lambda, ((eta - alpha) / (1.0 - alpha)));
    double etabar = (beta + 2.0 * alpha) * lambda - beta + 2.0 * alpha;
    etabar = etabar / ((2.0 * alpha + 1.0) * (1.0 + lambda));
    return etabar;
} // end roberts_original()

// ---------------------------------------------------------------------------
// D:183-284 GeometricFunction
// ---------------------------------------------------------------------------
GeometricFunction::GeometricFunction(double a, double r, int N, bool reverse) {
    this->a = a;
    this->r = r;
    this->N = N;
    this->reverse = reverse;
    if (a <= 0.0)
        throw eil::ErrorBase("Problematic parameters in GeometricFunction a= " + eil::to_string(a));
    if (r <= 1.0)
        throw eil::ErrorBase("Problematic parameters in GeometricFunction r= " + eil::to_string(r));
    double ns = solve_for_nswitch(a, r, N); // the local ns shadows the member, as in D:201
    if ((ns < 1.0) || (ns > N))
        throw eil::ErrorBase("Problematic parameters in GeometricFunction ns= " + eil::to_string(ns));
    this->ts = ns / (N - 1);
    this->ns = ns;
}

GeometricFunction::GeometricFunction(const GeometricFunction& other) {
    a = other.a;
    r = other.r;
    N = other.N;
    reverse = other.reverse;
    ts = other.ts;
    ns = other.ns;
}

std::shared_ptr<GeometricFunction> GeometricFunction::dup() const {
    return std::make_shared<GeometricFunction>(*this);
}

double GeometricFunction::operator()(double t) const {
    if (reverse) t = 1.0 - t;
    double n = t * (N - 1);
    double t1 = eil::d_init<number>();
    if (t < ts) {
        t1 = a * (1 - eil::pow(r, n)) / (1 - r);
    } else {
        t1 = a * (1 - eil::pow(r, ns)) / (1 - r) + (n - ns) * a * eil::pow(r, (ns - 1));
    }
    if (reverse) t1 = 1.0 - t1;
    return t1;
}

// Lambert's W function: solve for w given z = w*exp(w)
// See: en.wikipedia.org/wiki/Lambert_W_function#Numerical_evaluation
// Notes from D: "Adjustments were performed to the tol parameter on March 2024. This
// required a concerningly tight tolerance to function properly."
double GeometricFunction::lambertW(double z, double wguess, double tol) {
    double w = eil::d_init<number>(), expw = eil::d_init<number>(),
           zt = eil::d_init<number>(), wexpw = eil::d_init<number>();
    w = wguess;
    for (int n = 0; n < 100; ++n) {
        // Not in D: the trip count, so the probe can compare convergence behaviour.
        lambertW_iterations = n + 1;
        if (n >= 99)
            throw eil::ErrorBase("lambertW function eval timed out");
        expw = std::exp(w);
        zt = w * expw;
        if (std::fabs(zt - z) < tol) break;
        wexpw = w * expw;
        w -= (wexpw - z) / (expw + wexpw);
    }
    return w;
}

// Solve for the switching point where the geometric progression switches over to a
// linear function. (See derivation 20/07/2020.) Effectively solving for ns given:
//   1.0 = a*(1-r**ns)/(1-r) + (N-1-ns)*a*r**(ns-1)
double GeometricFunction::solve_for_nswitch(double a, double r, double N) {
    double A = 1 / a - 1 / (1 - r);
    double B = -1.0 / (1 - r);
    double M = N - 1;

    double powarg = -M - B * r + 1;
    double arg = -A * eil::pow(r, powarg) * eil::log(r);
    double wguess = (N / 4 - M - B * r) * eil::log(r);
    double w = lambertW(arg, wguess);
    double ns = w / eil::log(r) + B * r + M;
    return ns;
}

// ---------------------------------------------------------------------------
// D:286-368 GaussianFunction
// ---------------------------------------------------------------------------
GaussianFunction::GaussianFunction(double m, double s, double ratio) {
    this->m = m;
    this->s = s;
    this->ratio = ratio;
    if ((m < 0.0) || (m > 1.0))
        throw eil::ErrorBase("Problematic parameters in GaussianFunction m= " + eil::to_string(m));
    if ((ratio <= 0.0) || (ratio > 1.0))
        throw eil::ErrorBase("Problematic parameters in GaussianFunction ratio= " +
                                 eil::to_string(ratio));
    // See derivation 20/12/11 (NNG)

    // Iterate to ensure cluster center is at m.
    double b = m;
    double a = eil::d_init<number>(), c = eil::d_init<number>(), err = eil::d_init<number>();
    for (int n = 0; n < 100; ++n) {
        // Not in D: the trip count (see the header note).
        centre_iterations = n + 1;
        a = 1.0 / (1.0 + eil::sqrt(pi_ / 2.0) * (1.0 - ratio) * s *
                           (erf((b - 1.0) / eil::sqrt(2.0) / s) - erf(b / eil::sqrt(2.0) / s)));
        c = -eil::sqrt(pi_ / 2.0) * a * (1 - ratio) * s * erf(b / eil::sqrt(2.0) / s);
        err = (a * b + c) - m; // Center of the cluster is at opCall(b) = a*b + c
        if (std::fabs(err) < 1e-15) break;
        b -= err * (a * ratio); // Newton update, d/dx(opCall)(b) = a*ratio
    }
    this->a = a;
    this->b = b;
    this->c = c;
}

GaussianFunction::GaussianFunction(const GaussianFunction& other) {
    // D:324-331 — m, s, ratio, a, c. The fitted offset b is NOT copied, so dup()
    // computes erf(NaN/...) and returns NaN (measured: "dup GaussianFunction ... dup=nan").
    m = other.m;
    s = other.s;
    ratio = other.ratio;
    a = other.a;
    c = other.c;
}

std::shared_ptr<GaussianFunction> GaussianFunction::dup() const {
    return std::make_shared<GaussianFunction>(*this);
}

double GaussianFunction::operator()(double x) const {
    return a * x + eil::sqrt(pi_ / 2.0) * a * (1.0 - ratio) * s *
                   erf((b - x) / eil::sqrt(2.0) / s) +
           c;
}

// Approximate the gaussian error function using a curve fitted polynomial
// ("Handbook of Mathematical Functions", NIST; A&S 7.1.26).
// Note: the approximation is only valid for x>=0, and for x<0 the oddness of erf is
// used: f(x) = -f(-x).
double GaussianFunction::erf(double x) const {
    double xabs = std::fabs(x);
    double t = 1.0 / (1.0 + p_ * xabs);
    double f = 1.0 - (a1_ * t + a2_ * t * t + a3_ * t * t * t + a4_ * t * t * t * t +
                      a5_ * t * t * t * t * t) *
                         std::exp(-(xabs * xabs));
    f = std::copysign(f, x);
    return f;
}

// ---------------------------------------------------------------------------
// D:370-554 GaussGeomHybridFunction
// ---------------------------------------------------------------------------
GaussGeomHybridFunction::GaussGeomHybridFunction(double A, double R, int N, double m, double s,
                                                 double ratio, bool reverse) {
    this->A = A;
    this->R = R;
    this->N = N;
    this->m = m;
    this->s = s;
    this->ratio = ratio;
    this->reverse = reverse;
    if (A <= 0.0)
        throw eil::ErrorBase("Problematic parameters in GaussGeomHybridFunction A= " +
                                 eil::to_string(A));
    if (R <= 1.0)
        throw eil::ErrorBase("Problematic parameters in GaussGeomHybridFunction R= " +
                                 eil::to_string(R));
    if ((m <= 0.0) || (m >= 1.0))
        throw eil::ErrorBase("Problematic parameters in GaussGeomHybridFunction m= " +
                                 eil::to_string(m));
    if ((ratio <= 0.0) || (ratio >= 1.0))
        throw eil::ErrorBase("Problematic parameters in GaussGeomHybridFunction ratio= " +
                                 eil::to_string(ratio));

    double xs = solve_for_xswitch(); // local xs shadows the member, as in D:401
    if ((xs < 0.0) || (xs > 1.0))
        throw eil::ErrorBase("Problematic parameters in GaussGeomHybridFunction xs= " +
                                 eil::to_string(xs));
    this->xs = xs;

    this->a = dGeometricdx(xs) / dGaussiandx(1.0, xs);
    this->c = 1.0 - Gaussian(this->a, 0.0, 1.0);
}

GaussGeomHybridFunction::GaussGeomHybridFunction(const GaussGeomHybridFunction& other) {
    // D:409-419 — every field except a and c, so the gaussian half of a dup() is NaN
    // (measured: "dup GaussGeomHybrid ... dup0.5=nan dupIsNaN=true").
    A = other.A;
    R = other.R;
    N = other.N;
    m = other.m;
    s = other.s;
    ratio = other.ratio;
    reverse = other.reverse;
    xs = other.xs;
}

std::shared_ptr<GaussGeomHybridFunction> GaussGeomHybridFunction::dup() const {
    return std::make_shared<GaussGeomHybridFunction>(*this);
}

double GaussGeomHybridFunction::operator()(double x) const {
    if (reverse) x = 1.0 - x;
    double y = eil::d_init<number>();
    if (x < xs) {
        y = Geometric(x);
    } else {
        y = Gaussian(a, c, x);
    }
    if (reverse) y = 1.0 - y;
    return y;
}

double GaussGeomHybridFunction::erf(double x) const {
    double xabs = std::fabs(x);
    double t = 1.0 / (1.0 + p_ * xabs);
    double f = 1.0 - (a1_ * t + a2_ * t * t + a3_ * t * t * t + a4_ * t * t * t * t +
                      a5_ * t * t * t * t * t) *
                         std::exp(-(xabs * xabs));
    f = std::copysign(f, x);
    return f;
}

// Note that during setup, a and c are not yet known, so these helpers take a_ and c_
// to avoid the name clashes D's comment describes (D:469-475).
double GaussGeomHybridFunction::Gaussian(double a_, double c_, double x) const {
    return a_ * x + eil::sqrt(pi_ / 2.0) * a_ * (1.0 - ratio) * s *
                       erf((m - x) / eil::sqrt(2.0) / s) +
           c_;
}

double GaussGeomHybridFunction::dGaussiandx(double a_, double x) const {
    return a_ - a_ * (1.0 - ratio) * std::exp(-(m - x) * (m - x) / 2.0 / s / s);
}

double GaussGeomHybridFunction::d2Gaussiandx2(double a_, double x) const {
    return -a_ * (1.0 - ratio) * std::exp(-(m - x) * (m - x) / 2.0 / s / s) * (m - x) / s / s;
}

double GaussGeomHybridFunction::Geometric(double x) const {
    double n = x * (N - 1.0);
    return A * (1.0 - eil::pow(R, n)) / (1.0 - R);
}

double GaussGeomHybridFunction::dGeometricdx(double x) const {
    double n = x * (N - 1.0);
    return A * (N - 1.0) * eil::log(R) * eil::pow(R, n) / (R - 1.0);
}

double GaussGeomHybridFunction::d2Geometricdx2(double x) const {
    double dEdx = dGeometricdx(x);
    return (N - 1.0) * eil::log(R) * dEdx;
}

// Returns zero when 1.) 1.0 = Gaussian(1.0), 2.) Gaussian(xswitch) = Geometric(xswitch)
// and 3.) dGaussiandx(xswitch) = dGeometricdx(xswitch); solved numerically to get a
// smooth crossover point between the two functions.
double GaussGeomHybridFunction::SwitchEquationError(double xs) const {
    double E = Geometric(xs);
    double dEdx = dGeometricdx(xs);
    double G = Gaussian(1.0, 0.0, xs);
    double G1 = Gaussian(1.0, 0.0, 1.0);
    double dGdx = dGaussiandx(1.0, xs);

    double error = dEdx * (G - G1) / dGdx + 1.0 - E;
    return error;
}

double GaussGeomHybridFunction::SwitchEquationDerivative(double xs) const {
    double E = Geometric(xs);
    double dEdx = dGeometricdx(xs);
    double d2Edx2 = d2Geometricdx2(xs);
    double G = Gaussian(1.0, 0.0, xs);
    double G1 = Gaussian(1.0, 0.0, 1.0);
    double dGdx = dGaussiandx(1.0, xs);
    double d2Gdx2 = d2Gaussiandx2(1.0, xs);

    // D:522 computes E = Geometric(xs) here and never reads it (a dead call, kept
    // because the port must not silently drop D's evaluation order).
    (void)E;
    double de = (G - G1) * (dGdx * d2Edx2 - d2Gdx2 * dEdx) / dGdx / dGdx;
    return de;
}

// One variable Newton's method to find the switching point between the two functions.
double GaussGeomHybridFunction::solve_for_xswitch(double guess, double tol) {
    double xs = guess;
    double error = 1e32;
    double f = eil::d_init<number>(), dfdx = eil::d_init<number>(),
           dxs = eil::d_init<number>();
    int iterations = 0;

    while (error > tol) {
        f = SwitchEquationError(xs);
        error = eil::sqrt(f * f);
        dfdx = SwitchEquationDerivative(xs);
        dxs = -f / dfdx;
        xs += dxs;
        iterations += 1;
        xswitch_iterations = iterations; // not in D; see the header note
        if (iterations > 99)
            throw eil::ExceptionBase("Newton solver for xswitch timed out!", __FILE__, __LINE__);
    }
    return xs;
}

// ---------------------------------------------------------------------------
// D:557-641 VinokurFunction
// ---------------------------------------------------------------------------
VinokurFunction::VinokurFunction(int n, double s0, double s1) {
    this->n = n;
    this->s0 = s0;
    this->s1 = s1;
    this->A = eil::sqrt(s1 / s0);
    double B = 1 / (n * eil::sqrt(s1 * s0));
    this->B = B;
    if (B < 1.0) {
        // D:584 names the OTHER class in this message; kept as written.
        std::string msg = "Problematic parameters in VinokurGeomHybridFunction B<1!, B= " +
                          eil::to_string(B);
        msg += "\nnormalised end0 cell size: " + eil::to_string(s0);
        msg += "\nnormalised end1 cell size: " + eil::to_string(s1);
        msg += "\nnumber of cells: " + eil::to_string(n);
        throw eil::ErrorBase(msg, __FILE__, __LINE__);
    }
    const double beta_min = 1.0e-3;
    const double beta_max = 200.0;
    const double tol = 1.0e-11;
    this->beta_min = beta_min;
    this->beta_max = beta_max;
    this->tol = tol;
    // D:596-601 is a nested function closing over the local B (which is this->B), and
    // its parameter `beta` shadows the member beta. A lambda is the same capture.
    auto zeroFun = [B](double beta) { return std::sinh(beta) / beta - B; };
    try {
        // Callable entry point of nm.brent.solve (wave-02-brief section 4b.5).
        this->beta = eil::nm::brent::solve(zeroFun, beta_min, beta_max, tol);
    } catch (const ExceptionBase& e) {
        std::string msg =
            "There was a problem iterating to find the vinokur function stretch factor\n";
        // D:607-612 `debug { ... }` — kept unconditionally (see the header note about
        // the reference build flavour and --d-debug).
        msg += eil::format("The minimum value of the bracket is: %12.6f\n", 1.0e-3);
        msg += eil::format("The maximum value of the bracket is: %12.6f\n", 200.0);
        msg += "The message from the brent.solve function is:\n";
        msg += e.msg();
        throw eil::ExceptionBase(msg, __FILE__, __LINE__);
    }
}

VinokurFunction::VinokurFunction(const VinokurFunction& other) {
    n = other.n;
    s0 = other.s0;
    s1 = other.s1;
    A = other.A;
    B = other.B;
    beta = other.beta;
}

std::shared_ptr<VinokurFunction> VinokurFunction::dup() const {
    return std::make_shared<VinokurFunction>(*this);
}

double VinokurFunction::operator()(double x) const {
    double u = 0.5 * (1 + std::tanh(beta * (x - 0.5)) / std::tanh(0.5 * beta));
    return u / (A + (1 - A) * u);
}

// ---------------------------------------------------------------------------
// D:643-766 VinokurGeomHybridFunction
// ---------------------------------------------------------------------------
VinokurGeomHybridFunction::VinokurGeomHybridFunction(int n, double s0, int n0, double r0, double s1,
                                                     int n1, double r1) {
    this->n = n;
    this->s0 = s0;
    this->n0 = n0;
    this->r0 = r0;
    this->l0 = geometricSeriesLength(s0, r0, n0);
    this->s1 = s1;
    this->n1 = n1;
    this->r1 = r1;
    this->l1 = geometricSeriesLength(s1, r1, n1);
    this->vs0 = s0 * eil::pow(r0, n0) / (1 - l0 - l1);
    this->vs1 = s1 * eil::pow(r1, n1) / (1 - l0 - l1);
    this->vn = n - n0 - n1;
    if (vn < 1) {
        std::string msg = "Problematic parameters in VinokurGeomHybridFunction vn= " +
                          eil::to_string(vn);
        msg += "\nnumber of cells: " + eil::to_string(n);
        msg += "\nnumber geometric layers at start: " + eil::to_string(n0);
        // D:681 prints vn although the label says "at end" (n1 is that value). Kept.
        msg += "\nnumber geometric layers at end: " + eil::to_string(vn);
        throw eil::ErrorBase(msg, __FILE__, __LINE__);
    }
    this->A = eil::sqrt(vs1 / vs0);
    double B = 1 / (vn * eil::sqrt(vs1 * vs0));
    this->B = B;
    if (B < 1.0) {
        std::string msg = "Problematic parameters in VinokurGeomHybridFunction B<1!, B= " +
                          eil::to_string(B);
        msg += "\nnormalised first cell size: " + eil::to_string(vs0);
        msg += "\nnormalised last cell size: " + eil::to_string(vs1);
        msg += "\nnumber of cells: " + eil::to_string(vn);
        throw eil::ErrorBase(msg, __FILE__, __LINE__);
    }
    const double beta_min = 1.0e-3;
    const double beta_max = 200.0;
    const double tol = 1.0e-11;
    this->beta_min = beta_min;
    this->beta_max = beta_max;
    this->tol = tol;
    auto zeroFun = [B](double beta) { return std::sinh(beta) / beta - B; };
    try {
        this->beta = eil::nm::brent::solve(zeroFun, beta_min, beta_max, tol);
    } catch (const ExceptionBase& e) {
        std::string msg =
            "There was a problem iterating to find the vinokur function stretch factor\n";
        msg += eil::format("The minimum value of the bracket is: %12.6f\n", 1.0e-3);
        msg += eil::format("The maximum value of the bracket is: %12.6f\n", 200.0);
        msg += "The message from the brent.solve function is:\n";
        msg += e.msg();
        throw eil::ExceptionBase(msg, __FILE__, __LINE__);
    }
}

VinokurGeomHybridFunction::VinokurGeomHybridFunction(const VinokurGeomHybridFunction& other) {
    n = other.n;
    s0 = other.s0;
    r0 = other.r0;
    n0 = other.n0;
    l0 = other.l0;
    s1 = other.s1;
    r1 = other.r1;
    n1 = other.n1;
    l1 = other.l1;
    vs0 = other.vs0;
    vs1 = other.vs1;
    vn = other.vn;
    A = other.A;
    B = other.B;
    beta = other.beta;
}

std::shared_ptr<VinokurGeomHybridFunction> VinokurGeomHybridFunction::dup() const {
    return std::make_shared<VinokurGeomHybridFunction>(*this);
}

double VinokurGeomHybridFunction::operator()(double x) const {
    if (x <= static_cast<double>(n0) / static_cast<double>(n)) {
        return geometricSeriesLength(s0, r0, static_cast<int>(std::round(x * n)));
    }
    if (x > 1 - static_cast<double>(n1) / static_cast<double>(n)) {
        return 1 - geometricSeriesLength(s1, r1, n - static_cast<int>(std::round(x * n)));
    }
    double xi = (x - static_cast<double>(n0) / static_cast<double>(n)) /
                (1 - static_cast<double>(n1) / static_cast<double>(n) -
                 static_cast<double>(n0) / static_cast<double>(n));
    double u = 0.5 * (1 + std::tanh(beta * (xi - 0.5)) / std::tanh(0.5 * beta));
    return u / (A + (1 - A) * u) * (1 - l1 - l0) + l0;
}

double VinokurGeomHybridFunction::geometricSeriesLength(double s, double r, int n) const {
    if (r == 1.0) { return s * n; }
    return s * (eil::pow(r, n) - 1) / (r - 1);
}

} // namespace geom
} // namespace eil
