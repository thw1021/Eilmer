// probes/nm/probe_splinelsq.d — pin what ldc2's nm/splinelsq.d actually produces.
//
// Why this probe exists: CubicSplineLsq is a least-squares spline fit whose only
// observable output is the underlying CubicSpline it hands to nm.nelmin.minimize
// (splinelsq.d:87). The fit consumes the whole Nelder-Mead machinery, so this
// probe is simultaneously a nelmin integration test: a single differing bit in
// the simplex stepping lands on a different local minimum and every printed
// value moves. geom/gpath/xsplinelsq.d holds one of these, so the ported class
// has to reproduce them at %.17g.
//
// Build and capture (same one-attribute bbla.d fix as probes/nm/probe_spline.d;
// see that header for why the gdtk tree itself does not compile spline.d):
//
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   python3 -c "p='/tmp/gdtk_shadow/nm/bbla.d';s=open(p).read();\
//     open(p,'w').write(s.replace('@nogc\nvoid gaussJordanElimination','void gaussJordanElimination'))"
//   ldc2 -O0 -i -I/tmp/gdtk_shadow probes/nm/probe_splinelsq.d -of=/tmp/splsq_d \
//     && /tmp/splsq_d > baselines/e587b235/probes/nm_splinelsq.txt
// Replay check: ctest -R nm_splinelsq
import std.stdio, std.math, std.conv;
import nm.splinelsq;

double runge(double x) {
    return 1.0 / (1.0 + 25 * x * x);
}

void dump_state(string tag, CubicSplineLsq s) {
    writefln("%s nseg=%d nd=%d nx=%s=%s", tag, s.nseg, s.xd.length, "xmin", s.xmin());
    writefln("%s xmax=%.17g", tag, s.xmax());
    foreach (i; 0 .. s.xs.length) {
        writefln("%s knot %d x=%.17g", tag, i, s.xs[i]);
    }
    writefln("%s d0 xd=%.17g yd=%.17g wd=%.17g", tag, s.xd[0], s.yd[0], s.wd[0]);
    writefln("%s dL xd=%.17g yd=%.17g wd=%.17g", tag, s.xd[$-1], s.yd[$-1], s.wd[$-1]);
}

void dump_evals(string tag, CubicSplineLsq s, int M) {
    double x0 = -1.0, x1 = 1.0;
    double dx = (x1 - x0)/(M - 1);
    double h = 1.0e-5;
    foreach (i; 0 .. M) {
        double xx = x0 + dx*i;
        double y = s(xx);
        // central difference through opCall (splinelsq.d exposes no derivative)
        double d = (s(xx + h) - s(xx - h))/(2.0*h);
        writefln("%s %.17g y=%.17g err=%.17g d=%.17g", tag, xx, y, y - runge(xx), d);
    }
}

void main() {
    // ---- the unittest dataset: splinelsq.d:133-147 -------------------------
    int N = 200;
    double x0 = -1.0;
    double x1 = 1.0;
    double dx = (x1 - x0) / (N - 1);
    double[] x_sample, y_sample, w_sample;
    foreach (i; 0 .. N) {
        double xx = x0 + dx * i;
        x_sample ~= xx;
        y_sample ~= runge(xx);
        w_sample ~= 1.0;
    }
    w_sample[0] = 100.0;
    w_sample[$ - 1] = 100.0;
    double[] xs;
    auto s = new CubicSplineLsq(x_sample, y_sample, w_sample, xs, 10);

    // The exact text splinelsq.d:157 prints in its unittest block, in the same
    // order, so the ported test can be diffed against D's own words.
    N = 100;
    dx = (x1 - x0) / (N - 1);
    double max_dy = 0.0;
    foreach (i; 0 .. N) {
        double xx = x0 + dx * i;
        double y_runge = runge(xx);
        double y_spline = s(xx);
        double dy = y_spline - y_runge;
        max_dy = fmax(max_dy, fabs(dy));
        writefln("%g %g %g %g", xx, y_runge, y_spline, dy);
    }
    writeln("max_dy=", max_dy);
    writefln("unittest_assertions %s %s", max_dy < 0.02, true);

    dump_state("ut", s);
    dump_evals("ut_eval", s, 41);

    // ---- copy constructor: splinelsq.d:95-103 -----------------------------
    auto c = new CubicSplineLsq(s);
    foreach (i; 0 .. 41) {
        double xx = -1.0 + 2.0*i/40.0;
        writefln("copy %.17g orig=%.17g dup=%.17g same=%s", xx, s(xx), c(xx), s(xx)==c(xx));
    }
    dump_state("copy", c);

    // ---- empty weights (the wd.length == 0 branch, splinelsq.d:39-41) ------
    double[] w_empty;
    auto s2 = new CubicSplineLsq(x_sample, y_sample, w_empty, xs, 10);
    dump_state("noweight", s2);
    dump_evals("noweight_eval", s2, 21);

    // ---- caller-supplied knot positions (the xs.length > 0 branch) ---------
    // splinelsq.d:45-47 requires xs.length == nseg+1; splinelsq.d:60-65 then
    // builds the straight-line initial guess from the supplied knots.
    double[] xs_in = [-1.0, -0.75, -0.2, 0.0, 0.35, 1.0];
    auto s3 = new CubicSplineLsq(x_sample, y_sample, w_sample, xs_in, 5);
    dump_state("fixed_knots", s3);
    dump_evals("fixed_knots_eval", s3, 21);

    // ---- a small dataset, few segments -------------------------------------
    double[] sx, sy, sw;
    foreach (i; 0 .. 12) {
        double xx = -1.0 + 2.0*i/11.0;
        sx ~= xx;
        sy ~= 1.0/(1.0 + 4.0*xx*xx) + 0.05*cos(3.0*xx);
        sw ~= 1.0;
    }
    double[] xs_small;
    auto s4 = new CubicSplineLsq(sx, sy, sw, xs_small, 3);
    dump_state("small", s4);
    foreach (i; 0 .. 17) {
        double xx = -1.1 + 2.2*i/16.0;
        writefln("small_eval %.17g y=%.17g", xx, s4(xx));
    }

    // ---- one heavily weighted interior point -------------------------------
    double[] w_peak;
    foreach (i; 0 .. x_sample.length) {
        w_peak ~= (i == 100) ? 1000.0 : 1.0;
    }
    auto s5 = new CubicSplineLsq(x_sample, y_sample, w_peak, xs, 8);
    dump_state("peak_weight", s5);
    dump_evals("peak_weight_eval", s5, 21);
}
