// probes/geom/surface/probe_bezierpatch.d — what geom/surface/bezierpatch.d produces.
//
// BezierPatch (D:18-80) is a tensor-product Bezier surface: n+1 Bezier curves in the
// s direction (_b1s, private, D:79) whose points at a given s form the control net of a
// Bezier curve in r (D:53-63). It has no unittest-visible guards of its own, so every
// failure it can produce comes from the geom/gpath Bezier it builds — which is exactly
// why this probe drives Bezier construction through it rather than trusting Bezier's
// own corpus.
//
// What is measured rather than reasoned:
//  * the copy constructor (D:37-46) writes `m = other.n;` — NOT other.m. Measured here
//    from a net with n=3, m=4 so the two differ; a "obvious fix" in the port would
//    change every observable that reads m and is forbidden by wave-02-brief 0.1.
//  * opCall's evaluation order (D:55-62): B[i] = _b1s[i](s) then Bezier(B)(r). The
//    de Casteljau arithmetic depends on that order, so the whole (r,s) sweep is dumped
//    at %.17g, including r/s exactly 0.0 and 1.0 and one point in each quadrant.
//  * Q is a jagged-capable nested array: Q.length is forced to _n+1 (D:30) while the
//    inner lengths come from the caller, so lengths and every point are dumped.
//  * area() is the base-class sum (parametricsurface.d:40-66) driven through this
//    subclass at the D defaults (10,10) and an asymmetric (3,7); a second, genuinely
//    non-planar net is used so all three components are non-zero.
//  * the throw paths print WHICH type escaped (Error and Exception are siblings here)
//    plus the escaped message text: n=0 and n=-1 make `new Bezier(B)` throw from opCall
//    (D:61), an inner net of one point makes initialiseWorkingSpace() throw from the
//    constructor (D:74), a short outer net makes `_Q[i]` raise RangeError (D:32), and a
//    negative _n makes `Q.length = _n+1` raise whatever Phobos raises (D:30).
//  * writeCtrlPtsAsVtkXml (D:82-105) writes n = Q.length and m = Q[0].length and then
//    indexes Q[i][j] with that j bound, so a jagged net reads out of bounds — dumped,
//    not avoided. The whole file body is dumped, and so is the text of a failed open.
//
// Build and capture (see probes/geom/surface/probe_parametricsurface.d for why the
// reference tree is copied with @nogc stripped):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface \
//     probes/geom/surface/probe_bezierpatch.d -of=/tmp/probe_bzp -od/tmp/probe_obj_w2_surfA
//   /tmp/probe_bzp > baselines/e587b235/probes/geom_surface_bezierpatch.txt
import std.array;
import std.conv;
import std.file;
import std.format;
import std.math;
import std.stdio;

import geom.elements;
import geom.gpath;
import geom.surface.bezierpatch;
import geom.surface.parametricsurface;
import nm.number;

/// The control net the D unittest builds (D:108-121): n=3, m=4, L=2.0, H=1.0.
Vector3[][] make_unit_net() {
    double L = 2.0;
    double H = 1.0;
    int n = 3;
    int m = 4;
    double dx = L / n;
    double dy = H / m;
    Vector3[][] Q;
    Q.length = n + 1;
    foreach (i; 0 .. n + 1) {
        Q[i].length = m + 1;
        foreach (j; 0 .. m + 1) {
            Q[i][j] = Vector3(i * dx, j * dy, 0.0);
        }
    }
    return Q;
}

/// A genuinely non-planar net (n=2, m=3) so area() has three non-zero components.
Vector3[][] make_twist_net() {
    Vector3[][] Q;
    Q.length = 3;
    foreach (i; 0 .. 3) {
        Q[i].length = 4;
        foreach (j; 0 .. 4) {
            Q[i][j] = Vector3(0.3 * i - 0.11 * j, 0.25 * j + 0.07 * i,
                              0.13 * i * j - 0.05 * (i + j) + 0.02);
        }
    }
    return Q;
}

/// Members, dumped through the public surface of the class.
void dump_members(const char[] tag, BezierPatch bp) {
    writefln("%s str=%s", tag, bp.toString());
    writefln("%s classname=%s", tag, split(to!string(typeid(bp).name), ".")[$-1]);
    writefln("%s n=%d m=%d Q.length=%d", tag, bp.n, bp.m, bp.Q.length);
    foreach (i; 0 .. bp.Q.length) {
        writefln("%s Q[%d].length=%d", tag, i, bp.Q[i].length);
        foreach (j; 0 .. bp.Q[i].length) {
            writefln("%s Q[%d][%d] x=%.17g y=%.17g z=%.17g", tag, i, j, bp.Q[i][j].x,
                     bp.Q[i][j].y, bp.Q[i][j].z);
        }
    }
}

void dump_sweep(const char[] tag, ParametricSurface ps) {
    foreach (r; [0.0, 0.2, 0.5, 0.9, 1.0]) {
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

/// Print which type escaped and its message text. Error and Exception are siblings in
/// D, so the catch order below is a real test of which branch the throw took.
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

string one_line(string path) {
    return join(split(cast(string)read(path), "\n"), "<newline>");
}

void main()
{
    //--- group A: the D unittest net, its copy ctor, and dup() -------------------
    auto Q = make_unit_net();
    auto bp = new BezierPatch(Q, 3, 4);
    dump_members("bzp.unit", bp);
    dump_sweep("bzp.unit", bp);
    dump_area("bzp.unit", bp, 10, 10);
    dump_area("bzp.unit", bp, 3, 7);

    // D:37-46 — the copy constructor, including `m = other.n`.
    auto cp = new BezierPatch(bp);
    dump_members("bzp.copy", cp);
    dump_sweep("bzp.copy", cp);
    dump_area("bzp.copy", cp, 10, 10);

    // D:48-51 — dup() goes through the three-argument constructor, so m survives here.
    auto dp = bp.dup();
    dump_members("bzp.dup", dp);
    dump_sweep("bzp.dup", dp);
    dump_area("bzp.dup", dp, 10, 10);

    // mutating the caller's net afterwards must not reach the patch (D:32 uses .dup)
    Q[0][0] = Vector3(99.0, 98.0, 97.0);
    writefln("bzp.aliaschk Q[0][0] x=%.17g y=%.17g z=%.17g", bp.Q[0][0].x, bp.Q[0][0].y,
             bp.Q[0][0].z);

    //--- group B: a non-planar net ------------------------------------------------
    auto Qt = make_twist_net();
    auto tw = new BezierPatch(Qt, 2, 3);
    dump_members("bzp.twist", tw);
    dump_sweep("bzp.twist", tw);
    dump_area("bzp.twist", tw, 10, 10);
    dump_area("bzp.twist", tw, 3, 7);
    dump_area("bzp.twist", tw, 1, 1);
    writefln("bzp.twist str.as.base=%s", (cast(ParametricSurface)tw).toString());

    //--- group C: jagged nets ----------------------------------------------------
    // inner arrays of unequal length: legal to build, and opCall still works because
    // only _b1s (one Bezier per row) is read.
    Vector3[][] jg;
    jg.length = 3;
    jg[0].length = 4;
    jg[1].length = 2;
    jg[2].length = 3;
    foreach (i; 0 .. 3) {
        foreach (j; 0 .. jg[i].length) {
            jg[i][j] = Vector3(0.5 * i + 0.25 * j, 1.0 - 0.3 * i, 0.1 * j - 0.05 * i);
        }
    }
    auto jp = new BezierPatch(jg, 2, 3);
    dump_members("bzp.jagged", jp);
    dump_sweep("bzp.jagged", jp);
    dump_area("bzp.jagged", jp, 10, 10);

    //--- group D: constructor and evaluation guards ------------------------------
    // n = 0: construction succeeds (one s-direction curve), opCall builds a Bezier
    // from a single control point and Bezier.d:22 throws an Error.
    Vector3[][] q0;
    q0.length = 1;
    q0[0].length = 3;
    foreach (j; 0 .. 3) q0[0][j] = Vector3(0.5 * j, 0.25 * j, 0.0);
    auto p0 = new BezierPatch(q0, 0, 2);
    dump_members("bzp.n0", p0);
    dump_throw("bzp.n0.opCall", () { p0(0.5, 0.5); });
    dump_throw("bzp.n0.area", () { p0.area(); });

    // n = -1: Q.length = 0, so opCall's B is empty and Bezier.d:19 throws.
    Vector3[][] qm1;
    dump_throw("bzp.nm1.ctor", () { new BezierPatch(qm1, -1, 0); });
    auto pm1 = new BezierPatch(qm1, -1, 0);
    dump_members("bzp.nm1", pm1);
    dump_throw("bzp.nm1.opCall", () { pm1(0.3, 0.7); });
    dump_throw("bzp.nm1.area", () { pm1.area(); });

    // n = -2: `Q.length = _n+1` is a negative length assignment.
    dump_throw("bzp.nm2.ctor", () { new BezierPatch(qm1, -2, 0); });

    // an inner net with a single point: initialiseWorkingSpace() throws (D:74).
    Vector3[][] one;
    one.length = 2;
    one[0].length = 1;
    one[1].length = 1;
    one[0][0] = Vector3(1.0, 2.0, 3.0);
    one[1][0] = Vector3(4.0, 5.0, 6.0);
    dump_throw("bzp.inner1.ctor", () { new BezierPatch(one, 1, 0); });

    // empty inner net: Bezier.d:19 "No control points present."
    Vector3[][] empty_inner;
    empty_inner.length = 2;
    dump_throw("bzp.inner0.ctor", () { new BezierPatch(empty_inner, 1, 1); });

    // outer net shorter than _n+1: `_Q[i]` (D:32) is out of bounds.
    Vector3[][] shortQ;
    shortQ.length = 2;
    shortQ[0].length = 2;
    shortQ[1].length = 2;
    foreach (i; 0 .. 2) foreach (j; 0 .. 2) shortQ[i][j] = Vector3(i + j, i - j, 0.0);
    dump_throw("bzp.short.ctor", () { new BezierPatch(shortQ, 4, 1); });

    // _m is never read by opCall (only _n and the stored net are), so a patch whose m is
    // larger than any inner array still evaluates. Dumped to pin that down.
    auto sp = new BezierPatch(shortQ, 1, 9);
    dump_members("bzp.mismatch", sp);
    dump_sweep("bzp.mismatch", sp);
    // but _n larger than the caller's outer array is an out-of-bounds read of _Q[i].
    dump_throw("bzp.overlong.ctor", () { new BezierPatch(shortQ, 2, 9); });

    //--- group E: writeCtrlPtsAsVtkXml -------------------------------------------
    string f1 = "/tmp/probe_bzp_unit.vtk";
    writeCtrlPtsAsVtkXml(bp, f1);
    writefln("bzp.vtk.unit len=%d", (cast(string)read(f1)).length);
    writefln("bzp.vtk.unit.body=%s", one_line(f1));

    string f2 = "/tmp/probe_bzp_twist.vtk";
    writeCtrlPtsAsVtkXml(tw, f2);
    writefln("bzp.vtk.twist len=%d", (cast(string)read(f2)).length);
    writefln("bzp.vtk.twist.body=%s", one_line(f2));

    // jagged: m = Q[0].length = 4 but Q[1] only has 2 points -> out of bounds read.
    dump_throw("bzp.vtk.jagged", () { writeCtrlPtsAsVtkXml(jp, "/tmp/probe_bzp_jag.vtk"); });

    // empty net: `bPatch.Q[0]` (D:85) is out of bounds.
    dump_throw("bzp.vtk.empty", () { writeCtrlPtsAsVtkXml(pm1, "/tmp/probe_bzp_empty.vtk"); });

    // unusable path: std.stdio.File raises, and the message text is what a user sees.
    dump_throw("bzp.vtk.badpath",
               () { writeCtrlPtsAsVtkXml(bp, "/nonexistent-dir-xyz/a.vtk"); });

    foreach (f; [f1, f2, "/tmp/probe_bzp_jag.vtk", "/tmp/probe_bzp_empty.vtk"]) {
        if (exists(f)) remove(f);
    }
}
