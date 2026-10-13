// probes/nm/probe_spline.d — pin what ldc2's nm/spline.d actually produces.
//
// Why this probe exists: geom/gpath/{xspline,xsplinelsq}.d and
// geom/surface/beziertrianglepatch.d build on CubicSpline, and the ported class
// has to hand back the same doubles from opCall(). CubicSpline stores a, b, c
// privately and exposes no derivative, so the observables are the interpolated
// values themselves (dumped at %.17g), the stored knots, xmin()/xmax(), and a
// central-difference derivative estimate of opCall — which is sensitive to the
// segment search in spline.d:68-82 and to the coefficient arithmetic in the
// constructor.
//
// NOTE ABOUT THE REFERENCE BUILD. nm/spline.d does not compile in the gdtk tree
// at e587b235: it calls nm.bbla.gaussJordanElimination, which is declared @nogc
// but throws `new Exception` (bbla.d:705-711), so ldc2 1.40 rejects the
// instantiation and spline.d:51 fails to build. The capture below therefore
// compiles against a copy of gdtk/src in /tmp with that ONE attribute deleted
// (no statement changed; @nogc has no numeric effect). The command is:
//
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   python3 -c "p='/tmp/gdtk_shadow/nm/bbla.d';s=open(p).read();\
//     open(p,'w').write(s.replace('@nogc\nvoid gaussJordanElimination','void gaussJordanElimination'))"
//   ldc2 -O0 -i -I/tmp/gdtk_shadow probes/nm/probe_spline.d -of=/tmp/spline_d \
//     && /tmp/spline_d > baselines/e587b235/probes/nm_spline.txt
// Replay check: ctest -R nm_spline
import std.stdio, std.math, std.conv;
import nm.spline;

double runge(double x) { return 1.0/(1.0 + 25* x * x); }

// The dataset from spline.d:99-108 (unittest): 20 uniform Runge points.
double[] knot_x, knot_y;

void build_sample(int N, double x0, double x1) {
    double dx = (x1-x0)/(N-1);
    foreach (i; 0 .. N) {
        double xx = x0 + dx*i;
        knot_x ~= xx;
        knot_y ~= runge(xx);
    }
}

void dump_evals(string tag, CubicSpline s, int M, double gx0, double gx1) {
    double h = 1.0e-5;
    double gdx = (gx1-gx0)/(M-1);
    foreach (i; 0 .. M) {
        double xx = gx0 + gdx*i;
        double y = s(xx);
        // central difference through opCall; spline.d has no derivative method
        double d = (s(xx + h) - s(xx - h))/(2.0*h);
        writefln("%s %.17g y=%.17g err=%.17g d=%.17g", tag, xx, y, y - runge(xx), d);
    }
}

void main() {
    build_sample(20, -1.0, 1.0);
    auto s = new CubicSpline(knot_x, knot_y);

    // stored knots (public xs/ys, spline.d:20) -- must equal the inputs, and
    // must be copies (mutating the caller's arrays may not reach the spline).
    foreach (i; 0 .. knot_x.length) {
        writefln("knot %d xs=%.17g ys=%.17g", i, s.xs[i], s.ys[i]);
    }
    writefln("bounds xmin=%.17g xmax=%.17g", s.xmin(), s.xmax());

    // Exact knot evaluations: pins the interpolation property and the segment
    // search at the boundaries (x == xs[0] and x == xs[$-1] included).
    foreach (i; 0 .. knot_x.length) {
        writefln("at_knot %d x=%.17g y=%.17g want=%.17g", i, knot_x[i], s(knot_x[i]),
                 knot_y[i]);
    }
    // Half-step points: the segment index changes at every knot, so these catch
    // an off-by-one in `while ((x < xs[i]) && (i > 0)) i -= 1;`.
    foreach (i; 0 .. knot_x.length - 1) {
        double xm = knot_x[i] + 0.5*(knot_x[i+1] - knot_x[i]);
        writefln("midpoint %d x=%.17g y=%.17g", i, xm, s(xm));
    }
    // Out of range, both sides (spline.d:73-77 extrapolates with the end piece).
    dump_evals("runge20", s, 25, -1.2, 1.2);

    // Copy constructor: this(ref const(CubicSpline)) at spline.d:63-66 re-runs
    // the whole construction from other.xs/other.ys.
    auto t = new CubicSpline(s);
    foreach (i; 0 .. 25) {
        double xx = -1.2 + 2.4*i/24;
        writefln("copy %.17g orig=%.17g dup=%.17g same=%s", xx, s(xx), t(xx), s(xx)==t(xx));
    }
    // Mutating the caller's arrays must not reach the spline (`.dup()` at
    // spline.d:31-32).
    knot_x[0] = 12345.0;
    knot_y[0] = -6789.0;
    writefln("after_caller_mutation xs0=%.17g ys0=%.17g y_at_xmin=%.17g", s.xs[0], s.ys[0],
             s(-1.0));

    // A small non-uniform dataset: n=3 segments, so the tridiagonal system has
    // m=2 unknowns and both the `k > 0` and `k < m-1` branches are exercised.
    double[] xq = [-1.0, 0.0, 0.3, 2.0];
    double[] yq = [2.0, -1.0, 0.5, 3.0];
    auto u = new CubicSpline(xq, yq);
    foreach (i; 0 .. 17) {
        double xx = -1.5 + 4.0*i/16.0;
        writefln("quad %.17g y=%.17g", xx, u(xx));
    }
    writefln("quad_bounds xmin=%.17g xmax=%.17g", u.xmin(), u.xmax());

    // n=2 segments (m=1 unknown): the smallest system that still solves.
    double[] x2 = [0.0, 1.0, 3.0];
    double[] y2 = [1.0, -2.0, 2.0];
    auto v = new CubicSpline(x2, y2);
    foreach (i; 0 .. 9) {
        double xx = -0.5 + 4.0*i/8.0;
        writefln("seg2 %.17g y=%.17g", xx, v(xx));
    }

    // A single-segment spline: n=1, m=0, so the augmented matrix has zero rows
    // and the natural end conditions leave sigma all zeros.
    double[] x1k = [0.0, 2.0];
    double[] y1k = [1.0, 5.0];
    auto w = new CubicSpline(x1k, y1k);
    foreach (i; 0 .. 5) {
        double xx = -1.0 + 4.0*i/4.0;
        writefln("seg1 %.17g y=%.17g", xx, w(xx));
    }
    writefln("seg1_bounds xmin=%.17g xmax=%.17g", w.xmin(), w.xmax());

    // Clustered knots, the shape a grid-generation path hands these classes.
    // The last pair is repeated, so h[3] == 0.0 and the right-hand side of the
    // tridiagonal system divides by zero (spline.d:49). Whether that throws or
    // just produces non-finite coefficients is decided by measurement, and it is
    // caught here so both D and the port print the same single line.
    double[] xc = [0.0, 0.001, 0.5, 1.0, 1.0];
    double[] yc = [0.0, 0.5, -0.25, 0.75, 1.0];
    try {
        auto z = new CubicSpline(xc, yc);
        foreach (i; 0 .. 11) {
            double xx = -0.1 + 1.2*i/10.0;
            writefln("clustered %.17g y=%.17g", xx, z(xx));
        }
        writefln("clustered_at_end x=%.17g y=%.17g", xc[4], z(xc[4]));
    } catch (Exception e) {
        writefln("clustered threw Exception msg=%s", e.msg);
    }
}
