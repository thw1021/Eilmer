// tests/test_nm_spline.cpp — the unittest block from nm/spline.d:94-120, plus a
// line-for-line replay of probes/nm/probe_spline.d against
// baselines/e587b235/probes/nm_spline.txt.
//
// The D unittest builds a 20-knot Runge spline, evaluates it at 100 points and
// asserts |y_spline - y_runge| < 0.02 at every one (spline.d:118). Its writefln is
// commented out in D (spline.d:117), so the block prints nothing and cannot be
// diffed; the %.17g dump is what makes the port measurable. It pins, in order:
// the tridiagonal assembly (spline.d:44-50), the Gauss-Jordan solve, the sigma
// placement, the a/b/c coefficients, the stored dup'd knots, the segment search in
// opCall including the off-range-left branch and the x == xmax case, and the
// rebuild-on-copy path. CubicSpline has no derivative method, so the dump's "d="
// column is a central difference of opCall at h=1e-5, which is sensitive to the
// same coefficients.
//
// The clustered-knot case (repeated x, so h == 0.0) is included because it produces
// -nan rather than an exception in D, which pins both the arithmetic and the NaN
// sign the log comparison sees.
//
// Run: ./test_nm_spline ../baselines/e587b235/probes/nm_spline.txt

#include <cstdarg>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "eil/array.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "nm/number.hpp"
#include "nm/spline.hpp"

namespace {

int failures = 0;
long compared = 0;
long differed = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    } else {
        std::printf("ok: %s\n", what);
    }
}

std::vector<std::string> produced;

void emitf(const char* fmt, ...) {
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    produced.emplace_back(buf);
}

double runge(double x) { return 1.0/(1.0 + 25* x * x); }   // spline.d:98

// ---- the ported unittest block: spline.d:94-120 ----------------------------
void unittest_spline() {
    int N = 20;
    double x0 = -1.0;
    double x1 = 1.0;
    double dx = (x1-x0)/(N-1);
    eil::Array<double> x_sample, y_sample;
    for (int i = 0; i < N; ++i) {
        double xx = x0 + dx*i;
        x_sample.append(xx);
        y_sample.append(runge(xx));
    }
    eil::nm::CubicSpline s(x_sample.all(), y_sample.all());
    N = 100;
    dx = (x1-x0)/(N-1);
    bool every_point_in_tol = true;
    double worst = 0.0;
    for (int i = 0; i < N; ++i) {
        double xx = x0 + dx*i;
        double y_runge = runge(xx);
        double y_spline = s(xx);
        double dy = y_spline - y_runge;
        // writefln("%g %g %g %g", xx, y_runge, y_spline, dy);   // commented in D
        if (!(std::fabs(dy) < 0.02)) every_point_in_tol = false;   // spline.d:118
        if (std::fabs(dy) > worst) worst = std::fabs(dy);
    }
    check(every_point_in_tol, "spline.d:94-120 |y_spline - y_runge| < 0.02 at 100 points");
    char note[160];
    std::snprintf(note, sizeof note,
                  "worst deviation %.17g equals D's 0.011722508490865535 "
                  "(measured with the same 20-knot/100-point loop)", worst);
    // D's own bound is 0.02 (spline.d:118); the equality is stronger and possible
    // because runge() needs no libm, so the whole loop is pure arithmetic.
    check(worst == 0.011722508490865535, note);
}

// ---- probe replay, mirroring probes/nm/probe_spline.d ----------------------
void dump_evals(const char* tag, const eil::nm::CubicSpline& s, int M, double gx0,
               double gx1) {
    double h = 1.0e-5;
    double gdx = (gx1-gx0)/(M-1);
    for (int i = 0; i < M; ++i) {
        double xx = gx0 + gdx*i;
        double y = s(xx);
        double d = (s(xx + h) - s(xx - h))/(2.0*h);
        emitf("%s %.17g y=%.17g err=%.17g d=%.17g", tag, xx, y, y - runge(xx), d);
    }
}

void replay_probe() {
    eil::Array<double> knot_x, knot_y;
    {   // build_sample(20, -1.0, 1.0)
        int N = 20;
        double x0 = -1.0, x1 = 1.0;
        double dx = (x1-x0)/(N-1);
        for (int i = 0; i < N; ++i) {
            double xx = x0 + dx*i;
            knot_x.append(xx);
            knot_y.append(runge(xx));
        }
    }
    eil::nm::CubicSpline s(knot_x.all(), knot_y.all());

    for (size_t i = 0; i < knot_x.length(); ++i) {
        emitf("knot %zu xs=%.17g ys=%.17g", i, s.xs[i], s.ys[i]);
    }
    emitf("bounds xmin=%.17g xmax=%.17g", s.xmin(), s.xmax());

    for (size_t i = 0; i < knot_x.length(); ++i) {
        emitf("at_knot %zu x=%.17g y=%.17g want=%.17g", i, knot_x[i], s(knot_x[i]),
              knot_y[i]);
    }
    for (size_t i = 0; i + 1 < knot_x.length(); ++i) {
        double xm = knot_x[i] + 0.5*(knot_x[i+1] - knot_x[i]);
        emitf("midpoint %zu x=%.17g y=%.17g", i, xm, s(xm));
    }
    dump_evals("runge20", s, 25, -1.2, 1.2);

    eil::nm::CubicSpline t(s);   // spline.d:63-66, the rebuild-from-knots copy
    for (int i = 0; i < 25; ++i) {
        double xx = -1.2 + 2.4*i/24;
        emitf("copy %.17g orig=%.17g dup=%.17g same=%s", xx, s(xx), t(xx),
              s(xx) == t(xx) ? "true" : "false");
    }
    knot_x[0] = 12345.0;
    knot_y[0] = -6789.0;
    emitf("after_caller_mutation xs0=%.17g ys0=%.17g y_at_xmin=%.17g", s.xs[0], s.ys[0],
          s(-1.0));

    eil::Array<double> xq({-1.0, 0.0, 0.3, 2.0});
    eil::Array<double> yq({2.0, -1.0, 0.5, 3.0});
    eil::nm::CubicSpline u(xq.all(), yq.all());
    for (int i = 0; i < 17; ++i) {
        double xx = -1.5 + 4.0*i/16.0;
        emitf("quad %.17g y=%.17g", xx, u(xx));
    }
    emitf("quad_bounds xmin=%.17g xmax=%.17g", u.xmin(), u.xmax());

    eil::Array<double> x2({0.0, 1.0, 3.0});
    eil::Array<double> y2({1.0, -2.0, 2.0});
    eil::nm::CubicSpline v(x2.all(), y2.all());
    for (int i = 0; i < 9; ++i) {
        double xx = -0.5 + 4.0*i/8.0;
        emitf("seg2 %.17g y=%.17g", xx, v(xx));
    }

    eil::Array<double> x1k({0.0, 2.0});
    eil::Array<double> y1k({1.0, 5.0});
    eil::nm::CubicSpline w(x1k.all(), y1k.all());
    for (int i = 0; i < 5; ++i) {
        double xx = -1.0 + 4.0*i/4.0;
        emitf("seg1 %.17g y=%.17g", xx, w(xx));
    }
    emitf("seg1_bounds xmin=%.17g xmax=%.17g", w.xmin(), w.xmax());

    // Repeated x makes h == 0.0, so the RHS of the tridiagonal system divides by
    // zero. D neither throws nor stops: the solve yields infinities that turn into
    // -nan, which is what the try/catch in the probe records.
    eil::Array<double> xc({0.0, 0.001, 0.5, 1.0, 1.0});
    eil::Array<double> yc({0.0, 0.5, -0.25, 0.75, 1.0});
    try {
        eil::nm::CubicSpline z(xc.all(), yc.all());
        for (int i = 0; i < 11; ++i) {
            double xx = -0.1 + 1.2*i/10.0;
            emitf("clustered %.17g y=%.17g", xx, z(xx));
        }
        emitf("clustered_at_end x=%.17g y=%.17g", xc[4], z(xc[4]));
    } catch (eil::ExceptionBase& e) {
        emitf("clustered threw Exception msg=%s", e.msg().c_str());
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: %s <probes/nm_spline.txt>\n", argv[0]);
        return 2;
    }
    unittest_spline();
    replay_probe();

    std::ifstream in(argv[1]);
    if (!in) {
        std::printf("cannot open corpus %s\n", argv[1]);
        return 2;
    }
    std::vector<std::string> expected;
    for (std::string line; std::getline(in, line);) expected.push_back(line);

    for (size_t i = 0; i < expected.size() || i < produced.size(); ++i) {
        ++compared;
        if (i >= expected.size() || i >= produced.size() || expected[i] != produced[i]) {
            if (differed < 20) {
                std::printf("line %zu\n  D:  |%s|\n  C++:|%s|\n", i + 1,
                            i < expected.size() ? expected[i].c_str() : "<none>",
                            i < produced.size() ? produced[i].c_str() : "<none>");
            }
            ++differed;
        }
    }
    std::printf("%s: nm.spline, %ld D lines vs %ld C++ lines, %ld differ, %ld assertion "
                "failures\n",
                (differed == 0 && failures == 0) ? "PASS" : "FAIL",
                static_cast<long>(expected.size()), static_cast<long>(produced.size()),
                static_cast<long>(differed), static_cast<long>(failures));
    return (differed == 0 && failures == 0) ? 0 : 1;
}
