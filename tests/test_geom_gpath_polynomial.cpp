// tests/test_geom_gpath_polynomial.cpp
//
// Verification of geom/gpath/polynomial.d (124 D lines)
// -> src/geom/gpath/polynomial.{hpp,cpp}.
//
//  A. unittest replay: polynomial.d has NO `unittest` block in D (measured: the file
//     ends with class Polynomial at :124 and contains no `unittest` keyword). What
//     D does assert about this module is indirect, through geom/gpath/polyline.d and
//     its consumers, so section A carries live checks of the two contracts the class
//     itself states in code: the (P, C) constructor's point-versus-coefficient test
//     (D:50-54, which dup() re-runs because it is implemented as
//     `new Polynomial(P, C)`), and the four Error paths of the constructors.
//
//  B. a line-for-line replay of probes/geom/gpath/probe_polynomial.d against
//     baselines/e587b235/probes/geom_gpath_polynomial.txt (548 D lines at %.17g).
//     The coefficients are dumped themselves (the "poly.* C[i]" rows), not just the
//     curve they generate, because evaluate_coefficients() solves a least-squares
//     problem through nm/bbla's gaussJordanElimination (polynomial.d:100) and a
//     different pivoting order still draws a plausible curve.
//
//  Two D behaviours that look wrong and are preserved:
//   * derivative_polynomial (polynomial.d:115-122) sums `C[i] * i * x^^(i-1)` from
//     i = 0, and the loop index is a size_t, so the i == 0 term is
//     C[0]*0*x^^18446744073709551615. Measured with ldc2: that is +inf for |x| > 1,
//     so the term becomes 0*inf = -nan and dpdt() prints -nan outside the unit
//     range ("poly.deriv" rows) while staying finite inside it ("poly.deriv.inrange").
//   * dup() throws for an over-determined fit ("poly.dup.five"), because the
//     residual makes `point.y != evaluate_polynomial(point.x).y` true.
//   * D's `to!int(length)` conversions mentioned in the stage brief are not in this
//     module: grep over path.d/line.d/arc.d/bezier.d/polynomial.d finds no `to!int`
//     site at all (they are in polyline.d/modifiedpath.d, stage 2 of this directory).
//
// Run: ./test_geom_gpath_polynomial ../baselines/e587b235/probes/geom_gpath_polynomial.txt

#include <cmath>
#include <memory>
#include <string>

#include "eil/eil.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"
#include "geom/gpath/polynomial.hpp"
#include "nm/number.hpp"

#include "gpath1_dump.hpp"

using eil::geom::Path;
using eil::geom::PathPtr;
using eil::geom::Polynomial;
using eil::geom::Vector3;
using eil::number;

using namespace eil_test_gpath1;

namespace {

// Mirrors dump_coeffs() in probes/geom/gpath/probe_polynomial.d.
void dump_coeffs(const char* tag, const eil::Array<number>& C) {
    for (size_t i = 0; i < C.length(); ++i) {
        emitf("%s C[%d]=%.17g", tag, static_cast<int>(i), C[i]);
    }
    emitf("%s ncoef=%d", tag, static_cast<int>(C.length()));
}

// ============================================================================
// A. live checks (D has no unittest block in this module)
// ============================================================================
void port_d_unittest() {
    const eil::Array<Vector3> pts = {Vector3(-1.0, 1.0), Vector3(0.0, 0.0), Vector3(1.0, 1.0)};
    Polynomial p = Polynomial(pts);
    // The exact fit through (-1,1), (0,0), (1,1) is y = x^2, and the solve is the
    // one nm/bbla runs: the coefficients themselves, not just the curve.
    CHECK1(p.C.length() == 3, "D:91-103 one coefficient per control point");
    CHECK1(p.C[0] == 0.0 && p.C[1] == 0.0 && p.C[2] == 1.0, "D:100 least-squares gives C = [0, 0, 1]");
    CHECK1(p.classString() == "Polynomial", "D:83-86 classString()");
    CHECK1(p.toString() == "Polynomial(P=[Vector3(-1, 1, 0), Vector3(0, 0, 0), Vector3(1, 1, 0)])",
           "D:79-82 toString() renders every control point");

    // opCall maps t in [0,1] onto the x range of the points (D:66-71), endpoints
    // included.
    const Vector3 p0 = p(0.0);
    const Vector3 p1 = p(1.0);
    CHECK1(p0.x == -1.0 && p0.y == 1.0, "D:69 opCall(0.0) is the first point's x");
    CHECK1(p1.x == 1.0 && p1.y == 1.0, "D:69 opCall(1.0) is the last point's x");

    // dpdt is the analytic derivative (D:72-78), and D:76 wraps its x extent the
    // same way opCall does.
    const Vector3 d50 = p(0.5);
    CHECK1(d50.x == 0.0 && d50.y == 0.0, "y = x^2 at x = 0");

    // The (P, C) constructor rejects points that do not lie on the polynomial
    // (D:50-54) -- the check dup() re-runs.
    const eil::Array<number> good = {0.0, 0.0, 1.0};
    const eil::Array<number> bad = {0.0, 0.0, 2.0};
    bool threw_good = false;
    bool threw_bad = false;
    try { Polynomial(p.P, good); } catch (const eil::ErrorBase&) { threw_good = true; }
    try { Polynomial(p.P, bad); } catch (const eil::ErrorBase&) { threw_bad = true; }
    CHECK1(!threw_good, "D:44-56 matching points and coefficients are accepted");
    CHECK1(threw_bad, "D:51-53 mismatched coefficients throw new Error");

    // The two empty-input Error paths (D:25-30, :36-38, :46-48).
    bool threw_empty = false;
    try { Polynomial(eil::Array<Vector3>()); } catch (const eil::ErrorBase&) { threw_empty = true; }
    CHECK1(threw_empty, "D:25-27 no control points throws new Error");
    bool threw_one = false;
    try { Polynomial(eil::Array<Vector3>{Vector3(1.0, 2.0)}); }
    catch (const eil::ErrorBase&) { threw_one = true; }
    CHECK1(threw_one, "D:28-30 one control point throws new Error");
    bool threw_noc = false;
    try { Polynomial(eil::Array<number>(), 0.0, 1.0); } catch (const eil::ErrorBase&) { threw_noc = true; }
    CHECK1(threw_noc, "D:36-38 no coefficients throws new Error");

    // ErrorBase, not ExceptionBase: D writes `throw new Error(...)` here (D:26, :29,
    // :37, :47, :52), and a ported `catch (Exception e)` must not see it.
    bool seen_as_exception = false;
    try {
        Polynomial(eil::Array<Vector3>());
    } catch (const eil::ExceptionBase&) {
        seen_as_exception = true;
    } catch (const eil::ErrorBase&) {
    }
    CHECK1(!seen_as_exception, "the empty-input throw is an Error, not an Exception");
}

// ============================================================================
// B. the probe replay, in probe order.
// ============================================================================
void replay_probe() {
    // A parabola through three points inside |x| <= 1.
    Polynomial p1 = Polynomial(eil::Array<Vector3>{Vector3(-1.0, 1.0), Vector3(0.0, 0.0),
                                                   Vector3(1.0, 1.0)});
    dump_path("poly.parab", p1);
    dump_coeffs("poly.parab", p1.C);
    emitf("poly.parab xrange a=%.17g b=%.17g", p1.P[0].x, p1.P[p1.P.length() - 1].x);

    // The same shape scaled so that x runs from -2 to 2: the i == 0 wraparound term
    // then multiplies 0 by +inf.
    Polynomial p2 = Polynomial(eil::Array<Vector3>{Vector3(-2.0, 4.0), Vector3(0.0, 0.0),
                                                   Vector3(2.0, 4.0)});
    dump_path("poly.wide", p2);
    dump_coeffs("poly.wide", p2.C);
    for (double t : {0.0, 0.25, 0.5, 1.0}) {
        Vector3 d = p2.dpdt(t);
        emitf("poly.deriv t=%.17g x=%.17g y=%.17g z=%.17g", t, d.x, d.y, d.z);
        Vector3 d0 = p1.dpdt(t);
        emitf("poly.deriv.inrange t=%.17g x=%.17g y=%.17g z=%.17g", t, d0.x, d0.y, d0.z);
    }

    // A quartic-ish fit through five points (least squares over-determined by one).
    Polynomial p3 = Polynomial(eil::Array<Vector3>{
        Vector3(0.0, 1.0), Vector3(0.25, 1.0954), Vector3(0.5, 1.1892), Vector3(0.75, 1.2649),
        Vector3(1.0, 1.3229)});
    dump_path("poly.five", p3);
    dump_coeffs("poly.five", p3.C);

    // The coefficients constructor (polynomial.d:34-43).
    eil::Array<number> cs = {1.0, 2.0, -0.5, 0.125};
    Polynomial p4 = Polynomial(cs, 0.0, 1.0);
    dump_path("poly.fromC", p4);
    dump_coeffs("poly.fromC", p4.C);
    emitf("poly.fromC P0 x=%.17g y=%.17g P1 x=%.17g y=%.17g", p4.P[0].x, p4.P[0].y, p4.P[1].x,
          p4.P[1].y);

    // The (P, C) constructor, which checks the points against the coefficients.
    Polynomial p5 = Polynomial(p4.P, cs);
    dump_path("poly.fromPC", p5);
    eil::Array<number> wrong = {1.0, 2.0, -0.5, 0.25};
    dump_error("poly.err.mismatch", [&p4, &wrong]() { Polynomial(p4.P, wrong); });
    dump_error("poly.err.nocoef", []() { Polynomial(eil::Array<number>(), 0.0, 1.0); });
    dump_error("poly.err.noPoints", []() { Polynomial(eil::Array<Vector3>()); });
    dump_error("poly.err.onePoint",
               []() { Polynomial(eil::Array<Vector3>{Vector3(1.0, 2.0)}); });

    // dup() re-runs the (P, C) check, so it throws for an over-determined fit.
    dump_error("poly.dup.exact", [&p1]() { p1.dup(); });
    dump_error("poly.dup.wide", [&p2]() { p2.dup(); });
    dump_error("poly.dup.five", [&p3]() { p3.dup(); });
    dump_error("poly.dup.fromC", [&p4]() { p4.dup(); });
    {
        const PathPtr q = p1.dup();
        const Vector3 v = (*q)(0.5);
        emitf("poly.dup.exactok t=%.17g x=%.17g y=%.17g z=%.17g", 0.5, v.x, v.y, v.z);
    }
    dump_path("poly.copy", Polynomial(p3));

    // A flat polynomial and a rank-deficient one.
    Polynomial p6 = Polynomial(eil::Array<Vector3>{Vector3(0.0, 1.0), Vector3(1.0, 1.0),
                                                   Vector3(2.0, 1.0)});
    dump_path("poly.flat", p6);
    dump_coeffs("poly.flat", p6.C);
    dump_error("poly.dup.flat", [&p6]() { p6.dup(); });
    try {
        Polynomial p7 = Polynomial(eil::Array<Vector3>{Vector3(0.0, 1.0), Vector3(0.0, 2.0),
                                                       Vector3(1.0, 3.0)});
        dump_path("poly.rankdeficient", p7);
        dump_coeffs("poly.rankdeficient", p7.C);
    } catch (const eil::ThrowableBase& e) {
        // D catches Throwable here (probe line 118), i.e. both branches.
        emitf("poly.rankdeficient threw msg=%s", e.msg().c_str());
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <probes/geom_gpath_polynomial.txt>\n";
        return 2;
    }
    port_d_unittest();
    replay_probe();
    return report("polynomial", argv[1], argc > 2);
}
