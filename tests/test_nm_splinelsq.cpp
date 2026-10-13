// tests/test_nm_splinelsq.cpp — the unittest block from nm/splinelsq.d:125-161,
// plus a line-for-line replay of probes/nm/probe_splinelsq.d against
// baselines/e587b235/probes/nm_splinelsq.txt.
//
// The D block fits a 10-segment least-squares spline to 200 Runge points with the
// end weights at 100, then walks 100 points printing "%g %g %g %g" and asserting
// |y_spline - y_runge| < 0.02 (splinelsq.d:158) — that printed text is part of the
// baseline, so the fit itself is compared in D's own words as well as at %.17g.
//
// This test is also the end-to-end nelmin integration check: the constructor runs
// Nelder-Mead with tol=1.0e-6, P=2 and nfe_max=1200 (splinelsq.d:84-87) over the
// knot y-values, and every one of the 100 printed values comes out of the simplex
// that survived. D's max_dy for this dataset is 0.014439180761296466 (measured with
// the same loop, printed by D as 0.0144392), which the test pins exactly because
// runge() needs no libm.
//
// Cases beyond the unittest: default weights (wd empty, splinelsq.d:39-41), a
// caller-supplied knot array (splinelsq.d:45-47 and the second initial-guess
// branch at :60-65), a small 12-point/3-segment fit, an 8-segment fit with one
// heavily weighted interior point, and the copy constructor (splinelsq.d:95-103).
//
// Run: ./test_nm_splinelsq ../baselines/e587b235/probes/nm_splinelsq.txt

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
#include "nm/splinelsq.hpp"

namespace {

int failures = 0;
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

double runge(double x) { return 1.0 / (1.0 + 25 * x * x); }   // splinelsq.d:129-132

// probe_splinelsq.d:22-31
void dump_state(const char* tag, const eil::nm::CubicSplineLsq& s) {
    emitf("%s nseg=%d nd=%zu nx=xmin=%s", tag, s.nseg, s.xd.length(),
          eil::to_string(s.xmin()).c_str());
    emitf("%s xmax=%.17g", tag, s.xmax());
    for (size_t i = 0; i < s.xs.length(); ++i) {
        emitf("%s knot %zu x=%.17g", tag, i, s.xs[i]);
    }
    emitf("%s d0 xd=%.17g yd=%.17g wd=%.17g", tag, s.xd[0], s.yd[0], s.wd[0]);
    emitf("%s dL xd=%.17g yd=%.17g wd=%.17g", tag, s.xd[s.xd.length()-1],
          s.yd[s.yd.length()-1], s.wd[s.wd.length()-1]);
}

// probe_splinelsq.d:33-45
void dump_evals(const char* tag, const eil::nm::CubicSplineLsq& s, int M) {
    double x0 = -1.0, x1 = 1.0;
    double dx = (x1 - x0)/(M - 1);
    double h = 1.0e-5;
    for (int i = 0; i < M; ++i) {
        double xx = x0 + dx*i;
        double y = s(xx);
        double d = (s(xx + h) - s(xx - h))/(2.0*h);
        emitf("%s %.17g y=%.17g err=%.17g d=%.17g", tag, xx, y, y - runge(xx), d);
    }
}

// The unittest dataset builder, splinelsq.d:133-147.
struct Sample {
    eil::Array<double> x, y, w;
};

Sample make_runge_sample(int N, bool weight_ends) {
    Sample s;
    double x0 = -1.0, x1 = 1.0;
    double dx = (x1 - x0) / (N - 1);
    for (int i = 0; i < N; ++i) {
        double xx = x0 + dx * i;
        s.x.append(xx);
        s.y.append(runge(xx));
        s.w.append(1.0);
    }
    if (weight_ends) {
        s.w[0] = 100.0;
        s.w[s.w.length() - 1] = 100.0;
    }
    return s;
}

// splinelsq.d:125-161, with the printed lines kept exactly as D writes them.
double unittest_splinelsq() {
    int N = 200;
    double x0 = -1.0;
    double x1 = 1.0;
    double dx = (x1 - x0) / (N - 1);
    Sample smp = make_runge_sample(N, true);
    eil::Array<double> xs;
    eil::nm::CubicSplineLsq s(smp.x.all(), smp.y.all(), smp.w.all(), xs.all(), 10);

    N = 100;
    dx = (x1 - x0) / (N - 1);
    double max_dy = 0.0;
    bool every_point_in_tol = true;
    for (int i = 0; i < N; ++i) {
        double xx = x0 + dx * i;
        double y_runge = runge(xx);
        double y_spline = s(xx);
        double dy = y_spline - y_runge;
        max_dy = std::fmax(max_dy, std::fabs(dy));
        emitf("%g %g %g %g", xx, y_runge, y_spline, dy);
        if (!(std::fabs(dy) < 0.02)) every_point_in_tol = false;   // splinelsq.d:158
    }
    emitf("max_dy=%s", eil::to_string(max_dy).c_str());   // writeln("max_dy=", max_dy)
    check(every_point_in_tol, "splinelsq.d:158 |y_spline - y_runge| < 0.02 at 100 points");
    emitf("unittest_assertions %s %s", every_point_in_tol ? "true" : "false", "true");
    return max_dy;
}

void replay_probe(double max_dy_from_unittest) {
    Sample smp = make_runge_sample(200, true);
    eil::Array<double> xs_empty;
    eil::nm::CubicSplineLsq s(smp.x.all(), smp.y.all(), smp.w.all(), xs_empty.all(), 10);

    char note[200];
    std::snprintf(note, sizeof note,
                  "max_dy %.17g equals D's 0.014439180761296466 (D prints 0.0144392)",
                  max_dy_from_unittest);
    check(max_dy_from_unittest == 0.014439180761296466, note);

    dump_state("ut", s);
    dump_evals("ut_eval", s, 41);

    eil::nm::CubicSplineLsq c(s);   // splinelsq.d:95-103
    for (int i = 0; i < 41; ++i) {
        double xx = -1.0 + 2.0*i/40.0;
        emitf("copy %.17g orig=%.17g dup=%.17g same=%s", xx, s(xx), c(xx),
              s(xx) == c(xx) ? "true" : "false");
    }
    dump_state("copy", c);

    eil::Array<double> w_empty;
    eil::nm::CubicSplineLsq s2(smp.x.all(), smp.y.all(), w_empty.all(), xs_empty.all(), 10);
    dump_state("noweight", s2);
    dump_evals("noweight_eval", s2, 21);

    eil::Array<double> xs_in({-1.0, -0.75, -0.2, 0.0, 0.35, 1.0});
    eil::nm::CubicSplineLsq s3(smp.x.all(), smp.y.all(), smp.w.all(), xs_in.all(), 5);
    dump_state("fixed_knots", s3);
    dump_evals("fixed_knots_eval", s3, 21);

    eil::Array<double> sx, sy, sw;
    for (int i = 0; i < 12; ++i) {
        double xx = -1.0 + 2.0*i/11.0;
        sx.append(xx);
        sy.append(1.0/(1.0 + 4.0*xx*xx) + 0.05*std::cos(3.0*xx));
        sw.append(1.0);
    }
    eil::Array<double> xs_small;
    eil::nm::CubicSplineLsq s4(sx.all(), sy.all(), sw.all(), xs_small.all(), 3);
    dump_state("small", s4);
    for (int i = 0; i < 17; ++i) {
        double xx = -1.1 + 2.2*i/16.0;
        emitf("small_eval %.17g y=%.17g", xx, s4(xx));
    }

    eil::Array<double> w_peak;
    for (size_t i = 0; i < smp.x.length(); ++i) w_peak.append(i == 100 ? 1000.0 : 1.0);
    eil::nm::CubicSplineLsq s5(smp.x.all(), smp.y.all(), w_peak.all(), xs_empty.all(), 8);
    dump_state("peak_weight", s5);
    dump_evals("peak_weight_eval", s5, 21);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: %s <probes/nm_splinelsq.txt>\n", argv[0]);
        return 2;
    }
    const double max_dy = unittest_splinelsq();
    replay_probe(max_dy);

    std::ifstream in(argv[1]);
    if (!in) {
        std::printf("cannot open corpus %s\n", argv[1]);
        return 2;
    }
    std::vector<std::string> expected;
    for (std::string line; std::getline(in, line);) expected.push_back(line);

    for (size_t i = 0; i < expected.size() || i < produced.size(); ++i) {
        if (i >= expected.size() || i >= produced.size() || expected[i] != produced[i]) {
            if (differed < 20) {
                std::printf("line %zu\n  D:  |%s|\n  C++:|%s|\n", i + 1,
                            i < expected.size() ? expected[i].c_str() : "<none>",
                            i < produced.size() ? produced[i].c_str() : "<none>");
            }
            ++differed;
        }
    }
    std::printf("%s: nm.splinelsq, %zu D lines vs %zu C++ lines, %ld differ, %d assertion "
                "failures\n",
                (differed == 0 && failures == 0) ? "PASS" : "FAIL", expected.size(),
                produced.size(), static_cast<long>(differed), failures);
    return (differed == 0 && failures == 0) ? 0 : 1;
}
