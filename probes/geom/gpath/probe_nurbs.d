// probes/geom/gpath/probe_nurbs.d — what geom/gpath/nurbs.d produces.
//
// NURBS is the Path that wraps Piegl & Tiller's A4.1 de Boor evaluation on top of
// geom/misc/nurbs_utils.d's findSpan/basisFuns. Four things are measured here
// rather than assumed:
//
//  1. the toString() text (D:68-71), which is `to!string` of a `double[4][]` and a
//     `double[]` — the exact bracket/comma/number rendering the port's d_tostr has
//     to match (rows "nurbs.*.str").
//  2. the four STATIC fields (D:82-85: mN, mCw, mC, mNws). They are shared by every
//     NURBS object in the process, and each constructor resizes mN to its own p+1
//     and replaces mNws. So a degree-2 curve evaluated AFTER a degree-5 one reads
//     mN[0..2] of an array the last constructor sized to 6 (harmless), but the
//     other order leaves mN at 3 entries and basisFuns writes index 3 of it, which
//     is a RangeError. Measured, not inferred: rows "nurbs.static.*". D's own
//     unittest block does not cover this; the port keeps the statics so it cannot
//     drift.
//  3. the three constructor guards (D:23-42), all `throw new Error`, whose messages
//     are format("%s") of a size_t and an int (rows "nurbs.err.*").
//  4. the knot-vector edges: u exactly at the last knot (findSpan's early `return n`
//     branch), u one ULP beyond it (the tol=1e-12 adjustment in nurbs_utils.d:32-36),
//     and u well outside the range, which is an Exception, not an Error.
//
// See probe_path.d for why the reference tree is copied before building (gdtk at
// e587b235 does not compile as committed with ldc2 1.40: @nogc functions that use
// `new` in geom/elements/projection.d, nm/bbla.d:709, geom/grid/grid.d, sgrid.d and
// nm/brent.d via geom.misc.univariatefunctions). The @nogc deletion is a
// compile-time-only attribute change; DMD 2.110 compiles the UNMODIFIED tree and
// agrees with the shadow-tree output byte for byte (see probes/geom/gpath/probe_path.d).
//
// Regenerate (exact commands; nothing under /home/tang/packages/gdtk is written):
//   cd /home/tang/packages/Eilmer
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
//   /home/tang/packages/dlang/bin/ldc2 -i -I/tmp/gdtk_shadow \
//     -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/gpath \
//     probes/geom/gpath/probe_nurbs.d -of=/tmp/probe_nurbs_d
//   /tmp/probe_nurbs_d > baselines/e587b235/probes/geom_gpath_nurbs.txt
import std.conv;
import std.format;
import std.math;
import std.stdio;

import geom.elements;
import geom.gpath.path;
import geom.gpath.nurbs;
import nm.number;
import pathdump;

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

// Evaluate one curve at a sweep of u, printing the raw point. The sweep is D's own
// unittest value plus the knot boundaries and the mid-knot values, so a port that
// picks the wrong span shows up here rather than in the aggregate dumps only.
void sweep(const char[] tag, const NURBS nc)
{
    foreach (u; [0.0, 0.001, 0.375, 0.4, 0.5, 0.6, 0.625, 0.9, 0.999, 1.0]) {
        auto p = nc(u);
        writefln("%s opCall u=%.17g x=%.17g y=%.17g z=%.17g", tag, u, p.x, p.y, p.z);
    }
}

void build(const char[] tag, double[4][] Pw, double[] U, int p)
{
    try {
        auto nc = new NURBS(Pw, U, p);
        writefln("%s built=1 str=%s", tag, nc.toString());
    } catch (Exception e) {
        writefln("%s built=0 asError=false asException=true msg=%s", tag, esc(e.msg));
    } catch (Error e) {
        writefln("%s built=0 asError=true asException=false msg=%s", tag, esc(e.msg));
    }
}

void main()
{
    // ---- nurbs.d's own unittest (D:101-114) --------------------------------
    double[4][] Pw = [
        [-4.0, -4.0, 0.0, 1.0],
        [-2.0, 4.0, 0.0, 1.0],
        [2.0 * 5, -4.0 * 5, 0.0, 5],
        [4.0, 4.0, 0.0, 1.0],
        [3.778, 1.836, 2.933, 1.0],
        [4 * 2.772, -4 * 3.875, 4 * 1.736, 4.0]
    ];
    int p = 2;
    double[] U = [0.0, 0.0, 0.0, 0.375, 0.5, 0.625, 1.0, 1.0, 1.0];
    auto ncurve = new NURBS(Pw, U, p);
    auto puni = ncurve(0.6);
    writefln("un.pt x=%.17g y=%.17g z=%.17g", puni.x, puni.y, puni.z);
    writefln("un.approxEqual=%s", approxEqualVectors(Vector3(3.782, 2.939, 0.435), puni));
    writefln("un.isClose x=%s y=%s z=%s",
             isClose(puni.x, 3.782), isClose(puni.y, 2.939), isClose(puni.z, 0.435));

    // toString/classString/dup (D:60-75).
    writefln("nurbs.str=%s", ncurve.toString());
    writefln("nurbs.class=%s", ncurve.classString());
    auto ndup = ncurve.dup();
    writefln("nurbs.dup.class=%s", ndup.classString());
    writefln("nurbs.dup.str=%s", ndup.toString());
    auto pdup = ndup(0.6);
    writefln("nurbs.dup u=%.17g x=%.17g y=%.17g z=%.17g", 0.6, pdup.x, pdup.y, pdup.z);

    // The whole Path interface through the NURBS (dpdt/d2pdt2 are Path's finite
    // differences; length/closestDistance/intersect2D all go through opCall).
    dump_path("nurbs.p2", ncurve);
    sweep("nurbs.p2", ncurve);

    // The copy constructor (D:51-59): same payload, no validation, and it resizes
    // the shared mN to the copied degree.
    auto ncopy = new NURBS(ncurve);
    writefln("nurbs.copy str=%s", ncopy.toString());
    auto pc = ncopy(0.6);
    writefln("nurbs.copy u=%.17g x=%.17g y=%.17g z=%.17g", 0.6, pc.x, pc.y, pc.z);

    // ---- a degree-5 curve on a clamped uniform knot vector -----------------
    double[4][] Pw5 = [
        [0.0, 0.0, 0.0, 1.0],
        [1.0, 2.0, 0.0, 1.0],
        [2.0, 1.0, 0.5, 2.0],
        [3.0, 3.0, 1.0, 1.0],
        [4.0, 1.5, 0.0, 0.5],
        [5.0, 2.5, 0.0, 1.0]
    ];
    double[] U5 = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0];
    {
        auto nc5 = new NURBS(Pw5, U5, 5);
        writefln("nurbs.p5.str=%s", nc5.toString());
        sweep("nurbs.p5", nc5);
        dump_path("nurbs.p5", nc5);
    }

    // ---- the static fields, in both construction orders -------------------
    // mN/mNws are module-wide; the last constructor wins. Degree 5 first, then a
    // degree-2 curve: mN is left with 6 slots, and the degree-2 evaluation only
    // writes 0..2 of them, so it agrees with the standalone result above.
    {
        double[4][] small2 = [[-4.0, -4.0, 0.0, 1.0], [-2.0, 4.0, 0.0, 1.0],
                              [10.0, -20.0, 0.0, 5.0], [4.0, 4.0, 0.0, 1.0],
                              [3.778, 1.836, 2.933, 1.0], [11.088, -15.5, 6.944, 4.0]];
        double[] U2 = [0.0, 0.0, 0.0, 0.375, 0.5, 0.625, 1.0, 1.0, 1.0];
        auto after5 = new NURBS(small2, U2, 2);
        auto pa = after5(0.6);
        writefln("nurbs.static.after5 u=%.17g x=%.17g y=%.17g z=%.17g", 0.6, pa.x, pa.y, pa.z);
        // Now a degree-5 curve is built and evaluated, which resizes mN to 6 again.
        auto nc5b = new NURBS(Pw5, U5, 5);
        auto pb = nc5b(0.5);
        writefln("nurbs.static.p5again u=%.17g x=%.17g y=%.17g z=%.17g", 0.5, pb.x, pb.y, pb.z);
        // And a degree-4 curve on a 5-point net: mN becomes 5 wide, so the
        // degree-2 curve above still evaluates (it reads only 0..2), but a
        // freshly built degree-6 curve would find mN too narrow.
        auto nc2c = new NURBS(small2, U2, 2);
        auto pc2 = nc2c(0.375);
        writefln("nurbs.static.p2again u=%.17g x=%.17g y=%.17g z=%.17g", 0.375,
                 pc2.x, pc2.y, pc2.z);
        // The interference itself: hold on to a degree-5 curve, then construct a
        // degree-2 one (which resizes the shared mN to 3 slots), then evaluate the
        // degree-5 curve. basisFuns writes N[3] into mN and D's bounds check fires.
        auto keep5 = new NURBS(Pw5, U5, 5);
        auto shrink = new NURBS(small2, U2, 2);
        try {
            auto pk = keep5(0.5);
            writefln("nurbs.static.keep5 x=%.17g y=%.17g z=%.17g", pk.x, pk.y, pk.z);
        } catch (Exception e) {
            writefln("nurbs.static.keep5 asError=false asException=true msg=%s", esc(e.msg));
        } catch (Error e) {
            writefln("nurbs.static.keep5 asError=true asException=false msg=%s", esc(e.msg));
        }
        // mCw (D:83) is a static double[4] and mC (D:84) a static double[3], so they
        // are shared too, but deBoor zeroes both before every use, so the sharing is
        // not observable through opCall. Rebuilding the degree-5 curve restores mN,
        // and the value matches the earlier p5 row for the same u.
        try {
            auto pr = keep5(0.25);
            writefln("nurbs.static.retry u=%.17g x=%.17g y=%.17g z=%.17g", 0.25,
                     pr.x, pr.y, pr.z);
        } catch (Exception e) {
            writefln("nurbs.static.retry asError=false asException=true msg=%s", esc(e.msg));
        } catch (Error e) {
            writefln("nurbs.static.retry asError=true asException=false msg=%s", esc(e.msg));
        }
        auto rebuild5 = new NURBS(Pw5, U5, 5);
        auto prb = rebuild5(0.5);
        writefln("nurbs.static.rebuild5 u=%.17g x=%.17g y=%.17g z=%.17g", 0.5,
                 prb.x, prb.y, prb.z);
    }

    // ---- constructor guards (D:23-42), all `throw new Error` --------------
    double[4][] one = [[0.0, 0.0, 0.0, 1.0]];
    double[4][] two = [[0.0, 0.0, 0.0, 1.0], [1.0, 1.0, 0.0, 1.0]];
    double[4][] three = [[0.0, 0.0, 0.0, 1.0], [1.0, 1.0, 0.0, 1.0], [2.0, 0.0, 0.0, 1.0]];
    build("nurbs.err.none", [], [], 1);
    build("nurbs.err.one", one, [0.0, 0.0, 1.0], 1);
    build("nurbs.err.degree", two, [0.0, 0.0, 1.0], 2);
    build("nurbs.err.knots", two, [0.0, 1.0], 1);
    build("nurbs.err.knotsextra", two, [0.0, 0.5, 1.0], 1);
    build("nurbs.err.zero", two, [0.0, 1.0], 0);
    // A valid linear curve, for the record (a=1, p=1 needs 4 knots).
    build("nurbs.ok.linear", two, [0.0, 0.0, 1.0, 1.0], 1);

    // ---- parameter range edges -------------------------------------------
    {
        auto nc = new NURBS(Pw, U, 2);
        auto pend = nc(1.0);
        writefln("nurbs.edge u=%.17g x=%.17g y=%.17g z=%.17g", 1.0, pend.x, pend.y, pend.z);
        auto pstart = nc(0.0);
        writefln("nurbs.edge u=%.17g x=%.17g y=%.17g z=%.17g", 0.0, pstart.x, pstart.y, pstart.z);
        // 1 ULP above the last knot: nurbs_utils.d:34-36 pulls u back inside the
        // range with tol = 1e-12, so this evaluates rather than throwing.
        double just_over = 1.0 + 1e-13;
        auto pio = nc(just_over);
        writefln("nurbs.edge u=%.17g x=%.17g y=%.17g z=%.17g", just_over, pio.x, pio.y, pio.z);
        double just_under = 0.0 - 1e-13;
        auto piu = nc(just_under);
        writefln("nurbs.edge u=%.17g x=%.17g y=%.17g z=%.17g", just_under, piu.x, piu.y, piu.z);
        // Well outside: findSpan throws an Exception (not an Error).
        try {
            auto pbad = nc(1.5);
            writefln("nurbs.outside x=%.17g y=%.17g z=%.17g", pbad.x, pbad.y, pbad.z);
        } catch (Exception e) {
            writefln("nurbs.outside asError=false asException=true msg=%s", esc(e.msg));
        } catch (Error e) {
            writefln("nurbs.outside asError=true asException=false msg=%s", esc(e.msg));
        }
        try {
            auto pbad = nc(-0.25);
            writefln("nurbs.below x=%.17g y=%.17g z=%.17g", pbad.x, pbad.y, pbad.z);
        } catch (Exception e) {
            writefln("nurbs.below asError=false asException=true msg=%s", esc(e.msg));
        } catch (Error e) {
            writefln("nurbs.below asError=true asException=false msg=%s", esc(e.msg));
        }
        // NaN parameter: every comparison in findSpan is false, so the binary
        // search walks until it reads out of range or terminates. Dumped as-is.
        try {
            auto pn = nc(double.nan);
            writefln("nurbs.nan x=%.17g y=%.17g z=%.17g", pn.x, pn.y, pn.z);
        } catch (Exception e) {
            writefln("nurbs.nan asError=false asException=true msg=%s", esc(e.msg));
        } catch (Error e) {
            writefln("nurbs.nan asError=true asException=false msg=%s", esc(e.msg));
        }
    }
} // end main
