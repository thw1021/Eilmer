#pragma once
// geom/misc/univariatefunctions.d -> src/geom/misc/univariatefunctions.hpp
// (module geom.misc.univariatefunctions).
//
// The clustering (grid-spacing) functions. geom/grid/sgrid.d takes
// `const(UnivariateFunction)[] clusterf` and calls
// distribute_parameter_values(n) on each face to place the rows/columns of a block,
// so this is the code that decides where every grid line of the reference solver
// falls. The nine classes of the module are ported: the base plus the eight derived
// classes (the ninth subclass in the wave-02 brief's count is
// geom/luawrap/luaunifunction.d:207 LuaFnUnivariateFunction, which is Wave 7).
//
// Namespace: eil::geom, flat, like the other three misc modules. Checked: no geom
// module other than univariatefunctions.d declares UnivariateFunction, roberts_original,
// or any of the eight class names, so nothing forced an eil::geom::<module> scope
// (wave-02-brief section 2 asks for the reason when one is used; here there is none).
// The classes are only reachable in D via this module, and the private members of one
// class never leak into the namespace.
//
// Non-1:1 notes (all registered in provenance/fidelity.yml):
//   * D's `opCall(double t) const` -> `virtual double operator()(double t) const`, per
//     wave-02-brief section 4b. `toString()` stays virtual because the log-comparison
//     gate prints it.
//   * The base class is NOT abstract: D:19 gives opCall a body ("expect this to be
//     overridden") and D:27 gives distribute_parameter_values a body, and
//     `new UnivariateFunction()` is legal (the probe constructs one, and
//     sgrid.d's defaults hold a base-typed handle). So there are no pure virtuals here,
//     unlike Path/ParametricSurface/ParametricVolume, whose D bases are abstract.
//   * There is NO `dup()` in the base class, because D has none: each derived class
//     declares its own `X dup() const` returning the concrete type (D:55, :90, :145,
//     :217, :333, :421, :627, :740). The port therefore declares eight non-virtual
//     dup()s, each returning std::shared_ptr<that class> (adapted-handles: D class
//     values are references, so `dup()` in D is a fresh object and the ported return
//     type has to own). Nothing in geom/ calls dup() through a base handle, which is
//     why D compiles without a virtual dup.
//   * D's `throw new Error(...)` sites (GeometricFunction D:199, :200, :202, :253;
//     GaussianFunction :305, :306; GaussGeomHybridFunction :396-:402; VinokurFunction
//     :588; VinokurGeomHybridFunction :682, :692; autoKnotVector is in nurbs_utils)
//     become eil::ErrorBase, and D's `throw new Exception(...)` sites
//     (GaussGeomHybridFunction D:550, VinokurFunction D:613, VinokurGeomHybridFunction
//     D:717) become eil::ExceptionBase. Error and Exception are siblings, so a ported
//     `catch (const ExceptionBase&)` (as the two Vinokur ctors do) does NOT catch the
//     guard Errors — same as D. Verified by the "err" rows of the probe replay.
//   * The Vinokur constructors call brent on a nested function `zeroFun` that closes
//     over the local B (D:596-601, :700-705). Per wave-02-brief section 4b.5 that maps
//     to a lambda, and it needs the callable entry point of eil::nm::brent::solve,
//     which is what src/nm/brent.hpp now provides (shared solve_fn body, so the two
//     entry points cannot drift apart).
//   * `debug { ... }` blocks in the two Vinokur catch clauses (D:607-612, :711-716) are
//     kept unconditionally, as src/nm/brent.hpp:84-86 does. Measured reason: a plain
//     ldc2 invocation compiles debug blocks OUT (only --d-debug=1 keeps them), while the
//     frozen reference is "Build-flavour: debug" and its message includes those lines,
//     so the probe is captured with --d-debug=1 and the port keeps the text.
//   * The three iteration counters (lambertW_iterations, centre_iterations,
//     xswitch_iterations) do not exist in D: they mirror the statements that
//     probes/geom/misc/univariatefunctions_copy.d adds so the loop trip counts are
//     measurable at all. They are public, written only during construction, and read by
//     nothing in the module, so no arithmetic or printed value depends on them. The
//     copy constructors deliberately do not carry them (D's dup() leaves the module
//     global at whatever the last loop wrote, so per-object state would be a lie).
//   * FOUR of the eight copy constructors omit fields, and D's class fields take
//     .init for whatever a constructor does not assign (double.init == quiet NaN, see
//     wave-02-brief section 5). Measured consequences, all preserved (probe rows tagged
//     "dup", baselines/e587b235/probes/geom_misc_univariatefunctions.txt):
//       QuadraticFunction's copy ctor (D:85-89) skips a and b  -> dup() returns NaN
//       GaussianFunction's copy ctor (D:324-331) skips b        -> dup() returns NaN
//       GaussGeomHybridFunction's copy ctor (D:409-419) skips a and c -> NaN
//       VinokurGeomHybrid/VinokurFunction/Roberts/Linear/Geometric copy all the fields
//       their opCall reads, so dup() is faithful there.
//     The port reproduces this by giving every field a double.init NaN default and
//     copying exactly D's list.
//   * pi is NOT EIL_PI in these two classes: D:345 and D:445 declare
//     `immutable double pi = 3.1415926535` (11 digits, deliberately the truncated
//     value of the Abramowitz & Stegun erf fit), together with p/a1..a5 of the same
//     fit. Writing EIL_PI there would change every Gaussian value.
//   * `a ^^ b` does not appear in this module; every power is an explicit pow(), which
//     is eil::pow. D's `pow(r, n)` with an INT n (geometricSeriesLength, D:764) is the
//     libm path, not exponentiation-by-squaring (measured: 80 of 80 rows
//     pow(x,int) == pow(x,(double)int)), so eil::pow's int overload is the match.
//   * distribute_parameter_values computes `1.0 / (n-1)` where n is a size_t: for n == 1
//     that is a division by zero giving +inf (and the row then evaluates f at
//     inf*0 == -nan, printed by the probe as "dist linear.n1 v[0]=-nan"), and for
//     n == 0 it wraps to 1.8446744073709552e19. Both are kept; the loop simply does not
//     run for n == 0.

#include <cstddef>
#include <memory>
#include <string>

#include "eil/array.hpp"
#include "eil/number.hpp"

namespace eil {
namespace geom {

// D:17-39. Base class for functions of one variable.
class UnivariateFunction {
  public:
    virtual ~UnivariateFunction() = default;

    // D:19-22: returns its argument ("expect this to be overridden").
    virtual double operator()(double t) const;

    // D:23-26.
    virtual std::string toString() const;

    // D:27-38. Returns an array of parameter values distributed from t0 through t1;
    // the subclass determines the form of distribution.
    virtual eil::Array<double> distribute_parameter_values(size_t n, double t0 = 0.0,
                                                           double t1 = 1.0) const;
}; // end class UnivariateFunction

// D:41-67.
class LinearFunction : public UnivariateFunction {
  public:
    double t0 = eil::d_init<number>();
    double t1 = eil::d_init<number>();

    explicit LinearFunction(double t0 = 0.0, double t1 = 1.0);
    LinearFunction(const LinearFunction& other);
    std::shared_ptr<LinearFunction> dup() const;
    double operator()(double t) const override;
    std::string toString() const override;
}; // end class LinearFunction

// D:69-108. A cluster function based on a quadratic, for linearly increasing or
// decreasing cell sizes. `ratio` is the largest cell size at the end over the smallest
// at the beginning.
class QuadraticFunction : public UnivariateFunction {
  public:
    QuadraticFunction(double ratio, bool reverse);
    // D:85-89 copies ratio and reverse only, so a and b stay at double.init and dup()
    // evaluates to NaN. Kept as written.
    QuadraticFunction(const QuadraticFunction& other);
    std::shared_ptr<QuadraticFunction> dup() const;
    double operator()(double t) const override;
    std::string toString() const override;

  private:
    double a = eil::d_init<number>();
    double b = eil::d_init<number>();
    double ratio = eil::d_init<number>();
    bool reverse = eil::d_init<bool>();
}; // end class QuadraticFunction

// D:110-166. Stretching parameters for the original Robert's transform.
class RobertsFunction : public UnivariateFunction {
  public:
    bool end0 = eil::d_init<bool>();
    bool end1 = eil::d_init<bool>();
    double alpha = 0.5;
    double beta = eil::d_init<number>();
    bool reverse = false;
    bool cluster = true;

    RobertsFunction(bool end0, bool end1, double beta);
    RobertsFunction(const RobertsFunction& other);
    std::shared_ptr<RobertsFunction> dup() const;
    double operator()(double t) const override;
    std::string toString() const override;
}; // end class RobertsFunction()

// D:168-181. eta: unstretched coordinate 0<=eta<=1; alpha: where the stretching sits
// (0.5 clusters both ends, 0.0 clusters near eta=1); beta: stretching factor.
double roberts_original(double eta, double alpha, double beta);

// D:183-284. Geometric progression from a fixed start size by a constant multiple
// until a threshold, then equally sized cells (mimics GridPro's "clu" utility).
class GeometricFunction : public UnivariateFunction {
  public:
    GeometricFunction(double a, double r, int N, bool reverse);
    GeometricFunction(const GeometricFunction& other);
    std::shared_ptr<GeometricFunction> dup() const;
    double operator()(double t) const override;

    // Trip count of the Lambert-W fixed-point loop of the last construction (0 for a
    // copy, see the header note). Not in D.
    int lambertW_iterations = 0;

  private:
    double a = eil::d_init<number>();
    double r = eil::d_init<number>();
    double ts = eil::d_init<number>();
    double ns = eil::d_init<number>();
    int N = eil::d_init<int>();
    bool reverse = eil::d_init<bool>();

    double lambertW(double z, double wguess = 1.0, double tol = 1e-15);
    double solve_for_nswitch(double a, double r, double N);
}; // end class GeometricFunction

// D:286-368. Gaussian-normal cluster function; m is the location of peak clustering,
// s the width of the peak and ratio smallest cell size / largest cell size.
class GaussianFunction : public UnivariateFunction {
  public:
    GaussianFunction(double m, double s, double ratio);
    // D:324-331 copies m, s, ratio, a, c but not b -> dup() evaluates to NaN.
    GaussianFunction(const GaussianFunction& other);
    std::shared_ptr<GaussianFunction> dup() const;
    double operator()(double x) const override;
    std::string toString() const override;

    // Trip count of the cluster-centre Newton loop of the last construction. Not in D.
    int centre_iterations = 0;

  private:
    double m = eil::d_init<number>();
    double s = eil::d_init<number>();
    double ratio = eil::d_init<number>();
    double a = eil::d_init<number>();
    double b = eil::d_init<number>();
    double c = eil::d_init<number>();

    // D:345-351: this is the A&S 7.1.26 erf fit, whose pi is the 11-digit literal, not
    // the Phobos constant. Keep the literals.
    static constexpr double pi_ = 3.1415926535;
    static constexpr double p_ = 0.3275911;
    static constexpr double a1_ = 0.254829592;
    static constexpr double a2_ = -0.284496736;
    static constexpr double a3_ = 1.421413741;
    static constexpr double a4_ = -1.453152027;
    static constexpr double a5_ = 1.061405429;

    double erf(double x) const;
}; // end class GaussianFunction

// D:370-554. Geometric growth beginning plus a smooth Gaussian cluster in the middle.
class GaussGeomHybridFunction : public UnivariateFunction {
  public:
    GaussGeomHybridFunction(double A, double R, int N, double m, double s, double ratio,
                            bool reverse);
    // D:409-419 copies everything except a and c -> the gaussian half of a dup() is NaN.
    GaussGeomHybridFunction(const GaussGeomHybridFunction& other);
    std::shared_ptr<GaussGeomHybridFunction> dup() const;
    double operator()(double x) const override;

    // Trip count of the Newton iteration that locates the geometric/gaussian switch
    // point, from the last construction. Not in D.
    int xswitch_iterations = 0;

  private:
    double A = eil::d_init<number>();
    double R = eil::d_init<number>();
    double m = eil::d_init<number>();
    double s = eil::d_init<number>();
    double ratio = eil::d_init<number>();
    double xs = eil::d_init<number>();
    int N = eil::d_init<int>();
    double a = eil::d_init<number>();
    double c = eil::d_init<number>();
    bool reverse = eil::d_init<bool>();

    static constexpr double pi_ = 3.1415926535;
    static constexpr double p_ = 0.3275911;
    static constexpr double a1_ = 0.254829592;
    static constexpr double a2_ = -0.284496736;
    static constexpr double a3_ = 1.421413741;
    static constexpr double a4_ = -1.453152027;
    static constexpr double a5_ = 1.061405429;

    double erf(double x) const;
    double Gaussian(double a_, double c_, double x) const;
    double dGaussiandx(double a_, double x) const;
    double d2Gaussiandx2(double a_, double x) const;
    double Geometric(double x) const;
    double dGeometricdx(double x) const;
    double d2Geometricdx2(double x) const;
    double SwitchEquationError(double xs) const;
    double SwitchEquationDerivative(double xs) const;
    double solve_for_xswitch(double guess = 0.5, double tol = 1e-8);
}; // end class GaussGeomHybridFunction

// D:557-641. A cluster function with specified cell sizes at either end (Pointwise's
// tanh distribution; https://ntrs.nasa.gov/citations/19800025680). The stretch factor
// is iterated for with brent.
class VinokurFunction : public UnivariateFunction {
  public:
    VinokurFunction(int n, double s0, double s1);
    VinokurFunction(const VinokurFunction& other);
    std::shared_ptr<VinokurFunction> dup() const;
    double operator()(double x) const override;

  private:
    int n = eil::d_init<int>();
    double s0 = eil::d_init<number>();
    double s1 = eil::d_init<number>();
    double A = eil::d_init<number>();
    double B = eil::d_init<number>();
    double beta_min = eil::d_init<number>();
    double beta_max = eil::d_init<number>();
    double tol = eil::d_init<number>();
    double beta = eil::d_init<number>();
}; // end class VinokurFunction

// D:643-766. Geometric series clustering at both ends of the edge and a Vinokur
// function in between, with the number of cells in each geometric layer explicit.
class VinokurGeomHybridFunction : public UnivariateFunction {
  public:
    VinokurGeomHybridFunction(int n, double s0, int n0, double r0, double s1, int n1,
                              double r1);
    VinokurGeomHybridFunction(const VinokurGeomHybridFunction& other);
    std::shared_ptr<VinokurGeomHybridFunction> dup() const;
    double operator()(double x) const override;

  private:
    int n = eil::d_init<int>();
    int n0 = eil::d_init<int>();
    int n1 = eil::d_init<int>();
    int vn = eil::d_init<int>();
    double s0 = eil::d_init<number>();
    double r0 = eil::d_init<number>();
    double l0 = eil::d_init<number>();
    double s1 = eil::d_init<number>();
    double r1 = eil::d_init<number>();
    double l1 = eil::d_init<number>();
    double vs0 = eil::d_init<number>();
    double vs1 = eil::d_init<number>();
    double A = eil::d_init<number>();
    double B = eil::d_init<number>();
    double beta_min = eil::d_init<number>();
    double beta_max = eil::d_init<number>();
    double tol = eil::d_init<number>();
    double beta = eil::d_init<number>();

    double geometricSeriesLength(double s, double r, int n) const;
}; // end class VinokurGeomHybridFunction

} // namespace geom
} // namespace eil
