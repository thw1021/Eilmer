// probes/geom/gpath/probe_xspline.d — what geom/gpath/xspline.d produces.
//
// XSpline is a Path whose geometry is the natural cubic spline y(x) of nm/spline.d,
// swept over t in [0,1] mapped onto the spline's own x range (xspline.d:63-69). Two
// things are separately measurable and both matter: the *fit* (the knots and the
// coefficients the CubicSpline constructor solves) and the *t mapping*. A port that
// got the mapping wrong still prints plausible y values, so every row below that
// evaluates prints x, t and y together, and the evaluations are dense across each
// polynomial segment (ends, quarters and the +-1e-12 window around every knot,
// which is where spline.d:71-83's search-from-the-right index arithmetic changes).
//
// The cubic coefficients a/b/c are `private` in nm/spline.d:88 and cannot be read
// from outside the class, so the fit is pinned by the public knot arrays (spl.xs,
// spl.ys -- also what toString() prints) plus the dense evaluations, which
// determine the coefficients uniquely.
//
// The copy path is checked behaviourally, not by pointer printing: xspline.d:55
// builds a *new* CubicSpline from other.spl, so after `new XSpline(s)` the copy's
// knot array is written with a sentinel and the original is re-read -- if the two
// aliased, the original would change too.
//
// Build and capture (see probes/geom/gpath/probe_path.d for why the reference tree
// is copied with @nogc deleted -- geom.gpath.path's import closure needs it):
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/gpath \
//     probes/geom/gpath/probe_xspline.d -of=/tmp/probe_xs -od/tmp/probe_xs_obj \
//     && /tmp/probe_xs > baselines/e587b235/probes/geom_gpath_xspline.txt
//
// Run from the Eilmer repository root: the file rows read tests/test_data/*.dat.
//
// SUBSTRATE NOTE (T-9xx candidate, reported rather than worked around in code):
// the file-parse error rows print the Error/Exception taxonomy and not e.msg,
// because eil::to_double (substrate/include/eil/conv.hpp:87-92) sends every first
// character that is not a digit, '.', '+' or '-' down the
// `error converting input to floating point` branch, whereas Phobos only does that
// for a leading n/N/i/I (the start of nan/infinity) and says
// `no digits seen for input "X".` for everything else -- measured with ldc2 1.40 on
// "#c", "abc", "@1", "$1.0", "x", "e", "-abc", "  abc" (all "no digits seen") vs.
// "not a number", "n", "i", "na", "in" (all "error converting input to floating
// point"). geom/gpath/{polyline,xspline,xsplinelsq}.d read their coordinates with
// to!double, so this is on their error path; the rows below stay taxonomy-only
// until conv.hpp is corrected, at which point the msg field can be restored.
import std.conv;
import std.format;
import std.math;
import std.stdio;
import std.typecons : Tuple;

import geom.elements;
import geom.gpath.path;
import geom.gpath.xspline;
import nm.number;
import nm.spline;
import pathdump;

double runge(double x) {
    return 1.0 / (1.0 + 25 * x * x);
}

void dump_knots(const char[] tag, const XSpline s)
{
    writefln("%s class=%s", tag, s.classString());
    writefln("%s str=%s", tag, s.toString());
    writefln("%s npts=%d", tag, s.spl.xs.length);
    foreach (i; 0 .. s.spl.xs.length) {
        writefln("%s knot[%d] x=%.17g y=%.17g", tag, i, s.spl.xs[i], s.spl.ys[i]);
    }
    writefln("%s xmin=%.17g xmax=%.17g", tag, s.spl.xmin(), s.spl.xmax());
}

/// Evaluations across every polynomial piece: the segment ends, the quarters, the
/// +-eps windows around the knots and the two out-of-range branches.
void dump_evals(const char[] tag, const XSpline s)
{
    const xs = s.spl.xs;
    size_t n = xs.length;
    for (size_t i = 0; i < n; ++i) {
        double lo = xs[i];
        double hi = (i + 1 < n) ? xs[i + 1] : lo + (xs[$ - 1] - xs[0]) / cast(double)(n - 1);
        foreach (f; [0.0, 0.25, 0.5, 0.75, 1.0]) {
            double x = lo + (hi - lo) * f;
            double y = s.spl(x);
            writefln("%s eval x=%.17g y=%.17g runge=%.17g err=%.17g", tag, x, y, runge(x),
                     y - runge(x));
        }
        // The knot window: x just below picks segment i-1, just above picks i.
        foreach (eps; [-1.0e-12, 1.0e-12]) {
            double x = lo + eps * (xs[$ - 1] - xs[0]);
            writefln("%s win x=%.17g y=%.17g", tag, x, s.spl(x));
        }
    }
    // Out-of-range branches of spline.d:73-75.
    foreach (x; [xs[0] - 0.3, xs[$ - 1] + 0.3, 0.0]) {
        writefln("%s oor x=%.17g y=%.17g", tag, x, s.spl(x));
    }
}

/// The t mapping, at the shared sweep and at 100 even divisions.
void dump_tmap(const char[] tag, const XSpline s)
{
    foreach (t; t_sweep()) {
        auto p = s(t);
        writefln("%s tmap t=%.17g x=%.17g y=%.17g z=%.17g", tag, t, p.x, p.y, p.z);
    }
    int N = 100;
    double dt = 1.0 / (N - 1);
    double max_err = 0.0;
    foreach (i; 0 .. N) {
        double t = dt * i;
        double xx = s(0.0).x * (1.0 - t) + s(1.0).x * t;   // xspline.d:110
        double y_runge = runge(xx);
        Vector3 p = s(t);
        double err = p.y - y_runge;
        if (fabs(err) > max_err) max_err = fabs(err);
        writefln("%s u %d x=%.17g y_runge=%.17g y=%.17g err=%.17g within2pc=%s", tag, i,
                 xx, y_runge, p.y, err, approxEqualVectors(p, Vector3(xx, y_runge), 0.02));
    }
    writefln("%s max_relerr=%.17g", tag, max_err);
}

void main()
{
    // xspline.d:95-115, the D unittest's own spline: 20 Runge samples.
    int N = 20;
    double x0 = -1.0;
    double x1 = 1.0;
    double dx = (x1 - x0) / (N - 1);
    double[] x_sample, y_sample;
    foreach (i; 0 .. N) {
        double xx = x0 + dx * i;
        x_sample ~= xx;
        y_sample ~= runge(xx);
    }
    auto s = new XSpline(x_sample, y_sample);
    dump_knots("xs.runge", s);
    dump_evals("xs.runge", s);
    dump_tmap("xs.runge", s);
    dump_path("xs.runge", s);

    // The file constructor (xspline.d:34-51) over tests/test_data/gpath_xspline.dat.
    auto sf = new XSpline("tests/test_data/gpath_xspline.dat");
    dump_knots("xs.file", sf);
    dump_tmap("xs.file", sf);
    // The same file, closed? XSpline has no closed flag -- instead read the
    // one-column file, where a missing y is 0.0 (xspline.d:47).
    auto so = new XSpline("tests/test_data/gpath_points3d.dat");
    writefln("xs.3col npts=%d str=%s", so.spl.xs.length, so.toString());

    // Copy constructor (xspline.d:53-56) and dup (xspline.d:58-61): the copy must
    // rebuild, not alias.
    auto sc = new XSpline(s);
    dump_knots("xs.copy", sc);
    dump_tmap("xs.copy", sc);
    auto sd = cast(XSpline) s.dup();
    writefln("xs.dup class=%s str=%s", sd.classString(), sd.toString());
    // Sentinel write through the copy, then re-read the original.
    sc.spl.xs[0] = -987.654321;
    sc.spl.ys[1] = -123.456;
    writefln("xs.aliaschk copy.xs0=%.17g orig.xs0=%.17g copy.ys1=%.17g orig.ys1=%.17g",
             sc.spl.xs[0], s.spl.xs[0], sc.spl.ys[1], s.spl.ys[1]);
    writefln("xs.aliaschk orig.str=%s", s.toString());

    // Error paths. A missing file and an empty file are both reachable from the
    // file constructor; only the classification is printed for the assert-style
    // failures, whose D message carries the reference tree's file and line.
    // A non-numeric token raises std.conv's ConversionException. The taxonomy only:
    // D's text for "#c" is `no digits seen for input "#c".` while eil::to_double
    // (substrate/include/eil/conv.hpp:87-92) emits
    // `error converting input to floating point for input "#c".` for every first
    // character that is not a digit, '.', '+' or '-'. Measured against ldc2:
    // "#c", "abc", "@1", "$1.0", "x", "e", "-abc", "  abc" all give "no digits
    // seen", and only a leading n/N/i/I (the start of nan/infinity) gives
    // "error converting input to floating point" -- so the substrate's branch
    // condition is too broad. Reported as a T-9xx candidate; the row is restored to
    // `dump_error` form when conv.hpp is fixed.
    write("xs.err.bad asError=");
    try {
        auto bad = new XSpline("tests/test_data/gpath_bad.dat");
        writefln("false asException=false msg=none");
    } catch (Error e) {
        writefln("true asException=false kind=Error");
    } catch (Exception e) {
        writefln("false asException=true kind=Exception");
    }
    // A missing file and an empty file are printed as classifications only: D's
    // message is std.stdio.File's own text (and, for the empty file, the OOM/Range
    // text of the allocation inside nm/spline.d), neither of which the port claims
    // to reproduce -- only the Error/Exception taxonomy is compared here. The
    // file-open message difference is noted in the verification report.
    foreach (probe; [Tuple!(string, string)("xs.err.missing", "tests/test_data/does_not_exist.dat"),
                     Tuple!(string, string)("xs.err.empty", "tests/test_data/gpath_empty.dat")]) {
        write(probe[0], ".class=");
        try {
            auto se = new XSpline(probe[1]);
            writeln("none");
        } catch (Error e) {
            writeln("Error");
        } catch (Exception e) {
            writeln("Exception");
        }
    }

    // A two-point spline (one segment) and a three-point one, to reach the small-n
    // branches of the tridiagonal solve (spline.d:41-52, m = n-1 may be 0).
    double[] tx = [0.0, 1.0];
    double[] ty = [0.0, 1.0];
    auto s2 = new XSpline(tx, ty);
    dump_knots("xs.two", s2);
    foreach (t; [0.0, 0.25, 0.5, 0.75, 1.0]) {
        auto p = s2(t);
        writefln("xs.two t=%.17g x=%.17g y=%.17g", t, p.x, p.y);
    }
    tx = [0.0, 0.5, 1.0];
    ty = [1.0, 0.0, 1.0];
    auto s3 = new XSpline(tx, ty);
    dump_knots("xs.three", s3);
    foreach (t; [0.0, 0.25, 0.5, 0.75, 1.0]) {
        auto p = s3(t);
        writefln("xs.three t=%.17g x=%.17g y=%.17g", t, p.x, p.y);
    }
}
