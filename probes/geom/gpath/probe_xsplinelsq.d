// probes/geom/gpath/probe_xsplinelsq.d — what geom/gpath/xsplinelsq.d produces.
//
// XSplineLsq is a Path whose geometry is a *fitted* cubic spline y(x): the knot
// x-positions are supplied or spread evenly, and nm/nelmin's Nelder-Mead adjusts the
// knot y-values to minimise the weighted sum of squared errors (splinelsq.d:52-92).
// Three separate things can diverge and are pinned separately here:
//
//   1. the fit: the public knot/data arrays (spl.xd, spl.yd, spl.wd, spl.xs,
//      spl.nseg -- the same fields toString() prints) and the recovered knot
//      y-values. The underlying CubicSpline is `private` in splinelsq.d:122, but
//      the fitted spline interpolates its own knots, so spl(xs[i]) recovers the
//      i-th optimised y without reaching into a private field.
//   2. the t mapping: xsplinelsq.d:78-84, printed with x, t and y on one row.
//   3. the *optimiser's path*: the function-evaluation and restart counts. D's
//      CubicSplineLsq constructor keeps nfe/nres as locals and its four writeln
//      lines are commented out (splinelsq.d:88-91), so the counts are not
//      observable through the Path at all. They are measured here the way
//      probes/gasdyn/probe_idealgasflow.d measures its root-finder counts: a
//      faithful replica of the same call -- the same initial guess, the same
//      perturbation, tol=1.0e-6, P=2, nfe_max=1200 and a module-level `sse`
//      with the body of splinelsq.d:70-80 -- whose resulting knot values are then
//      compared against the real object's (rows "xsl.*.replica.matches"). The same
//      optimum in a different number of steps means the simplex diverged, so the
//      counts are part of the corpus.
//
// The copy path (xsplinelsq.d:68-71 -> splinelsq.d:95-103, which rebuilds
// `new CubicSpline(other.spl)`) is checked behaviourally with a sentinel write, as
// in probe_xspline.d.
//
// Build and capture (see probes/geom/gpath/probe_path.d for why the reference tree
// is copied with @nogc deleted):
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/gpath \
//     probes/geom/gpath/probe_xsplinelsq.d -of=/tmp/probe_xsl -od/tmp/probe_xsl_obj \
//     && /tmp/probe_xsl > baselines/e587b235/probes/geom_gpath_xsplinelsq.txt
//
// Run from the Eilmer repository root: the file rows read tests/test_data/*.dat.
import std.algorithm.searching: maxElement, minElement;
import std.conv;
import std.format;
import std.math;
import std.stdio;

import geom.elements;
import geom.gpath.path;
import geom.gpath.xsplinelsq;
import nm.number;
import nm.spline;
import nm.splinelsq;
import nm.nelmin;
import pathdump;

double runge(double x) {
    return 1.0 / (1.0 + 25 * x * x);
}

// ---------------------------------------------------------------------------
// The replica of splinelsq.d:70-80's nested sse, with module-level state playing
// the part of the closure's `this`. Bodies are copied statement for statement.
// ---------------------------------------------------------------------------
double[] g_xd, g_yd, g_wd, g_xs;
CubicSpline g_spl;

double sse(double[] ps) {
    // The parameter vector is the vector of y-values for the spline knots.
    assert(ps.length == g_xs.length, "Unexpected length for parameter vector.");
    g_spl = new CubicSpline(g_xs, ps);
    double sumerr = 0.0;
    foreach (j; 0 .. g_xd.length) {
        double e = g_yd[j] - g_spl(g_xd[j]);
        sumerr += g_wd[j] * e * e;
    }
    return sumerr;
}

/// splinelsq.d:33-87 verbatim, with the counts printed and the resulting knot
/// values compared against the real object's own interpolation of them.
void replicate_fit(const char[] tag, const double[] xd, const double[] yd, const double[] wd,
                   const double[] xs_in, int nseg, const XSplineLsq obj)
{
    g_xd = xd.dup();
    size_t nd = xd.length;
    g_yd = yd.dup();
    g_wd = wd.dup();
    g_xs = xs_in.dup();
    double[] ys;
    if (g_xs.length == 0) {
        foreach (i; 0 .. nseg + 1) {
            double frac = to!double(i) / nseg;
            g_xs ~= xd[0] * (1.0 - frac) + xd[$ - 1] * frac;
            ys ~= yd[0] * (1.0 - frac) + yd[$ - 1] * frac;
        }
    } else {
        foreach (i; 0 .. nseg + 1) {
            double frac = (g_xs[i] - xd[0]) / (xd[$ - 1] - xd[0]);
            ys ~= yd[0] * (1.0 - frac) + yd[$ - 1] * frac;
        }
    }
    writefln("%s initial.length=%d", tag, ys.length);
    foreach (i; 0 .. ys.length) {
        writefln("%s initial[%d] x=%.17g y=%.17g", tag, i, g_xs[i], ys[i]);
    }
    double dy = maxElement(yd) - minElement(yd);
    dy *= 0.1;
    writefln("%s dy=%.17g", tag, dy);
    double[] delys;
    foreach (i; 0 .. nseg + 1) { delys ~= dy; }
    double f;
    int nfe, nres;
    double tol = 1.0e-6;
    int P = 2;
    int nfe_max = 1200;
    bool conv_flag = minimize!(sse, double)(ys, f, nfe, nres, delys, tol, P, nfe_max);
    writefln("%s counts converged=%s nfe=%d nres=%d f=%.17g", tag, conv_flag, nfe, nres, f);
    foreach (i; 0 .. ys.length) {
        // obj(g_xs[i]) is the fitted spline read back through XSplineLsq itself, so
        // a replica that drifted from the real constructor shows as a diff here.
        double ov = obj.spl(g_xs[i]);
        writefln("%s final[%d] y=%.17g obj=%.17g diff=%.17g", tag, i, ys[i], ov, ov - ys[i]);
    }
}

void dump_state(const char[] tag, const XSplineLsq s, int stride = 1)
{
    writefln("%s class=%s", tag, s.classString());
    // toString() lists every data point, so the 200-point fits print its length
    // and prefix only; the small cases print the whole text (that text is what a
    // log line would carry, so it must match byte for byte).
    string ts = s.toString();
    if (ts.length <= 600) {
        writefln("%s str=%s", tag, ts);
    } else {
        writefln("%s strlen=%d strhead=%s", tag, ts.length, ts[0 .. 60]);
    }
    writefln("%s nseg=%d ndata=%d nknots=%d", tag, s.spl.nseg, s.spl.xd.length,
             s.spl.xs.length);
    // The data arrays are printed at a stride (the 200-point Runge set is the
    // same samples the XSpline probe already prints); the first, last and every
    // stride-th entry are enough to pin the dup'd copies.
    foreach (i; 0 .. s.spl.xd.length) {
        if ((i % stride != 0) && (i != s.spl.xd.length - 1)) continue;
        writefln("%s data[%d] x=%.17g y=%.17g w=%.17g", tag, i, s.spl.xd[i], s.spl.yd[i],
                 s.spl.wd[i]);
    }
    foreach (i; 0 .. s.spl.xs.length) {
        writefln("%s knot[%d] x=%.17g yfit=%.17g", tag, i, s.spl.xs[i], s.spl(s.spl.xs[i]));
    }
    writefln("%s xmin=%.17g xmax=%.17g", tag, s.spl.xmin(), s.spl.xmax());
}

/// The D unittest's own loop (xsplinelsq.d:129-141), printed the way D prints it
/// and then again at %.17g under key=value names so the replay can pair the
/// numeric fields positionally.
void dump_unittest_rows(const char[] tag, const XSplineLsq s)
{
    int N = 100;
    double max_dy = 0.0;
    double dt = 1.0 / (N - 1);
    foreach (i; 0 .. N) {
        double t = dt * i;
        Vector3 p = s(t);
        double y_runge = runge(p.x);
        double dy = p.y - y_runge;
        max_dy = fmax(max_dy, fabs(dy));
        writefln("%s %g %g %g %g px=%.17g yr=%.17g py=%.17g dy=%.17g ok=%s", tag, p.x, y_runge,
                 p.y, dy, p.x, y_runge, p.y, dy, fabs(dy) < 0.02);
    }
    writefln("%s max_dy=%s", tag, max_dy);
}

void main()
{
    // xsplinelsq.d:114-141: 200 Runge samples, ends weighted 100, 10 segments,
    // knots left to the even distribution (splinelsq.d:59-63).
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
    auto s = new XSplineLsq(x_sample, y_sample, w_sample, xs, 10);
    dump_state("xsl.runge", s);
    dump_unittest_rows("xsl.runge", s);
    dump_trows("xsl.runge", s);
    dump_path("xsl.runge", s);
    replicate_fit("xsl.runge", x_sample, y_sample, w_sample, xs, 10, s);

    // A supplied knot vector reaches splinelsq.d:60-64 instead.
    double[] xsup = [-1.0, -0.5, 0.0, 0.5, 1.0];
    auto s2 = new XSplineLsq(x_sample, y_sample, w_sample, xsup, 4);
    dump_state("xsl.knots", s2, 20);
    dump_trows("xsl.knots", s2);
    replicate_fit("xsl.knots", x_sample, y_sample, w_sample, xsup, 4, s2);

    // The file constructor (xsplinelsq.d:35-66) over the weighted triples, with
    // both the supplied-knot and the generated-knot branches.
    auto s3 = new XSplineLsq("tests/test_data/gpath_weights.dat", xs, 5);
    dump_state("xsl.file", s3);
    auto s4 = new XSplineLsq("tests/test_data/gpath_weights.dat", xsup, 4);
    dump_state("xsl.file.knots", s4);
    // The caller's xs array is a D view; splinelsq.d's parameter resize does not
    // reach it. Recorded so the C++ port's local-array choice is measured, not
    // assumed.
    writefln("xsl.file caller.xs.length=%d caller.xs0=%.17g", xs.length,
             (xs.length > 0) ? xs[0] : 0.0);
    writefln("xsl.file caller.xsup.length=%d", xsup.length);

    // Uniform weights: wd empty means "all 1.0" (splinelsq.d:39-41), reached by
    // the two-column file.
    auto s5 = new XSplineLsq("tests/test_data/gpath_xspline.dat", xs, 4);
    writefln("xsl.noweight nseg=%d ndata=%d w0=%.17g str=%s", s5.spl.nseg, s5.spl.xd.length,
             s5.spl.wd[0], s5.toString());  // 17 points: the full text fits

    // Copy constructor (xsplinelsq.d:68-71) and dup (xsplinelsq.d:73-76).
    auto sc = new XSplineLsq(s);
    dump_state("xsl.copy", sc, 20);
    auto sd = cast(XSplineLsq) s.dup();
    string dups = sd.toString();
    writefln("xsl.dup class=%s strlen=%d strhead=%s", sd.classString(), dups.length,
             dups[0 .. 60]);
    sc.spl.xd[0] = -987.654321;
    sc.spl.xs[1] = -123.456;
    writefln("xsl.aliaschk copy.xd0=%.17g orig.xd0=%.17g copy.xs1=%.17g orig.xs1=%.17g",
             sc.spl.xd[0], s.spl.xd[0], sc.spl.xs[1], s.spl.xs[1]);
    writefln("xsl.aliaschk copy.eval.at.knot1=%.17g orig.eval.at.knot1=%.17g", sc.spl(s.spl.xs[1]),
             s.spl(s.spl.xs[1]));

    // Error paths: nseg <= 2 (xsplinelsq.d:53), a wrong knot count (xsplinelsq.d:56),
    // and the too-few-data check inside the fit (splinelsq.d:35). Only the
    // classification is printed for the asserts, whose D message carries the
    // reference tree's file and line; the enforce-style messages would print in
    // full but there are none in this module.
    write("xsl.err.nseg2.class=");
    try { auto e1 = new XSplineLsq("tests/test_data/gpath_weights.dat", xs, 2); writeln("none"); }
    catch (Error e) { writeln("Error"); } catch (Exception e) { writeln("Exception"); }
    write("xsl.err.nsegknots.class=");
    try { auto e2 = new XSplineLsq("tests/test_data/gpath_weights.dat", xsup, 8); writeln("none"); }
    catch (Error e) { writeln("Error"); } catch (Exception e) { writeln("Exception"); }
    write("xsl.err.empty.class=");
    try { auto e3 = new XSplineLsq("tests/test_data/gpath_empty.dat", xs, 3); writeln("none"); }
    catch (Error e) { writeln("Error"); } catch (Exception e) { writeln("Exception"); }
    write("xsl.err.toofew.class=");
    try {
        auto e4 = new XSplineLsq([0.0, 1.0, 2.0], [0.0, 1.0, 2.0], [1.0, 1.0, 1.0], xs, 2 + 1);
        writeln("none");
    } catch (Error e) { writeln("Error"); } catch (Exception e) { writeln("Exception"); }
}

/// The t mapping rows: x, t and y on one line, at the shared sweep and at even
/// divisions, plus out-of-range extrapolation on both sides.
void dump_trows(const char[] tag, const XSplineLsq s)
{
    foreach (t; t_sweep()) {
        auto p = s(t);
        writefln("%s tmap t=%.17g x=%.17g y=%.17g z=%.17g", tag, t, p.x, p.y, p.z);
    }
    int N = 41;
    double dt = 1.1 / (N - 1) - 0.1 / (N - 1);
    foreach (i; 0 .. N) {
        double t = -0.05 + (1.1 * i) / cast(double)(N - 1);
        auto p = s(t);
        writefln("%s wide t=%.17g x=%.17g y=%.17g", tag, t, p.x, p.y);
    }
    foreach (x; [s.spl.xmin() - 0.4, s.spl.xmax() + 0.4]) {
        writefln("%s oor x=%.17g y=%.17g", tag, x, s.spl(x));
    }
}
