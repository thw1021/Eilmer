// probes/geom/surface/probe_coonspatch.d — what geom/surface/coonspatch.d produces.
//
// CoonsPatch (D:26-112) blends four bounding Paths. Two constructors matter and they
// do different work: the four-corner one (D:31-38) builds four Lines and delegates to
// the four-Path one (D:40-74), which dups each path, derives the corners from the
// paths, and cross-checks every corner against the alternate boundary — throwing an
// `Error` built with text(...) on the first mismatch.
//
// What is measured rather than reasoned:
//  * the boundary pairing of the corner constructor (D:33-36): north = Line(p01,p11),
//    east = Line(p10,p11), south = Line(p00,p10), west = Line(p00,p01). A swapped
//    north/south still yields a plausible-looking surface, so each bounding path is
//    dumped as its own toString() plus its values at t = 0.0, 0.5, 1.0.
//  * the corner *check order* (p00, p10, p11, p01 at D:58-73) — only the first
//    mismatching corner can throw, so four separate boundary sets are built, each with
//    exactly one open corner, and the escaped type plus message text is captured.
//    Error and Exception are siblings in this port, so `threw=` names which one.
//  * the message bodies are text(...) over Vector3 values, i.e. the %.6g form
//    ("Vector3(0, 0.1, 3)"), which is a different formatting rule from the %.17g
//    columns used for the numeric observables.
//  * the defaults of approxEqualVectors (geom/elements/vector3.d, maxRelDiff 1.0e-2 /
//    maxAbsDiff 1.0e-5) decide what counts as open, so a corner is deliberately
//    mismatched by 1.0e-3 and by 1.0e-6 to find the boundary of that rule.
//  * D:76-86 the copy constructor does NOT re-run the corner checks and copies the
//    corners verbatim, while D:88-91 dup() goes back through the checking constructor.
//    Both are dumped, including the toString() of the bounding paths, which is how a
//    missing dup() (aliasing the caller's paths, D:45-48) would show.
//  * opCall (D:93-102) is a bilinear blend minus the corner bilinear; the whole (r,s)
//    sweep is dumped at %.17g, including r/s exactly 0.0 and 1.0 and one point in each
//    parametric quadrant, for a Line-bounded and a Bezier-bounded patch.
//  * area() is the base sum (parametricsurface.d:40-66) at the D defaults (10,10) and
//    an asymmetric (3,7).
//  * the `Vector3 p;` default is NaN (wave-02-brief section 5), so the corner
//    constructor fed default-constructed corners is a real, measured throw path whose
//    message prints "Vector3(nan, nan, nan)" twice.
//  * D:19-23 of geom/surface/surface_demo.d is the CoonsPatch demo; its exact stdout
//    lines are reproduced here as the cnp.demo.* rows (surface_demo.d also touches
//    AOPatch and SubRangedSurface, which are outside this module's scope, so only the
//    CoonsPatch fragment is portable byte-for-byte).
//
// Build and capture (see probes/geom/surface/probe_parametricsurface.d for why the
// reference tree is copied with @nogc stripped):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface \
//     probes/geom/surface/probe_coonspatch.d -of=/tmp/probe_cnp -od/tmp/probe_obj_w2_surfA
//   /tmp/probe_cnp > baselines/e587b235/probes/geom_surface_coonspatch.txt
import std.array;
import std.conv;
import std.format;
import std.math;
import std.stdio;

import geom.elements;
import geom.gpath;
import geom.surface.coonspatch;
import geom.surface.parametricsurface;
import nm.number;

/// Dump the members of a patch: corners, and each bounding path as text + class + the
/// three t values that identify which corners it was built from.
void dump_one_bound(const char[] tag, const char[] name, Path b) {
    writefln("%s %s class=%s str=%s", tag, name, b.classString(), b.toString());
    foreach (t; [0.0, 0.5, 1.0]) {
        auto p = b(t);
        writefln("%s %s t=%.17g x=%.17g y=%.17g z=%.17g", tag, name, t, p.x, p.y, p.z);
    }
}

void dump_patch(const char[] tag, CoonsPatch cp) {
    writefln("%s str=%s", tag, cp.toString());
    writefln("%s classname=%s", tag, split(to!string(typeid(cp).name), ".")[$-1]);
    writefln("%s p00 x=%.17g y=%.17g z=%.17g", tag, cp.p00.x, cp.p00.y, cp.p00.z);
    writefln("%s p10 x=%.17g y=%.17g z=%.17g", tag, cp.p10.x, cp.p10.y, cp.p10.z);
    writefln("%s p11 x=%.17g y=%.17g z=%.17g", tag, cp.p11.x, cp.p11.y, cp.p11.z);
    writefln("%s p01 x=%.17g y=%.17g z=%.17g", tag, cp.p01.x, cp.p01.y, cp.p01.z);
    dump_one_bound(tag, "south", cp.south);
    dump_one_bound(tag, "north", cp.north);
    dump_one_bound(tag, "west", cp.west);
    dump_one_bound(tag, "east", cp.east);
}

void dump_sweep(const char[] tag, ParametricSurface ps) {
    foreach (r; [0.0, 0.2, 0.5, 0.75, 1.0]) {
        foreach (s; [0.0, 0.25, 0.5, 0.75, 1.0]) {
            auto p = ps(r, s);
            writefln("%s opCall r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g", tag, r, s, p.x, p.y,
                     p.z);
        }
    }
}

void dump_area(const char[] tag, ParametricSurface ps, int nr, int ns) {
    auto a = ps.area(nr, ns);
    writefln("%s area nr=%d ns=%d x=%.17g y=%.17g z=%.17g", tag, nr, ns, a.x, a.y, a.z);
}

void dump_throw(const char[] tag, void delegate() dg) {
    try {
        dg();
        writefln("%s threw=none", tag);
    } catch (Error e) {
        writefln("%s threw=Error msg=%s", tag, e.msg);
    } catch (Exception e) {
        writefln("%s threw=Exception msg=%s", tag, e.msg);
    }
}

void main()
{
    //--- group A: the corner constructor, on the D unittest's four corners -------
    auto p00 = Vector3([0.0, 0.1, 3.0]);
    auto p10 = Vector3(1.0, 0.1, 3.0);
    auto p11 = Vector3(1.0, 1.1, 3.0);
    auto p01 = Vector3(0.0, 1.1, 3.0);
    auto cp = new CoonsPatch(p00, p10, p11, p01);
    dump_patch("cnp.corners", cp);
    dump_sweep("cnp.corners", cp);
    dump_area("cnp.corners", cp, 10, 10);
    dump_area("cnp.corners", cp, 3, 7);
    dump_area("cnp.corners", cp, 1, 1);

    // D:88-91 dup() re-enters the checking constructor from the (already duped) paths.
    auto dp = cp.dup();
    dump_patch("cnp.dup", dp);
    dump_sweep("cnp.dup", dp);
    dump_area("cnp.dup", dp, 10, 10);

    // D:76-86 the copy constructor: no corner checks, corners copied verbatim.
    auto cpc = new CoonsPatch(cp);
    dump_patch("cnp.copy", cpc);
    dump_sweep("cnp.copy", cpc);
    dump_area("cnp.copy", cpc, 10, 10);

    //--- group B: a Bezier-bounded patch (the four-Path constructor) -------------
    // Bounding curves chosen so all four corners agree exactly.
    Path s = new Bezier([Vector3(0.0, 0.0, 0.0), Vector3(0.33, -0.2, 0.1),
                         Vector3(0.67, -0.2, 0.1), Vector3(1.0, 0.0, 0.0)]);
    Path n = new Bezier([Vector3(0.0, 1.0, 0.0), Vector3(0.33, 1.2, 0.1),
                         Vector3(0.67, 1.2, 0.1), Vector3(1.0, 1.0, 0.0)]);
    Path w = new Bezier([Vector3(0.0, 0.0, 0.0), Vector3(-0.15, 0.33, 0.1),
                         Vector3(-0.15, 0.67, 0.1), Vector3(0.0, 1.0, 0.0)]);
    Path e = new Bezier([Vector3(1.0, 0.0, 0.0), Vector3(1.15, 0.33, 0.1),
                         Vector3(1.15, 0.67, 0.1), Vector3(1.0, 1.0, 0.0)]);
    auto bp = new CoonsPatch(s, n, w, e);
    dump_patch("cnp.bezier", bp);
    dump_sweep("cnp.bezier", bp);
    dump_area("cnp.bezier", bp, 10, 10);
    dump_area("cnp.bezier", bp, 3, 7);

    // D:45-48 dup() each path, so mutating the caller's Bezier afterwards must not move
    // the patch's boundaries.
    (cast(Bezier)s).B[1] = Vector3(0.33, -0.2, 5.0);
    dump_patch("cnp.bezier.aftermut", bp);
    writefln("cnp.bezier.callerstr str=%s", s.toString());

    // toString() through a base-class handle, which is what the hierarchy's callers use.
    writefln("cnp.bezier str.as.base=%s", (cast(ParametricSurface)bp).toString());

    //--- group C: the four open-corner throw branches (D:58-73), in check order ---
    // p00 open: south(0.0) != west(0.0), everything else closed.
    {
        Path s2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
        Path n2 = new Line(Vector3(0.1, 1.0, 0.0), Vector3(1.0, 1.0, 0.0));
        Path w2 = new Line(Vector3(0.1, 0.0, 0.0), Vector3(0.1, 1.0, 0.0));
        Path e2 = new Line(Vector3(1.0, 0.0, 0.0), Vector3(1.0, 1.0, 0.0));
        dump_throw("cnp.open.p00", () { new CoonsPatch(s2, n2, w2, e2); });
    }
    // p10 open only.
    {
        Path s2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
        Path n2 = new Line(Vector3(0.0, 1.0, 0.0), Vector3(1.0, 1.0, 0.0));
        Path w2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(0.0, 1.0, 0.0));
        Path e2 = new Line(Vector3(1.0, 0.1, 0.0), Vector3(1.0, 1.0, 0.0));
        dump_throw("cnp.open.p10", () { new CoonsPatch(s2, n2, w2, e2); });
    }
    // p11 open only.
    {
        Path s2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
        Path n2 = new Line(Vector3(0.0, 1.0, 0.0), Vector3(1.0, 0.9, 0.0));
        Path w2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(0.0, 1.0, 0.0));
        Path e2 = new Line(Vector3(1.0, 0.0, 0.0), Vector3(1.0, 1.0, 0.0));
        dump_throw("cnp.open.p11", () { new CoonsPatch(s2, n2, w2, e2); });
    }
    // p01 open only.
    {
        Path s2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
        Path n2 = new Line(Vector3(0.1, 1.0, 0.0), Vector3(1.0, 1.0, 0.0));
        Path w2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(0.0, 1.0, 0.0));
        Path e2 = new Line(Vector3(1.0, 0.0, 0.0), Vector3(1.0, 1.0, 0.0));
        dump_throw("cnp.open.p01", () { new CoonsPatch(s2, n2, w2, e2); });
    }
    // all four open at once: only the p00 branch can report.
    {
        Path s2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
        Path n2 = new Line(Vector3(0.4, 1.0, 0.0), Vector3(1.0, 0.4, 0.0));
        Path w2 = new Line(Vector3(0.3, 0.1, 0.0), Vector3(0.2, 1.0, 0.0));
        Path e2 = new Line(Vector3(1.1, 0.2, 0.0), Vector3(1.0, 1.1, 0.0));
        dump_throw("cnp.open.all", () { new CoonsPatch(s2, n2, w2, e2); });
    }

    //--- group D: where approxEqualVectors' tolerance stops the check ------------
    // defaults are maxRelDiff 1.0e-2, maxAbsDiff 1.0e-5 (geom/elements/vector3.d).
    // 1.0e-3 off an origin corner: absolute tolerance wins -> must throw.
    {
        Path s2 = new Line(Vector3(1.0e-3, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
        Path n2 = new Line(Vector3(0.0, 1.0, 0.0), Vector3(1.0, 1.0, 0.0));
        Path w2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(0.0, 1.0, 0.0));
        Path e2 = new Line(Vector3(1.0, 0.0, 0.0), Vector3(1.0, 1.0, 0.0));
        dump_throw("cnp.tol.1e-3", () { new CoonsPatch(s2, n2, w2, e2); });
    }
    // 1.0e-6 off: inside maxAbsDiff -> must not throw.
    {
        Path s2 = new Line(Vector3(1.0e-6, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
        Path n2 = new Line(Vector3(0.0, 1.0, 0.0), Vector3(1.0, 1.0, 0.0));
        Path w2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(0.0, 1.0, 0.0));
        Path e2 = new Line(Vector3(1.0, 0.0, 0.0), Vector3(1.0, 1.0, 0.0));
        dump_throw("cnp.tol.1e-6", () { new CoonsPatch(s2, n2, w2, e2); });
    }
    // 1.5e-2 at a corner of magnitude 1.0: outside both tolerances -> must throw.
    {
        Path s2 = new Line(Vector3(1.0, 0.0, 0.0), Vector3(1.015, 0.0, 0.0));
        Path n2 = new Line(Vector3(0.0, 1.0, 0.0), Vector3(1.0, 1.0, 0.0));
        Path w2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(0.0, 1.0, 0.0));
        Path e2 = new Line(Vector3(1.0, 0.0, 0.0), Vector3(1.0, 1.0, 0.0));
        dump_throw("cnp.tol.1.5e-2", () { new CoonsPatch(s2, n2, w2, e2); });
    }
    // The relative half of approxEqualNumbers, isolated at the p10 branch: 5.0e-3 is
    // outside maxAbsDiff (1.0e-5) but inside maxRelDiff (1.0e-2 of a magnitude-1 corner),
    // so only the relative term can let this through. It also pins which value the
    // stored corner keeps (D:50 takes south(1.0), not east(0.0)).
    {
        Path s2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
        Path n2 = new Line(Vector3(0.0, 1.0, 0.0), Vector3(1.0, 1.0, 0.0));
        Path w2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(0.0, 1.0, 0.0));
        Path e2 = new Line(Vector3(1.005, 0.0, 0.0), Vector3(1.0, 1.0, 0.0));
        dump_throw("cnp.reltol.5e-3", () { new CoonsPatch(s2, n2, w2, e2); });
        auto okp = new CoonsPatch(s2, n2, w2, e2);
        writefln("cnp.reltol.5e-3.p10 x=%.17g y=%.17g z=%.17g", okp.p10.x, okp.p10.y, okp.p10.z);
        writefln("cnp.reltol.5e-3.east t=%.17g x=%.17g y=%.17g z=%.17g", 0.0, okp.east(0.0).x,
                 okp.east(0.0).y, okp.east(0.0).z);
    }
    // 1.5e-2 at the same corner, i.e. outside both terms, reported at p10.
    {
        Path s2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
        Path n2 = new Line(Vector3(0.0, 1.0, 0.0), Vector3(1.0, 1.0, 0.0));
        Path w2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(0.0, 1.0, 0.0));
        Path e2 = new Line(Vector3(1.015, 0.0, 0.0), Vector3(1.0, 1.0, 0.0));
        dump_throw("cnp.reltol.1.5e-2", () { new CoonsPatch(s2, n2, w2, e2); });
    }

    //--- group E: default-constructed (NaN) corners ------------------------------
    dump_throw("cnp.nan.corners", () { new CoonsPatch(Vector3(), Vector3(), Vector3(), Vector3()); });

    //--- group F: degenerate but legal patches -----------------------------------
    // four coincident corners -> every path is a point, area() sums zero-area quads.
    auto pt = new CoonsPatch(Vector3(1.0, 2.0, 3.0), Vector3(1.0, 2.0, 3.0),
                             Vector3(1.0, 2.0, 3.0), Vector3(1.0, 2.0, 3.0));
    dump_patch("cnp.point", pt);
    dump_sweep("cnp.point", pt);
    dump_area("cnp.point", pt, 10, 10);

    // a non-planar, sheared set of corners (the corner constructor's Lines are still
    // exact, so this measures the blend's corner bilinear term).
    auto sk = new CoonsPatch(Vector3(0.5, -0.25, 0.1), Vector3(2.0, 0.75, -0.3),
                             Vector3(1.25, 2.5, 0.05), Vector3(-0.5, 1.5, 0.9));
    dump_patch("cnp.skew", sk);
    dump_sweep("cnp.skew", sk);
    dump_area("cnp.skew", sk, 10, 10);
    dump_area("cnp.skew", sk, 3, 7);

    //--- group G: the CoonsPatch fragment of geom/surface/surface_demo.d (D:14-23) -
    {
        auto d00 = Vector3(0.0, 0.1, 3.0);
        auto d10 = Vector3(1.0, 0.1, 3.0);
        auto d11 = Vector3(1.0, 1.1, 3.0);
        auto d01 = Vector3(0.0, 1.1, 3.0);
        writefln("cnp.demo head=%s", "CoonsPatch demo");
        auto my_patch = new CoonsPatch(d00, d10, d11, d01);
        writefln("cnp.demo patch=%s", "my_patch= " ~ to!string(my_patch));
        auto c = my_patch(0.1, 0.1);
        writefln("cnp.demo c=%s", "my_patch(0.1, 0.1)= " ~ to!string(c));
        // the second demo point, with p10 moved as the AOPatch part of the demo does
        d10 = Vector3(1.0, 0.4, 3.0);
        auto my_patch2 = new CoonsPatch(d00, d10, d11, d01);
        writefln("cnp.demo patch2=%s", "my_patch= " ~ to!string(my_patch2));
    }
}
