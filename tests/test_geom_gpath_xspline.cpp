// tests/test_geom_gpath_xspline.cpp
//
// Two layers, both required (the pattern of tests/test_geom_elements_properties.cpp):
//
//  A. the `unittest` block of geom/gpath/xspline.d (D:83-117) ported assertion for
//     assertion, in source order: the 20 Runge samples, the 100-point sweep and the
//     approxEqualVectors(p, Vector3(xx, y_runge), 0.02) test at each t. D's own
//     writefln inside that loop is commented out (D:113), so the loop contributes
//     assertions only; the same quantities are corpus rows in layer B. The
//     `version (complex_numbers)` arm (D:87-90) calls util.test_runner:skip() and is
//     carried under EIL_COMPLEX_NUMBERS, not compiled (brief section 2, T-904).
//
//  B. a line-for-line replay of probes/geom/gpath/probe_xspline.d against
//     baselines/e587b235/probes/geom_gpath_xspline.txt: 650 lines covering the knot
//     arrays (XSpline.toString() prints exactly these), every polynomial piece
//     evaluated at its ends and quarters, the +-1e-12 window on both sides of every
//     knot (spline.d:71-83 searches from the right, so that is where the segment
//     index changes), the two out-of-range extrapolation branches, the t mapping,
//     the Path-level dump (dpdt/d2pdt2 are Path's finite differences because XSpline
//     overrides neither), the file constructor, the copy constructor and dup, and
//     the error paths.
//
// The cubic coefficients a/b/c cannot be printed: they are `private` in
// nm/spline.d:88, unreachable from a D probe. They are pinned indirectly instead --
// the knots (which determine them through the natural end conditions plus
// bbla's solve) and evaluations at the segment ends, quarters and knot windows, so
// any coefficient error shows up as a line diff.
//
// The copy path is checked behaviourally (probe rows "xs.aliaschk"): D's
// xspline.d:55 runs `new CubicSpline(other.spl)`, which re-enters the constructor
// with other's knots (spline.d:63-66), so after a copy the two objects own separate
// arrays. The test writes a sentinel into the copy and re-reads the original.
//
// Regenerate the baseline (full recipe in the probe header, including the
// SUBSTRATE NOTE about the atan/to!double message rows):
//   ldc2 -i -I/tmp/gdtk_shadow ... probes/geom/gpath/probe_xspline.d -of=/tmp/x
//   /tmp/x > baselines/e587b235/probes/geom_gpath_xspline.txt
//
// Run: ./test_geom_gpath_xspline <baseline.txt>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"
#include "geom/gpath/xspline.hpp"
#include "nm/number.hpp"
#include "nm/spline.hpp"

#include "gpath3_dump.hpp"

using eil::geom::Vector3;
using eil::geom::XSpline;
using eil_test_gpath3::bools;
using eil_test_gpath3::emitf;

namespace {

std::string data_dir;

double runge(double x) {  // xspline.d:91-93
    return 1.0 / (1.0 + 25 * x * x);
}

// probe_xspline.d:39-50
void dump_knots(const char* tag, const XSpline& s) {
    emitf("%s class=%s", tag, s.classString().c_str());
    emitf("%s str=%s", tag, s.toString().c_str());
    emitf("%s npts=%d", tag, static_cast<int>(s.spl.xs.length()));
    for (size_t i = 0; i < s.spl.xs.length(); ++i) {
        emitf("%s knot[%d] x=%.17g y=%.17g", tag, static_cast<int>(i), s.spl.xs[i], s.spl.ys[i]);
    }
    emitf("%s xmin=%.17g xmax=%.17g", tag, s.spl.xmin(), s.spl.xmax());
}

// probe_xspline.d:53-75
void dump_evals(const char* tag, const XSpline& s) {
    const eil::Array<double>& xs = s.spl.xs;
    size_t n = xs.length();
    for (size_t i = 0; i < n; ++i) {
        double lo = xs[i];
        double hi = (i + 1 < n) ? xs[i + 1] : lo + (xs[n - 1] - xs[0]) / static_cast<double>(n - 1);
        for (double f : {0.0, 0.25, 0.5, 0.75, 1.0}) {
            double x = lo + (hi - lo) * f;
            double y = s.spl(x);
            emitf("%s eval x=%.17g y=%.17g runge=%.17g err=%.17g", tag, x, y, runge(x),
                  y - runge(x));
        }
        for (double eps : {-1.0e-12, 1.0e-12}) {
            double x = lo + eps * (xs[n - 1] - xs[0]);
            emitf("%s win x=%.17g y=%.17g", tag, x, s.spl(x));
        }
    }
    for (double x : {xs[0] - 0.3, xs[n - 1] + 0.3, 0.0}) {
        emitf("%s oor x=%.17g y=%.17g", tag, x, s.spl(x));
    }
}

// probe_xspline.d:78-97
void dump_tmap(const char* tag, const XSpline& s) {
    for (double t : eil_test_gpath3::t_sweep()) {
        Vector3 p = s(t);
        emitf("%s tmap t=%.17g x=%.17g y=%.17g z=%.17g", tag, t, p.x, p.y, p.z);
    }
    int N = 100;
    double dt = 1.0 / (N - 1);
    double max_err = 0.0;
    for (int i = 0; i < N; ++i) {
        double t = dt * i;
        double xx = s(0.0).x * (1.0 - t) + s(1.0).x * t;  // xspline.d:110
        double y_runge = runge(xx);
        Vector3 p = s(t);
        double err = p.y - y_runge;
        if (std::fabs(err) > max_err) max_err = std::fabs(err);
        emitf("%s u %d x=%.17g y_runge=%.17g y=%.17g err=%.17g within2pc=%s", tag, i, xx, y_runge,
              p.y, err,
              bools(eil::geom::approxEqualVectors(p, Vector3(xx, y_runge), 0.02)));
    }
    emitf("%s max_relerr=%.17g", tag, max_err);
}

// ============================================================================
// A. the D unittest block, assertion for assertion, in D's order.
// ============================================================================
void port_d_unittest() {
#ifdef EIL_COMPLEX_NUMBERS
    // D:87-90 — `version (complex_numbers) { skip(); }`: XSpline is incompatible
    // with complex numbers, so the D test runner records the block as skipped and
    // asserts nothing. Carried for the wave-12 build (T-904); never compiled here.
    return;
#endif
    // D:91-105 — the 20 Runge samples.
    int N = 20;
    double x0 = -1.0;
    double x1 = 1.0;
    double dx = (x1 - x0) / (N - 1);
    eil::Array<double> x_sample, y_sample;
    for (int i = 0; i < N; ++i) {          // D: foreach (i; 0 .. N)
        double xx = x0 + dx * i;
        x_sample.append(xx);
        y_sample.append(runge(xx));
    }
    auto s = std::make_shared<XSpline>(x_sample, y_sample);
    // D:106-115 — 100 points, each asserted against the exact Runge value at the
    // mapped x with approxEqualVectors' 2% relative tolerance.
    N = 100;
    double dt = 1.0 / (N - 1);
    for (int i = 0; i < N; ++i) {
        double t = dt * i;
        double xx = (*s)(0.0).x * (1.0 - t) + (*s)(1.0).x * t;
        double y_runge = runge(xx);
        Vector3 p = (*s)(t);
        CHECK3(eil::geom::approxEqualVectors(p, Vector3(xx, y_runge), 0.02),
               "D:114 approxEqualVectors(p, Vector3(xx, y_runge), 0.02)");
    }
}

// ============================================================================
// B. the probe replay, in probe order.
// ============================================================================
void replay_probe() {
    using namespace eil_test_gpath3;

    // probe_xspline.d:103-117 — the D unittest's own spline.
    int N = 20;
    double x0 = -1.0;
    double x1 = 1.0;
    double dx = (x1 - x0) / (N - 1);
    eil::Array<double> x_sample, y_sample;
    for (int i = 0; i < N; ++i) {
        double xx = x0 + dx * i;
        x_sample.append(xx);
        y_sample.append(runge(xx));
    }
    auto s = std::make_shared<XSpline>(x_sample, y_sample);
    dump_knots("xs.runge", *s);
    dump_evals("xs.runge", *s);
    dump_tmap("xs.runge", *s);
    dump_path("xs.runge", s);

    // probe_xspline.d:120-126 — the file constructor (xspline.d:34-51).
    auto sf = std::make_shared<XSpline>(data_dir + "/gpath_xspline.dat");
    dump_knots("xs.file", *sf);
    dump_tmap("xs.file", *sf);
    auto so = std::make_shared<XSpline>(data_dir + "/gpath_points3d.dat");
    emitf("xs.3col npts=%d str=%s", static_cast<int>(so->spl.xs.length()),
          so->toString().c_str());

    // probe_xspline.d:129-142 — copy constructor, dup, and the anti-aliasing
    // sentinel.
    auto sc = std::make_shared<XSpline>(*s);
    dump_knots("xs.copy", *sc);
    dump_tmap("xs.copy", *sc);
    auto sd = std::static_pointer_cast<XSpline>(s->dup());
    emitf("xs.dup class=%s str=%s", sd->classString().c_str(), sd->toString().c_str());
    sc->spl.xs[0] = -987.654321;
    sc->spl.ys[1] = -123.456;
    emitf("xs.aliaschk copy.xs0=%.17g orig.xs0=%.17g copy.ys1=%.17g orig.ys1=%.17g", sc->spl.xs[0],
          s->spl.xs[0], sc->spl.ys[1], s->spl.ys[1]);
    emitf("xs.aliaschk orig.str=%s", s->toString().c_str());

    // probe_xspline.d:145-166 — the error paths. The bad-token row is taxonomy only
    // (see the SUBSTRATE NOTE in the probe header: eil::to_double's message for a
    // '#' lead differs from Phobos'); the missing-file row too, because D's text is
    // std.stdio.File's own; the empty-file row is a class name only).
    {
        auto t = classify([&]() { auto q = std::make_shared<XSpline>(data_dir + "/gpath_bad.dat"); });
        emitf("xs.err.bad asError=%s asException=%s kind=%s", bools(t.asError),
              bools(t.asException),
              t.asError ? "Error" : (t.asException ? "Exception" : "none"));
    }
    {
        auto t = classify([&]() { auto q = std::make_shared<XSpline>(
            data_dir + "/does_not_exist.dat"); });
        emitf("xs.err.missing.class=%s",
              t.asError ? "Error" : (t.asException ? "Exception" : "none"));
    }
    {
        auto t = classify([&]() { auto q = std::make_shared<XSpline>(
            data_dir + "/gpath_empty.dat"); });
        emitf("xs.err.empty.class=%s",
              t.asError ? "Error" : (t.asException ? "Exception" : "none"));
    }

    // probe_xspline.d:169-181 — the one-segment and two-segment splines (the m = 0
    // branch of the tridiagonal system, spline.d:41-52).
    eil::Array<double> tx = {0.0, 1.0};
    eil::Array<double> ty = {0.0, 1.0};
    auto s2 = std::make_shared<XSpline>(tx, ty);
    dump_knots("xs.two", *s2);
    for (double t : {0.0, 0.25, 0.5, 0.75, 1.0}) {
        Vector3 p = (*s2)(t);
        emitf("xs.two t=%.17g x=%.17g y=%.17g", t, p.x, p.y);
    }
    tx = eil::Array<double>{0.0, 0.5, 1.0};
    ty = eil::Array<double>{1.0, 0.0, 1.0};
    auto s3 = std::make_shared<XSpline>(tx, ty);
    dump_knots("xs.three", *s3);
    for (double t : {0.0, 0.25, 0.5, 0.75, 1.0}) {
        Vector3 p = (*s3)(t);
        emitf("xs.three t=%.17g x=%.17g y=%.17g", t, p.x, p.y);
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: " << argv[0]
                  << " <probes/geom_gpath_xspline.txt> <tests/test_data dir>\n";
        return 2;
    }
    data_dir = argv[2];
    port_d_unittest();
    replay_probe();
    return eil_test_gpath3::report("xspline", argv[1]);
}
