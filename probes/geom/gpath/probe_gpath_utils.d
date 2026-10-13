// probes/geom/gpath/probe_gpath_utils.d — what geom/gpath/gpath_utils.d produces.
//
// gpath_utils.d has NO unittest block, so this probe is its whole gate. It covers
// the four free functions:
//
//   * discretize_path (D:19-28) — a Path sampled at clusterf.distribute_parameter_values
//     (n) values, in order, with a LinearFunction, a RobertsFunction and the base
//     UnivariateFunction (whose opCall is the identity, D:19-22).
//   * readPointsFromFile (D:30-43) — the same strip/split/skip-comment tokenisation
//     the other three gpath file readers use, including the two edges those readers
//     were probed on: a line with only an x, and a line whose first token is "#"
//     exactly (skipped) versus "#c" (a ConversionException).
//   * optimiseBezierPoints (D:45-53 file form, D:55-174 point form) and
//     optimiseBezierPoints2 (D:176-315), which fit a Bezier to the points by running
//     nm.nelmin over a cost function that sums Path.closestDistance over the data.
//   * the two guard Exceptions (fewer data points than control points; an initial
//     guess with the wrong number of control points).
//
// WHY THE COUNTED COPIES ARE HERE: optimizeBezierPoints' n_fe/n_restart are local
// to D's function (gpath_utils.d:147, :282) and never reach the caller, so the
// number of simplex steps the fit took cannot be observed through the real entry
// point. The two `*_counted` functions below are line-for-line copies of
// gpath_utils.d:55-174 and :176-315 whose only additions are writefln of f_min,
// n_fe, n_restart and the design vector d after the minimize() call. Each counted
// result is compared against the real function's (rows "cmp.*"), so the counts can
// be attributed to the real code: if a port reaches the same optimum in a different
// number of evaluations, or a different d, the simplex arithmetic diverged.
//
// Two D behaviours here look like bugs and are dumped, not fixed:
//   * optimiseBezierPoints2's design vector is 2*(nCtrlPts-2)-2 wide (dim=2) but its
//     loop starts at i=2 and writes d[2*i-2], so d[0] and d[1] are left at 0 and the
//     last two entries are then OVERWRITTEN by the end-slope x values
//     (gpath_utils.d:227-244). The two dead entries still make the simplex two
//     dimensions wider than the free parameters the cost function reads.
//   * both cost functions share one `double t` declared in the enclosing function
//     (D:58, :180) and pass it by ref to closestDistance, so t carries the previous
//     data point's parameter into the next evaluation. It is not read afterwards, so
//     this is not observable in the output — but the port must not restructure it.
//
// Regenerate (exact commands; nothing under /home/tang/packages/gdtk is written —
// the reference tree is only READ as the cwd for the relative data path, and the
// copy under /tmp is what gets compiled):
//   cd /home/tang/packages/Eilmer
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
//   /home/tang/packages/dlang/bin/ldc2 -i -I/tmp/gdtk_shadow \
//     -I/tmp/gdtk_shadow/extern/gzip probes/geom/gpath/probe_gpath_utils.d \
//     -of=/tmp/probe_gpath_utils_d
//   /tmp/probe_gpath_utils_d /home/tang/packages/gdtk/src/geom/sample-data \
//     /home/tang/packages/Eilmer/tests/test_data \
//     > baselines/e587b235/probes/geom_gpath_gpath_utils.txt
//
// See probe_path.d for why the reference tree is copied (ldc2 1.40 rejects `new`
// inside @nogc in geom/elements/projection.d, nm/bbla.d:709, geom/grid/grid.d,
// sgrid.d and nm/brent.d via geom.misc.univariatefunctions; DMD 2.110 compiles the
// unmodified tree and agrees with the shadow-tree bytes).
import std.conv;
import std.format;
import std.math;
import std.stdio;

import geom.elements;
import geom.gpath.path;
import geom.gpath.line;
import geom.gpath.bezier;
import geom.gpath.gpath_utils;
import geom.misc.univariatefunctions;
import nm.nelmin;
import nm.number;

string esc(const(char)[] msg)
{
    string res = "";
    foreach (dchar c; msg) {
        if (c == '\n') res ~= "\\n";
        else if (c == '\r') res ~= "\\r";
        else res ~= c;
    }
    return res;
}

void dump_points(const char[] tag, const Vector3[] pts)
{
    writefln("%s n=%d", tag, pts.length);
    foreach (i, p; pts) {
        writefln("%s ptd[%d] x=%.17g y=%.17g z=%.17g", tag, i, p.x, p.y, p.z);
    }
}

void dump_bezier(const char[] tag, const Bezier bz)
{
    writefln("%s str=%s", tag, bz.toString());
    writefln("%s class=%s", tag, bz.classString());
    writefln("%s nb=%d L=%.17g", tag, bz.B.length, bz.length());
    foreach (i, p; bz.B) {
        writefln("%s B[%d] x=%.17g y=%.17g z=%.17g", tag, i, p.x, p.y, p.z);
    }
    // Six parameter values on the fitted curve, so a fit that lands on the same
    // control points but evaluates differently still shows up.
    foreach (t; [0.0, 0.1, 0.25, 0.5, 0.75, 1.0]) {
        auto p = bz(t);
        writefln("%s opCall t=%.17g x=%.17g y=%.17g z=%.17g", tag, t, p.x, p.y, p.z);
    }
}

void dump_ts(const char[] tag, const double[] ts)
{
    writefln("%s n=%d", tag, ts.length);
    foreach (i, t; ts) {
        writefln("%s ts[%d]=%.17g", tag, i, t);
    }
}

bool same_bezier(const Bezier a, const Bezier b)
{
    if (a is null || b is null) return a is b;
    if (a.B.length != b.B.length) return false;
    foreach (i; 0 .. a.B.length) {
        if (a.B[i].x != b.B[i].x || a.B[i].y != b.B[i].y || a.B[i].z != b.B[i].z) {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Line-for-line copies of gpath_utils.d:55-174 and :176-315 that publish the
// minimiser counters. The only additions are marked [+probe].
// ---------------------------------------------------------------------------
Bezier optimiseBezierPoints_counted(Vector3[] points, int nCtrlPts, Bezier initGuess,
                                    ref double[] ts, ref bool success,
                                    double tol=1.0e-06, int max_steps=10000, int dim=2)
{
    double t;
    if (points.length < nCtrlPts) {
        string errMsg = "Error in optimiseBezierPoints: there are fewer data points than the desired number of control points.\n";
        errMsg ~= format("No. of data points: %d\n", points.length);
        errMsg ~= format("No. of desired control points: %d", nCtrlPts);
        throw new Exception(errMsg);
    }
    ts.length = points.length;

    Bezier myBez;
    if (initGuess) {
        if (initGuess.B.length != nCtrlPts) {
            string errMsg = "Error in optimiseBezierPoints: the supplied initial guess Bezier does not have the same number of controls points\n";
            errMsg ~= "as the desired number of control points for the optimised Bezier.\n";
            errMsg ~= format("No. of control points desired: %d\n", nCtrlPts);
            errMsg ~= format("No. of control points in supplied initial guess: %d", initGuess.B.length);
            throw new Exception(errMsg);
        }
        myBez = new Bezier(initGuess);
    }
    else {
        Vector3[] bezPts;
        bezPts ~= points[0];
        auto line = new Line(points[0], points[$-1]);
        double dt = 1.0/(nCtrlPts-1);
        foreach (i; 1 .. nCtrlPts-1) {
            bezPts ~= line(i*dt);
        }
        bezPts ~= points[$-1];
        myBez = new Bezier(bezPts);
    }
    number[] d;
    if (dim == 2)
        d.length = 2*(nCtrlPts-2);
    else
        d.length = 3*(nCtrlPts-2);
    foreach (i; 1 .. nCtrlPts-1) {
        if (dim == 2) {
            d[2*i - 2] = myBez.B[i].x;
            d[2*i - 1] = myBez.B[i].y;
        }
        else {
            d[3*i - 3] = myBez.B[i].x;
            d[3*i - 2] = myBez.B[i].y;
            d[3*i - 1] = myBez.B[i].z;
        }
    }

    number funMin(number[] d)
    {
        foreach (i; 1 .. nCtrlPts-1) {
            if (dim == 2) {
                myBez.B[i].x = d[2*i - 2];
                myBez.B[i].y = d[2*i - 1];
            }
            else {
                myBez.B[i].x = d[3*i - 3];
                myBez.B[i].y = d[3*i - 2];
                myBez.B[i].z = d[3*i - 1];
            }
        }
        number err = 0.0;
        foreach (p; points) {
            err += myBez.closestDistance(p, t);
        }
        return err;
    }

    number f_min;
    int n_fe, n_restart;
    number[] dx;
    dx.length = d.length;
    number dp = myBez.length()/100.0;
    dx[] = dp;
    success = nm.nelmin.minimize!(funMin,number)(d, f_min, n_fe, n_restart, dx, tol, 1, max_steps);
    writefln("counted.bez dp=%.17g f_min=%.17g n_fe=%d n_restart=%d success=%s",
             dp, f_min, n_fe, n_restart, success); // [+probe]
    foreach (i, v; d) {
        writefln("counted.bez d[%d]=%.17g", i, v); // [+probe]
    }
    foreach (i; 1 .. nCtrlPts-1) {
        if (dim == 2) {
            myBez.B[i].x = d[2*i - 2];
            myBez.B[i].y = d[2*i - 1];
        }
        else {
            myBez.B[i].x = d[3*i - 3];
            myBez.B[i].y = d[3*i - 2];
            myBez.B[i].z = d[3*i - 1];
        }
    }
    ts[0] = 0.0;
    foreach (i; 1 .. points.length-1) {
        myBez.closestDistance(points[i], t);
        ts[i] = t;
    }
    ts[$-1] = 1.0;
    return myBez;
}

Bezier optimiseBezierPoints2_counted(Vector3[] points, int nCtrlPts, Bezier initGuess,
                                     Vector3 startSlope, Vector3 endSlope,
                                     ref double[] ts, ref bool success,
                                     double tol=1.0e-06, int max_steps=10000, int dim=2)
{
    double t;
    if (points.length < nCtrlPts) {
        string errMsg = "Error in optimiseBezierPoints: there are fewer data points than the desired number of control points.\n";
        errMsg ~= format("No. of data points: %d\n", points.length);
        errMsg ~= format("No. of desired control points: %d", nCtrlPts);
        throw new Exception(errMsg);
    }
    ts.length = points.length;

    Bezier myBez;
    if (initGuess) {
        if (initGuess.B.length != nCtrlPts) {
            string errMsg = "Error in optimiseBezierPoints: the supplied initial guess Bezier does not have the same number of controls points\n";
            errMsg ~= "as the desired number of control points for the optimised Bezier.\n";
            errMsg ~= format("No. of control points desired: %d\n", nCtrlPts);
            errMsg ~= format("No. of control points in supplied initial guess: %d", initGuess.B.length);
            throw new Exception(errMsg);
        }
        myBez = new Bezier(initGuess);
    }
    else {
        Vector3[] bezPts;
        bezPts ~= points[0];
        auto line = new Line(points[0], points[$-1]);
        double dt = 1.0/(nCtrlPts-1);
        foreach (i; 1 .. nCtrlPts-1) {
            bezPts ~= line(i*dt);
        }
        bezPts ~= points[$-1];
        auto scale0 = (bezPts[1].x - bezPts[0].x)/startSlope.x;
        bezPts[1] = bezPts[0] + scale0*startSlope;
        auto scale1 = (bezPts[nCtrlPts-1].x - bezPts[nCtrlPts-2].x)/endSlope.x;
        bezPts[nCtrlPts-2] = bezPts[nCtrlPts-1] - scale1*endSlope;
        myBez = new Bezier(bezPts);
    }
    number[] d;
    if (dim == 2)
        d.length = 2*(nCtrlPts-2) - 2;
    else
        d.length = 3*(nCtrlPts-2) - 4;
    foreach (i; 2 .. nCtrlPts-2) {
        if (dim == 2) {
            d[2*i - 2] = myBez.B[i].x;
            d[2*i - 1] = myBez.B[i].y;
        }
        else {
            d[3*i - 3] = myBez.B[i].x;
            d[3*i - 2] = myBez.B[i].y;
            d[3*i - 1] = myBez.B[i].z;
        }
    }
    d[$-2] = myBez.B[1].x;
    d[$-1] = myBez.B[nCtrlPts-2].x;

    number funMin(number[] d)
    {
        foreach (i; 2 .. nCtrlPts-2) {
            if (dim == 2) {
                myBez.B[i].x = d[2*i - 2];
                myBez.B[i].y = d[2*i - 1];
            }
            else {
                myBez.B[i].x = d[3*i - 3];
                myBez.B[i].y = d[3*i - 2];
                myBez.B[i].z = d[3*i - 1];
            }
        }
        auto scale0 = (d[$-2] - myBez.B[0].x)/startSlope.x;
        myBez.B[1] = myBez.B[0] + scale0*startSlope;
        auto scale1 = (myBez.B[nCtrlPts-1].x - d[$-1])/endSlope.x;
        myBez.B[nCtrlPts-2] = myBez.B[nCtrlPts-1] - scale1*endSlope;
        number err = 0.0;
        foreach (p; points) {
            err += myBez.closestDistance(p, t);
        }
        return err;
    }

    number f_min;
    int n_fe, n_restart;
    number[] dx;
    dx.length = d.length;
    number dp = myBez.length()/100.0;
    dx[] = dp;
    success = nm.nelmin.minimize!(funMin,number)(d, f_min, n_fe, n_restart, dx, tol, 1, max_steps);
    writefln("counted.bez2 dp=%.17g f_min=%.17g n_fe=%d n_restart=%d success=%s",
             dp, f_min, n_fe, n_restart, success); // [+probe]
    foreach (i, v; d) {
        writefln("counted.bez2 d[%d]=%.17g", i, v); // [+probe]
    }
    foreach (i; 2 .. nCtrlPts-2) {
        if (dim == 2) {
            myBez.B[i].x = d[2*i - 2];
            myBez.B[i].y = d[2*i - 1];
        }
        else {
            myBez.B[i].x = d[3*i - 3];
            myBez.B[i].y = d[3*i - 2];
            myBez.B[i].z = d[3*i - 1];
        }
    }
    auto scale0 = (d[$-2] - myBez.B[0].x)/startSlope.x;
    myBez.B[1] = myBez.B[0] + scale0*startSlope;
    auto scale1 = (myBez.B[nCtrlPts-1].x - d[$-1])/endSlope.x;
    myBez.B[nCtrlPts-2] = myBez.B[nCtrlPts-1] - scale1*endSlope;
    ts[0] = 0.0;
    foreach (i; 1 .. points.length-1) {
        myBez.closestDistance(points[i], t);
        ts[i] = t;
    }
    ts[$-1] = 1.0;
    return myBez;
}

// ---------------------------------------------------------------------------
void main(string[] args)
{
    // The data files come from argv so the same run works from any cwd:
    //   argv[1] = the directory holding cowl-spline-pts.dat (gdtk src/geom/sample-data)
    //   argv[2] = Eilmer's tests/test_data
    const string sampleDir = args[1];
    const string testData = args[2];
    const string cowl = sampleDir ~ "/cowl-spline-pts.dat";

    // ---- discretize_path (D:19-28) -----------------------------------------
    {
        auto pth = new Line(Vector3(0.0, 0.0), Vector3(3.0, 4.0));
        auto lin = new LinearFunction(0.0, 1.0);
        dump_points("disc.line.linear", discretize_path(pth, 5, lin));
        auto rob = new RobertsFunction(true, false, 0.3);
        dump_points("disc.line.roberts", discretize_path(pth, 6, rob));
        auto quad = new QuadraticFunction(4.0, false);
        dump_points("disc.line.quadratic", discretize_path(pth, 7, quad));
        // The base UnivariateFunction as the cluster function: distribute_parameter_values
        // is linear and opCall is the identity (univariatefunctions.d:19-38), so the
        // sampled points march along the line.
        auto base = new UnivariateFunction();
        dump_points("disc.line.base", discretize_path(pth, 4, base));
        // n = 1 and n = 0 hit the two divide-by-degenerate cases of
        // distribute_parameter_values (see the note in geom/misc/univariatefunctions.hpp).
        dump_points("disc.line.linear.n1", discretize_path(pth, 1, lin));
        dump_points("disc.line.linear.n0", discretize_path(pth, 0, lin));
        // A curved path, so the sampled parameters actually bend the polyline.
        auto bez = new Bezier([Vector3(0.0, 0.0), Vector3(1.0, 2.0), Vector3(3.0, 0.0)]);
        dump_points("disc.bez.linear", discretize_path(bez, 5, lin));
    }

    // ---- readPointsFromFile (D:30-43) --------------------------------------
    Vector3[] cowlPts;
    readPointsFromFile(cowl, cowlPts);
    dump_points("read.cowl", cowlPts);

    Vector3[] edgePts;
    readPointsFromFile(testData ~ "/gpath_edge.dat", edgePts);
    dump_points("read.edge", edgePts);

    Vector3[] p3Pts;
    readPointsFromFile(testData ~ "/gpath_points3d.dat", p3Pts);
    dump_points("read.points3d", p3Pts);

    try {
        Vector3[] bad;
        readPointsFromFile(testData ~ "/gpath_bad.dat", bad);
        writefln("read.bad asError=false asException=false n=%d", bad.length);
    } catch (Exception e) {
        writefln("read.bad asError=false asException=true msg=%s", esc(e.msg));
    } catch (Error e) {
        writefln("read.bad asError=true asException=false msg=%s", esc(e.msg));
    }
    try {
        Vector3[] missing;
        readPointsFromFile(testData ~ "/no_such_file_here.dat", missing);
        writefln("read.missing asError=false asException=false n=%d", missing.length);
    } catch (Exception e) {
        // The message of a FileException quotes the absolute path, which the C++
        // side is handed differently (argv), so only the taxonomy is compared here.
        writefln("read.missing asError=false asException=true");
    } catch (Error e) {
        writefln("read.missing asError=true asException=false");
    }
    // An empty file leaves points untouched (the caller's array stays empty).
    Vector3[] emptyPts;
    readPointsFromFile(testData ~ "/gpath_empty.dat", emptyPts);
    dump_points("read.empty", emptyPts);

    // ---- the file form of optimiseBezierPoints (D:45-53), the demo's call ----
    {
        Bezier b_init_guess;
        double[] ts;
        bool success;
        Bezier bz = optimiseBezierPoints(cowl, 7, b_init_guess, ts, success);
        dump_bezier("fit.file.n7", bz);
        dump_ts("fit.file.n7", ts);
        writefln("fit.file.n7 success=%s", success);
    }

    // ---- the point form (D:55-174) -----------------------------------------
    double[] ts1;
    bool success1;
    Bezier real1 = optimiseBezierPoints(cowlPts, 7, null, ts1, success1);
    dump_bezier("fit.pts.n7", real1);
    dump_ts("fit.pts.n7", ts1);
    writefln("fit.pts.n7 success=%s", success1);

    double[] ts2;
    bool success2;
    Bezier cnt2 = optimiseBezierPoints_counted(cowlPts, 7, null, ts2, success2);
    writefln("cmp.pts.n7 sameBez=%s sameTs=%s", same_bezier(real1, cnt2), ts1 == ts2);

    // Five control points on the same data (a smaller simplex).
    double[] ts3;
    bool success3;
    Bezier real3 = optimiseBezierPoints(cowlPts, 5, null, ts3, success3);
    dump_bezier("fit.pts.n5", real3);
    dump_ts("fit.pts.n5", ts3);
    writefln("fit.pts.n5 success=%s", success3);
    double[] ts3c;
    bool success3c;
    Bezier cnt3 = optimiseBezierPoints_counted(cowlPts, 5, null, ts3c, success3c);
    writefln("cmp.pts.n5 sameBez=%s sameTs=%s", same_bezier(real3, cnt3), ts3c == ts3);

    // A supplied initial guess (D:73-82) — a Bezier through the same endpoints with
    // the interior points pushed away from the chord.
    {
        auto guess = new Bezier([Vector3(cowlPts[0].x, cowlPts[0].y),
                                 Vector3(0.9, 0.3), Vector3(1.0, 0.25),
                                 Vector3(1.1, 0.22), Vector3(1.2, 0.2),
                                 Vector3(1.25, 0.19),
                                 Vector3(cowlPts[$-1].x, cowlPts[$-1].y)]);
        double[] ts4;
        bool success4;
        Bezier real4 = optimiseBezierPoints(cowlPts, 7, guess, ts4, success4);
        dump_bezier("fit.pts.guess", real4);
        dump_ts("fit.pts.guess", ts4);
        writefln("fit.pts.guess success=%s", success4);
        double[] ts4c;
        bool success4c;
        Bezier cnt4 = optimiseBezierPoints_counted(cowlPts, 7, guess, ts4c, success4c);
        writefln("cmp.pts.guess sameBez=%s sameTs=%s",
                 same_bezier(real4, cnt4), ts4 == ts4c);
        // The guess is copied (D:81 `new Bezier(initGuess)`), so the caller's
        // object is not mutated by the fit.
        writefln("fit.pts.guess kept B0 x=%.17g B3 x=%.17g", guess.B[0].x, guess.B[3].x);
    }

    // dim = 3 (D:100-113, :109-113): three design values per interior control point.
    {
        double[] ts5;
        bool success5;
        Bezier real5 = optimiseBezierPoints(cowlPts, 5, null, ts5, success5, 1.0e-06, 10000, 3);
        dump_bezier("fit.pts.dim3", real5);
        dump_ts("fit.pts.dim3", ts5);
        writefln("fit.pts.dim3 success=%s", success5);
        double[] ts5c;
        bool success5c;
        Bezier cnt5 = optimiseBezierPoints_counted(cowlPts, 5, null, ts5c, success5c,
                                                  1.0e-06, 10000, 3);
        writefln("cmp.pts.dim3 sameBez=%s sameTs=%s", same_bezier(real5, cnt5), ts5 == ts5c);
    }

    // A looser tolerance terminates in fewer steps; the counts are the point.
    {
        double[] ts6;
        bool success6;
        Bezier cnt6 = optimiseBezierPoints_counted(cowlPts, 5, null, ts6, success6,
                                                  1.0e-03, 10000, 2);
        dump_bezier("fit.pts.tol1e3", cnt6);
        dump_ts("fit.pts.tol1e3", ts6);
    }

    // ---- optimiseBezierPoints2 (D:176-315) ---------------------------------
    {
        double[] ts7;
        bool success7;
        auto sl = Vector3(1.0, 0.5, 0.0);
        auto sr = Vector3(1.0, -0.25, 0.0);
        Bezier real7 = optimiseBezierPoints2(cowlPts, 7, null, sl, sr, ts7, success7);
        dump_bezier("fit.pts2.n7", real7);
        dump_ts("fit.pts2.n7", ts7);
        writefln("fit.pts2.n7 success=%s", success7);
        double[] ts7c;
        bool success7c;
        Bezier cnt7 = optimiseBezierPoints2_counted(cowlPts, 7, null, sl, sr, ts7c, success7c);
        writefln("cmp.pts2.n7 sameBez=%s sameTs=%s", same_bezier(real7, cnt7), ts7 == ts7c);

        // The slope constraint with a guess supplied, and the dim=3 branch
        // (d.length = 3*(nCtrlPts-2) - 4, D:231).
        auto guess = new Bezier([Vector3(cowlPts[0].x, cowlPts[0].y),
                                 Vector3(0.9, 0.3), Vector3(1.0, 0.25),
                                 Vector3(1.1, 0.22), Vector3(1.2, 0.2),
                                 Vector3(1.25, 0.19),
                                 Vector3(cowlPts[$-1].x, cowlPts[$-1].y)]);
        double[] ts8;
        bool success8;
        Bezier cnt8 = optimiseBezierPoints2_counted(cowlPts, 7, guess, sl, sr, ts8, success8);
        dump_bezier("fit.pts2.guess", cnt8);
        dump_ts("fit.pts2.guess", ts8);

        // dim = 3 here is dead on arrival: d.length is 3*(nCtrlPts-2) - 4 while the
        // fill loop writes up to 3*(nCtrlPts-3) - 1, two entries past the end, for
        // every nCtrlPts. D's bounds check fires, and so does the port's (the same
        // "index [N] is out of bounds" RangeError, which is an Error not an Exception).
        double[] ts9;
        bool success9;
        try {
            auto cnt9 = optimiseBezierPoints2_counted(cowlPts, 6, null, sl, sr, ts9, success9,
                                                     1.0e-06, 10000, 3);
            dump_bezier("fit.pts2.dim3", cnt9);
            dump_ts("fit.pts2.dim3", ts9);
        } catch (Exception e) {
            writefln("fit.pts2.dim3 asError=false asException=true msg=%s", esc(e.msg));
        } catch (Error e) {
            writefln("fit.pts2.dim3 asError=true asException=false msg=%s", esc(e.msg));
        }
        // The real function, same input: the guard fires at the same place.
        double[] ts9r;
        bool success9r;
        try {
            auto r9 = optimiseBezierPoints2(cowlPts, 6, null, sl, sr, ts9r, success9r,
                                           1.0e-06, 10000, 3);
            dump_bezier("fit.pts2.dim3real", r9);
        } catch (Exception e) {
            writefln("fit.pts2.dim3real asError=false asException=true msg=%s", esc(e.msg));
        } catch (Error e) {
            writefln("fit.pts2.dim3real asError=true asException=false msg=%s", esc(e.msg));
        }
    }

    // ---- the two guards (D:61-66, :74-80) ----------------------------------
    {
        double[] tsg;
        bool sg;
        try {
            auto bz = optimiseBezierPoints(cowlPts, 40, null, tsg, sg);
            writefln("fit.err.toofew built=1 nb=%d", bz.B.length);
        } catch (Exception e) {
            writefln("fit.err.toofew built=0 asError=false asException=true msg=%s", esc(e.msg));
        } catch (Error e) {
            writefln("fit.err.toofew built=0 asError=true asException=false msg=%s", esc(e.msg));
        }
        auto guess3 = new Bezier([Vector3(0.0), Vector3(0.5, 0.5), Vector3(1.0)]);
        try {
            auto bz = optimiseBezierPoints(cowlPts, 7, guess3, tsg, sg);
            writefln("fit.err.guess built=1 nb=%d", bz.B.length);
        } catch (Exception e) {
            writefln("fit.err.guess built=0 asError=false asException=true msg=%s", esc(e.msg));
        } catch (Error e) {
            writefln("fit.err.guess built=0 asError=true asException=false msg=%s", esc(e.msg));
        }
        // The same two guards in optimiseBezierPoints2 (its messages name
        // optimiseBezierPoints, D:184 and D:197 — a copy-paste in D that is kept).
        auto sl = Vector3(1.0, 0.5, 0.0);
        auto sr = Vector3(1.0, -0.25, 0.0);
        try {
            auto bz = optimiseBezierPoints2(cowlPts, 40, null, sl, sr, tsg, sg);
            writefln("fit2.err.toofew built=1 nb=%d", bz.B.length);
        } catch (Exception e) {
            writefln("fit2.err.toofew built=0 asError=false asException=true msg=%s", esc(e.msg));
        } catch (Error e) {
            writefln("fit2.err.toofew built=0 asError=true asException=false msg=%s", esc(e.msg));
        }
        try {
            auto bz = optimiseBezierPoints2(cowlPts, 7, guess3, sl, sr, tsg, sg);
            writefln("fit2.err.guess built=1 nb=%d", bz.B.length);
        } catch (Exception e) {
            writefln("fit2.err.guess built=0 asError=false asException=true msg=%s", esc(e.msg));
        } catch (Error e) {
            writefln("fit2.err.guess built=0 asError=true asException=false msg=%s", esc(e.msg));
        }
    }
} // end main
