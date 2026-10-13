// tests/test_geom_gpath_xsplinelsq.cpp
//
// Two layers, both required (the pattern of tests/test_geom_elements_properties.cpp):
//
//  A. the `unittest` block of geom/gpath/xsplinelsq.d (D:102-143) ported assertion
//     for assertion, in source order: 200 Runge samples with the end weights at
//     100, an empty knot array (so the knots come from splinelsq.d:59-63), nseg=10,
//     then 100 evaluations each asserted with `fabs(dy) < 0.02` and the running
//     max_dy. The `version (complex_numbers)` arm (D:106-109) is skip(), carried
//     under EIL_COMPLEX_NUMBERS and not compiled (brief section 2, T-904).
//
//  B. a line-for-line replay of probes/geom/gpath/probe_xsplinelsq.d against
//     baselines/e587b235/probes/geom_gpath_xsplinelsq.txt: 643 lines covering the
//     fit's public state (xd, yd, wd, xs, nseg -- what toString() prints), the
//     recovered knot values (the fitted spline interpolates its own knots, so
//     spl(xs[i]) *is* the optimised y[i]; the inner CubicSpline of CubicSplineLsq is
//     private in splinelsq.d:122 and cannot be printed), the t mapping including
//     outside the knot range, the Path-level dump, the file constructor with and
//     without a weight column, copy/dup with the anti-aliasing sentinel, the four
//     assert-style error paths, and the OPTIMISER'S PATH.
//
// The optimiser counts (nfe, nres) are measured with a faithful replica of
// splinelsq.d:52-87 -- the same initial guess, the same perturbation, tol 1.0e-6,
// P 2, nfe_max 1200, and a module-state `sse` with the body of splinelsq.d:70-80 --
// because D's CubicSplineLsq keeps nfe/nres as locals with its four writeln lines
// commented out (splinelsq.d:88-91), so they are not observable through the Path.
// The replica's final knot vector is printed next to the real object's own
// interpolation of the same knots ("final[i] y=... obj=... diff="), which is what
// proves the replica is the same call: in D the diff is 0 for 10 of 11 knots (and
// 2.8e-17 for the eleventh, an interpolation residual, not a different optimum).
// The same optimum in a different number of steps would mean the simplex diverged,
// so nfe/nres/converged are part of the corpus.
//
// Regenerate the baseline (full recipe in the probe header):
//   ldc2 -i -I/tmp/gdtk_shadow ... probes/geom/gpath/probe_xsplinelsq.d -of=/tmp/x
//   /tmp/x > baselines/e587b235/probes/geom_gpath_xsplinelsq.txt
//
// Run: ./test_geom_gpath_xsplinelsq <baseline.txt> <tests/test_data dir>
#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"
#include "geom/gpath/xsplinelsq.hpp"
#include "nm/nelmin.hpp"
#include "nm/number.hpp"
#include "nm/spline.hpp"

#include "gpath3_dump.hpp"

using eil::geom::Vector3;
using eil::geom::XSplineLsq;
using eil_test_gpath3::bools;
using eil_test_gpath3::emitf;

namespace {

std::string data_dir;

double runge(double x) {  // xsplinelsq.d:110-112
    return 1.0 / (1.0 + 25 * x * x);
}

// -----------------------------------------------------------------------------
// The replica of splinelsq.d:70-80's nested sse: module-level state plays the part
// of the closure's `this` (it writes the module-level `spl` on every evaluation).
// Bodies are copied statement for statement from D.
// -----------------------------------------------------------------------------
struct Replica {
    eil::Array<double> xd, yd, wd, xs;
    eil::nm::CubicSpline spl;
};
Replica g;

struct Sse {
    double operator()(const eil::Slice<double>& ps) const {
        // The parameter vector is the vector of y-values for the spline knots.
        eil_assert(ps.length() == g.xs.length());  // splinelsq.d:72 message
        g.spl = eil::nm::CubicSpline(g.xs.all(), ps);
        double sumerr = 0.0;
        for (size_t j = 0; j < g.xd.length(); ++j) {
            double e = g.yd[j] - g.spl(g.xd[j]);
            sumerr += g.wd[j] * e * e;
        }
        return sumerr;
    }
};

// probe_xsplinelsq.d:80-123 = splinelsq.d:33-87 verbatim, counts printed.
void replicate_fit(const char* tag, const eil::Array<double>& xd, const eil::Array<double>& yd,
                   const eil::Array<double>& wd, const eil::Array<double>& xs_in, int nseg,
                   const XSplineLsq& obj) {
    g.xd = xd.dup();
    size_t nd = xd.length();
    g.yd = yd.dup();
    g.wd = wd.dup();
    g.xs = xs_in.dup();
    eil::Array<double> ys;
    if (g.xs.length() == 0) {
        for (int i = 0; i < nseg + 1; ++i) {  // D: foreach (i; 0 .. nseg+1)
            double frac = static_cast<double>(i) / nseg;
            g.xs.append(xd[0] * (1.0 - frac) + xd[nd - 1] * frac);
            ys.append(yd[0] * (1.0 - frac) + yd[nd - 1] * frac);
        }
    } else {
        for (int i = 0; i < nseg + 1; ++i) {
            double frac = (g.xs[static_cast<size_t>(i)] - xd[0]) / (xd[nd - 1] - xd[0]);
            ys.append(yd[0] * (1.0 - frac) + yd[nd - 1] * frac);
        }
    }
    emitf("%s initial.length=%d", tag, static_cast<int>(ys.length()));
    for (size_t i = 0; i < ys.length(); ++i) {
        emitf("%s initial[%d] x=%.17g y=%.17g", tag, static_cast<int>(i), g.xs[i], ys[i]);
    }
    // splinelsq.d:67, std.algorithm.searching's left-to-right first-extreme scan.
    double max_el = yd[0];
    double min_el = yd[0];
    for (size_t j = 1; j < yd.length(); ++j) {
        if (yd[j] > max_el) max_el = yd[j];
        if (yd[j] < min_el) min_el = yd[j];
    }
    double dy = max_el - min_el;
    dy *= 0.1;
    emitf("%s dy=%.17g", tag, dy);
    eil::Array<double> delys;
    for (int i = 0; i < nseg + 1; ++i) { delys.append(dy); }
    double f;
    int nfe, nres;
    double tol = 1.0e-6;
    int P = 2;
    int nfe_max = 1200;
    bool conv_flag = eil::nm::nelmin::minimize<Sse, double>(Sse{}, ys, f, nfe, nres,
                                                            delys.all(), tol, P, nfe_max);
    emitf("%s counts converged=%s nfe=%d nres=%d f=%.17g", tag, bools(conv_flag), nfe, nres, f);
    for (size_t i = 0; i < ys.length(); ++i) {
        double ov = obj.spl(g.xs[i]);
        emitf("%s final[%d] y=%.17g obj=%.17g diff=%.17g", tag, static_cast<int>(i), ys[i], ov,
              ov - ys[i]);
    }
}

// probe_xsplinelsq.d:125-142
void dump_state(const char* tag, const XSplineLsq& s, int stride = 1) {
    emitf("%s class=%s", tag, s.classString().c_str());
    std::string ts = s.toString();
    if (ts.length() <= 600) {
        emitf("%s str=%s", tag, ts.c_str());
    } else {
        emitf("%s strlen=%d strhead=%s", tag, static_cast<int>(ts.length()),
              ts.substr(0, 60).c_str());
    }
    emitf("%s nseg=%d ndata=%d nknots=%d", tag, s.spl.nseg,
          static_cast<int>(s.spl.xd.length()), static_cast<int>(s.spl.xs.length()));
    for (size_t i = 0; i < s.spl.xd.length(); ++i) {
        if ((i % static_cast<size_t>(stride) != 0) && (i != s.spl.xd.length() - 1)) continue;
        emitf("%s data[%d] x=%.17g y=%.17g w=%.17g", tag, static_cast<int>(i), s.spl.xd[i],
              s.spl.yd[i], s.spl.wd[i]);
    }
    for (size_t i = 0; i < s.spl.xs.length(); ++i) {
        emitf("%s knot[%d] x=%.17g yfit=%.17g", tag, static_cast<int>(i), s.spl.xs[i],
              s.spl(s.spl.xs[i]));
    }
    emitf("%s xmin=%.17g xmax=%.17g", tag, s.spl.xmin(), s.spl.xmax());
}

// probe_xsplinelsq.d:144-171, the D unittest's own loop.
void dump_unittest_rows(const char* tag, const XSplineLsq& s) {
    int N = 100;
    double max_dy = 0.0;
    double dt = 1.0 / (N - 1);
    for (int i = 0; i < N; ++i) {
        double t = dt * i;
        Vector3 p = s(t);
        double y_runge = runge(p.x);
        double dy = p.y - y_runge;
        max_dy = std::fmax(max_dy, std::fabs(dy));
        emitf("%s %g %g %g %g px=%.17g yr=%.17g py=%.17g dy=%.17g ok=%s", tag, p.x, y_runge, p.y,
              dy, p.x, y_runge, p.y, dy, bools(std::fabs(dy) < 0.02));
    }
    emitf("%s max_dy=%s", tag, eil::to_string(max_dy).c_str());
}

// probe_xsplinelsq.d:245-263
void dump_trows(const char* tag, const XSplineLsq& s) {
    for (double t : eil_test_gpath3::t_sweep()) {
        Vector3 p = s(t);
        emitf("%s tmap t=%.17g x=%.17g y=%.17g z=%.17g", tag, t, p.x, p.y, p.z);
    }
    int N = 41;
    for (int i = 0; i < N; ++i) {
        double t = -0.05 + (1.1 * i) / static_cast<double>(N - 1);
        Vector3 p = s(t);
        emitf("%s wide t=%.17g x=%.17g y=%.17g", tag, t, p.x, p.y);
    }
    for (double x : {s.spl.xmin() - 0.4, s.spl.xmax() + 0.4}) {
        emitf("%s oor x=%.17g y=%.17g", tag, x, s.spl(x));
    }
}

// ============================================================================
// A. the D unittest block, assertion for assertion, in D's order.
// ============================================================================
void port_d_unittest() {
#ifdef EIL_COMPLEX_NUMBERS
    // D:106-109 — `version (complex_numbers) { skip(); }`. XSplineLsq is
    // incompatible with complex numbers (T-904); nothing is asserted on that arm.
    return;
#endif
    int N = 200;
    double x0 = -1.0;
    double x1 = 1.0;
    double dx = (x1 - x0) / (N - 1);
    eil::Array<double> x_sample, y_sample, w_sample;
    for (int i = 0; i < N; ++i) {  // D: foreach (i; 0 .. N)
        double xx = x0 + dx * i;
        x_sample.append(xx);
        y_sample.append(runge(xx));
        w_sample.append(1.0);
    }
    w_sample[0] = 100.0;
    w_sample[w_sample.length() - 1] = 100.0;  // D: w_sample[$-1]
    eil::Array<double> xs;
    auto s = std::make_shared<XSplineLsq>(x_sample, y_sample, w_sample, xs, 10);
    // D:130-141, the sweep and its max_dy bookkeeping. The writeln lines D performs
    // inside this loop are replayed as corpus rows in layer B.
    N = 100;
    double max_dy = 0.0;
    double dt = 1.0 / (N - 1);
    for (int i = 0; i < N; ++i) {
        double t = dt * i;
        Vector3 p = (*s)(t);
        double y_runge = runge(p.x);
        double dy = p.y - y_runge;
        max_dy = std::fmax(max_dy, std::fabs(dy));
            CHECK3(std::fabs(dy) < 0.02, "D:139 assert(fabs(dy) < 0.02)");
    }
    // D:141 prints max_dy but does not assert on it; the value is a corpus row in
    // layer B ("xsl.runge max_dy=..."), which is where the port is measured.
}

// ============================================================================
// B. the probe replay, in probe order.
// ============================================================================
void replay_probe() {
    using namespace eil_test_gpath3;

    // probe_xsplinelsq.d:165-184 — the D unittest's fit, and its replica.
    int N = 200;
    double x0 = -1.0;
    double x1 = 1.0;
    double dx = (x1 - x0) / (N - 1);
    eil::Array<double> x_sample, y_sample, w_sample;
    for (int i = 0; i < N; ++i) {
        double xx = x0 + dx * i;
        x_sample.append(xx);
        y_sample.append(runge(xx));
        w_sample.append(1.0);
    }
    w_sample[0] = 100.0;
    w_sample[w_sample.length() - 1] = 100.0;
    eil::Array<double> xs;
    auto s = std::make_shared<XSplineLsq>(x_sample, y_sample, w_sample, xs, 10);
    dump_state("xsl.runge", *s);
    dump_unittest_rows("xsl.runge", *s);
    dump_trows("xsl.runge", *s);
    dump_path("xsl.runge", s);
    replicate_fit("xsl.runge", x_sample, y_sample, w_sample, xs, 10, *s);

    // probe_xsplinelsq.d:187-193 — a supplied knot vector (splinelsq.d:60-64).
    eil::Array<double> xsup = {-1.0, -0.5, 0.0, 0.5, 1.0};
    auto s2 = std::make_shared<XSplineLsq>(x_sample, y_sample, w_sample, xsup, 4);
    dump_state("xsl.knots", *s2, 20);
    dump_trows("xsl.knots", *s2);
    replicate_fit("xsl.knots", x_sample, y_sample, w_sample, xsup, 4, *s2);

    // probe_xsplinelsq.d:196-205 — the file constructor (xsplinelsq.d:35-66).
    auto s3 = std::make_shared<XSplineLsq>(data_dir + "/gpath_weights.dat", xs, 5);
    dump_state("xsl.file", *s3);
    auto s4 = std::make_shared<XSplineLsq>(data_dir + "/gpath_weights.dat", xsup, 4);
    dump_state("xsl.file.knots", *s4);
    emitf("xsl.file caller.xs.length=%d caller.xs0=%.17g", static_cast<int>(xs.length()),
          (xs.length() > 0) ? xs[0] : 0.0);
    emitf("xsl.file caller.xsup.length=%d", static_cast<int>(xsup.length()));

    // probe_xsplinelsq.d:208-210 — the two-column file, so wd is empty and
    // splinelsq.d:39-41 fills it with 1.0.
    auto s5 = std::make_shared<XSplineLsq>(data_dir + "/gpath_xspline.dat", xs, 4);
    emitf("xsl.noweight nseg=%d ndata=%d w0=%.17g str=%s", s5->spl.nseg,
          static_cast<int>(s5->spl.xd.length()), s5->spl.wd[0], s5->toString().c_str());

    // probe_xsplinelsq.d:213-222 — copy ctor (D:68-71), dup (D:73-76), sentinel.
    auto sc = std::make_shared<XSplineLsq>(*s);
    dump_state("xsl.copy", *sc, 20);
    auto sd = std::static_pointer_cast<XSplineLsq>(s->dup());
    std::string dups = sd->toString();
    emitf("xsl.dup class=%s strlen=%d strhead=%s", sd->classString().c_str(),
          static_cast<int>(dups.length()), dups.substr(0, 60).c_str());
    sc->spl.xd[0] = -987.654321;
    sc->spl.xs[1] = -123.456;
    emitf("xsl.aliaschk copy.xd0=%.17g orig.xd0=%.17g copy.xs1=%.17g orig.xs1=%.17g", sc->spl.xd[0],
          s->spl.xd[0], sc->spl.xs[1], s->spl.xs[1]);
    emitf("xsl.aliaschk copy.eval.at.knot1=%.17g orig.eval.at.knot1=%.17g",
          sc->spl(s->spl.xs[1]), s->spl(s->spl.xs[1]));

    // probe_xsplinelsq.d:229-242 — the four assert-style error paths, printed as
    // classifications only (an assert's D message carries the reference tree's file
    // and line, which no port can echo).
    const char* cls_err = "Error";
    const char* cls_exc = "Exception";
    const char* cls_none = "none";
    {
        auto t = classify([&]() {
            auto q = std::make_shared<XSplineLsq>(data_dir + "/gpath_weights.dat", xs, 2);
        });
        emitf("xsl.err.nseg2.class=%s", t.asError ? cls_err : (t.asException ? cls_exc : cls_none));
    }
    {
        auto t = classify([&]() {
            auto q = std::make_shared<XSplineLsq>(data_dir + "/gpath_weights.dat", xsup, 8);
        });
        emitf("xsl.err.nsegknots.class=%s",
              t.asError ? cls_err : (t.asException ? cls_exc : cls_none));
    }
    {
        auto t = classify([&]() {
            auto q = std::make_shared<XSplineLsq>(data_dir + "/gpath_empty.dat", xs, 3);
        });
        emitf("xsl.err.empty.class=%s", t.asError ? cls_err : (t.asException ? cls_exc : cls_none));
    }
    {
        eil::Array<double> d3 = {0.0, 1.0, 2.0};
        eil::Array<double> w3 = {1.0, 1.0, 1.0};
        auto t = classify([&]() {
            auto q = std::make_shared<XSplineLsq>(d3, d3, w3, xs, 2 + 1);
        });
        emitf("xsl.err.toofew.class=%s", t.asError ? cls_err : (t.asException ? cls_exc : cls_none));
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: " << argv[0]
                  << " <probes/geom_gpath_xsplinelsq.txt> <tests/test_data dir>\n";
        return 2;
    }
    data_dir = argv[2];
    port_d_unittest();
    replay_probe();
    return eil_test_gpath3::report("xsplinelsq", argv[1]);
}
