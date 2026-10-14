// probes/geom/surface/probe_aopatch.d — what geom/surface/aopatch.d produces.
//
// AOPatch (D:17-228) is a Coons surface re-fitted through an area-orthogonal (Knupp
// 1992) background mesh: it holds four bounding Paths, the four corners, a private
// CoonsPatch used as a fall-back near the boundaries, private _nx/_ny and a private
// (nx+1)x(ny+1) mesh that a Jacobi-style iteration moves.
//
// What is measured rather than reasoned:
//  * the private mesh is NOT directly observable, so it is recovered through opCall:
//    the bilinear blend of D:130-133 is dumped on a sweep that includes every mesh node
//    position r = dXi*ix, s = dEta*iy. Those are exactly the parameter values the D
//    constructor used to BUILD the mesh (D:158-159), so a row where local_r/local_s is
//    not 0 exposes the rounding of (r - dr*ix)/dr and of to!int(r/dr) at D:124-125.
//  * the TFI fall-back (D:114-118) triggers on `r < tol || s < tol || r > 1-tol ||
//    s > 1-tol` with tol = 1.0e-4. The comparison is strict, so r == 1.0e-4 exactly uses
//    the MESH, and the next representable double below it uses the Coons patch. Both
//    sides of all four boundaries are dumped, and each is compared against an
//    independently constructed CoonsPatch so the branch choice is a measured equality,
//    not an assumption (rows ao.tolbranch.*).
//  * D:90-102 the copy constructor copies the four paths, the four corners and
//    tfi_surface but NEVER assigns _nx/_ny, so they stay int.init == 0. That is a real
//    `.init` state left behind by a constructor, and it is observable three ways:
//    toString() prints nx=0, ny=0; compute_background_mesh() then runs with dXi = 1.0/0
//    == +inf, so the single node is tfi_surface(inf*0, inf*0) = tfi_surface(nan, nan);
//    and opCall at an interior parameter indexes _bgmesh[0][1], one past a length-1
//    array. Which exception type escapes with which text is captured verbatim
//    (rows ao.copy.*), including the base area(), which calls opCall.
//  * D:104-108 dup() re-enters the FULL constructor with this._nx/this._ny preserved, so
//    dup() and the copy constructor give different objects. Dumped as a pair.
//  * D:64-79 the four open-corner guards throw `new Error(text(...))` in the order p00,
//    p10, p11, p01, so only the first open corner reports. Error is the SIBLING of
//    Exception in this port and each row names which type escaped. The message is built
//    from Vector3 values, i.e. the %.6g form.
//  * D:223-225 `if (count > max_count) writeln("Warning: AO iteration did not converge.")`
//    writes to STDOUT during construction, so it is part of the captured text. It is
//    provoked with a large-coordinate patch (the moves are ~1e6 and x_tol is 1e-6), and
//    the row order shows the warning landing between the surrounding writeln rows.
//  * the AO iteration (D:174-222) is only defined in the (x,y)-plane; z keeps its TFI
//    value (D:164-165 comment). Patches with a non-constant z are dumped so that the
//    untouched z is observable.
//  * D:181-213 reads and writes `_bgmesh[][] .x .re` — `.re` on a double is an identity
//    (measured: ldc2 accepts `double.re` and gives the same bits), so eil::re() is the
//    port's spelling; `_bgmesh[ix][iy].x = numer/denom` writes the field directly.
//  * D:36-44 the corner constructor builds the four Lines and DELEGATES, so it inherits
//    the corner checks; the pairing is D's (north=Line(p01,p11), east=Line(p10,p11),
//    south=Line(p00,p10), west=Line(p00,p01)).
//  * area() (the base sum) at the D defaults (10,10), asymmetric (3,7) and (1,1).
//  * D:46-52 dup() each caller path, so mutating the caller's Bezier afterwards must not
//    move the patch (rows ao.bez.aftermut / ao.bez.callerstr).
//
// Build and capture (see probes/geom/surface/probe_parametricsurface.d for why the
// reference tree is copied with @nogc stripped):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface \
//     probes/geom/surface/probe_aopatch.d -of=/tmp/probe_ao -od/tmp/probe_obj_w2_surf2
//   /tmp/probe_ao > baselines/e587b235/probes/geom_surface_aopatch.txt
import std.array;
import std.conv;
import std.format;
import std.math;
import std.stdio;

import geom.elements;
import geom.gpath;
import geom.surface.aopatch;
import geom.surface.coonspatch;
import geom.surface.parametricsurface;
import nm.number;

void dump_one_bound(const char[] tag, const char[] name, Path b) {
    writefln("%s %s class=%s str=%s", tag, name, b.classString(), b.toString());
    foreach (t; [0.0, 0.5, 1.0]) {
        auto p = b(t);
        writefln("%s %s t=%.17g x=%.17g y=%.17g z=%.17g", tag, name, t, p.x, p.y, p.z);
    }
}

void dump_patch(const char[] tag, AOPatch ap) {
    writefln("%s str=%s", tag, ap.toString());
    writefln("%s classname=%s", tag, split(to!string(typeid(ap).name), ".")[$-1]);
    writefln("%s p00 x=%.17g y=%.17g z=%.17g", tag, ap.p00.x, ap.p00.y, ap.p00.z);
    writefln("%s p10 x=%.17g y=%.17g z=%.17g", tag, ap.p10.x, ap.p10.y, ap.p10.z);
    writefln("%s p11 x=%.17g y=%.17g z=%.17g", tag, ap.p11.x, ap.p11.y, ap.p11.z);
    writefln("%s p01 x=%.17g y=%.17g z=%.17g", tag, ap.p01.x, ap.p01.y, ap.p01.z);
    dump_one_bound(tag, "south", ap.south);
    dump_one_bound(tag, "north", ap.north);
    dump_one_bound(tag, "west", ap.west);
    dump_one_bound(tag, "east", ap.east);
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

/// Sweep every mesh node position: r = ix/nx, s = iy/ny. These are the parameter values
/// the constructor used to build the mesh, so each row reports how far the run-time
/// recomputation of ix_coarse/local_r/local_s lands from the node it is meant to hit.
void dump_nodes(const char[] tag, AOPatch ap, int nx, int ny) {
    double dr = 1.0 / nx;
    double ds = 1.0 / ny;
    foreach (ix; 0 .. nx+1) {
        foreach (iy; 0 .. ny+1) {
            double r = dr * ix;
            double s = ds * iy;
            auto p = ap(r, s);
            writefln("%s node ix=%d iy=%d r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g",
                     tag, ix, iy, r, s, p.x, p.y, p.z);
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
        writefln("%s threw=Error type=%s msg=%s", tag, e.classinfo.name, e.msg);
    } catch (Exception e) {
        writefln("%s threw=Exception type=%s msg=%s", tag, e.classinfo.name, e.msg);
    }
}

void main()
{
    //--- group A: the D unittest's patch, default nx=ny=10 (D:230-240) -----------
    auto q00 = Vector3([0.0, 0.1, 3.0]);
    auto q10 = Vector3(1.0, 0.4, 3.0);
    auto q11 = Vector3(1.0, 1.1, 3.0);
    auto q01 = Vector3(0.0, 1.1, 3.0);
    auto ap = new AOPatch(q00, q10, q11, q01);
    dump_patch("ao.ut", ap);
    dump_sweep("ao.ut", ap);
    dump_nodes("ao.ut", ap, 10, 10);
    dump_area("ao.ut", ap, 10, 10);
    dump_area("ao.ut", ap, 3, 7);
    dump_area("ao.ut", ap, 1, 1);
    // the two points the D unittest asserts on (D:236-239)
    {
        auto c1 = ap(0.5, 0.5);
        auto c2 = ap(0.1, 0.1);
        writefln("ao.ut.assert c1 x=%.17g y=%.17g z=%.17g", c1.x, c1.y, c1.z);
        writefln("ao.ut.assert c2 x=%.17g y=%.17g z=%.17g", c2.x, c2.y, c2.z);
        writefln("ao.ut.assert c1str=%s", to!string(c1));
        writefln("ao.ut.assert c2str=%s", to!string(c2));
        writefln("ao.ut.assert approx c1=%s",
                 approxEqualVectors(c1, Vector3(0.443775, 0.652291, 3.0)));
        writefln("ao.ut.assert approx c2=%s",
                 approxEqualVectors(c2, Vector3(0.0892476, 0.215529, 3.0)));
    }
    writefln("ao.ut str.as.base=%s", (cast(ParametricSurface)ap).toString());
    // dup() re-enters the full constructor with _nx/_ny preserved (D:104-108).
    auto dp = ap.dup();
    dump_patch("ao.dup", dp);
    dump_sweep("ao.dup", dp);
    dump_area("ao.dup", dp, 10, 10);
    // the copy constructor does NOT assign _nx/_ny (D:90-102).
    auto cp = new AOPatch(ap);
    dump_patch("ao.copy", cp);
    dump_throw("ao.copy.sweep", () {
        foreach (r; [0.2, 0.5, 0.75]) {
            foreach (s; [0.25, 0.5, 0.75]) {
                auto p = cp(r, s);
                writefln("ao.copy interior r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g",
                         r, s, p.x, p.y, p.z);
            }
        }
    });
    // the boundary side of the copy still takes the TFI fall-back.
    {
        auto p = cp(0.0, 0.0);
        writefln("ao.copy corner r=0 s=0 x=%.17g y=%.17g z=%.17g", p.x, p.y, p.z);
        auto p2 = cp(0.5, 0.0);
        writefln("ao.copy corner r=0.5 s=0 x=%.17g y=%.17g z=%.17g", p2.x, p2.y, p2.z);
    }
    dump_throw("ao.copy.area", () { dump_area("ao.copy", cp, 10, 10); });

    //--- group B: the fall-back boundary, tol = 1.0e-4 ---------------------------
    {
        double tol = 1.0e-4;
        double hi = 1.0 - tol;
        double[] rs = [0.0, nextafter(tol, -double.infinity), 9.9999e-5, tol,
                       nextafter(tol, double.infinity), 1.0e-3, 0.5, 1.0-1.0e-3,
                       nextafter(hi, -double.infinity), hi,
                       nextafter(hi, double.infinity), 1.0];
        CoonsPatch refp = new CoonsPatch(ap.south, ap.north, ap.west, ap.east);
        foreach (r; rs) {
            auto pa = ap(r, 0.5);
            auto pr = refp(r, 0.5);
            writefln("ao.tolbranch r=%.17g below=%s mesh x=%.17g y=%.17g z=%.17g tfi x=%.17g y=%.17g z=%.17g same=%s",
                     r, r < tol || r > hi, pa.x, pa.y, pa.z, pr.x, pr.y, pr.z,
                     (pa.x == pr.x) && (pa.y == pr.y) && (pa.z == pr.z));
        }
        foreach (s; rs) {
            auto pa = ap(0.5, s);
            auto pr = refp(0.5, s);
            writefln("ao.tolbranch s=%.17g below=%s mesh x=%.17g y=%.17g z=%.17g tfi x=%.17g y=%.17g z=%.17g same=%s",
                     s, s < tol || s > hi, pa.x, pa.y, pa.z, pr.x, pr.y, pr.z,
                     (pa.x == pr.x) && (pa.y == pr.y) && (pa.z == pr.z));
        }
    }

    //--- group C: nx/ny variants on the same corners ----------------------------
    {
        auto p35 = new AOPatch(q00, q10, q11, q01, 3, 5);
        dump_patch("ao.nx3ny5", p35);
        dump_nodes("ao.nx3ny5", p35, 3, 5);
        dump_sweep("ao.nx3ny5", p35);
        dump_area("ao.nx3ny5", p35, 10, 10);
        dump_area("ao.nx3ny5", p35, 3, 7);
        dump_area("ao.nx3ny5", p35, 1, 1);
        auto pdup = p35.dup();
        dump_patch("ao.nx3ny5.dup", pdup);
        dump_nodes("ao.nx3ny5.dup", pdup, 3, 5);

        auto p11b = new AOPatch(q00, q10, q11, q01, 1, 1);
        dump_patch("ao.nx1ny1", p11b);
        dump_nodes("ao.nx1ny1", p11b, 1, 1);
        dump_sweep("ao.nx1ny1", p11b);
        dump_area("ao.nx1ny1", p11b, 1, 1);

        auto p22 = new AOPatch(q00, q10, q11, q01, 2, 2);
        dump_patch("ao.nx2ny2", p22);
        dump_nodes("ao.nx2ny2", p22, 2, 2);

        auto p207 = new AOPatch(q00, q10, q11, q01, 20, 7);
        dump_patch("ao.nx20ny7", p207);
        dump_area("ao.nx20ny7", p207, 10, 10);
        // a single-cell-wide mesh: the interior loop is empty on one axis
        auto p17 = new AOPatch(q00, q10, q11, q01, 1, 7);
        dump_patch("ao.nx1ny7", p17);
        dump_nodes("ao.nx1ny7", p17, 1, 7);
    }

    //--- group D: a Bezier-bounded patch through the four-Path constructor -------
    {
        Path s = new Bezier([Vector3(0.0, 0.0, 0.0), Vector3(0.33, -0.2, 0.1),
                             Vector3(0.67, -0.2, 0.1), Vector3(1.0, 0.0, 0.0)]);
        Path n = new Bezier([Vector3(0.0, 1.0, 0.2), Vector3(0.33, 1.2, 0.3),
                             Vector3(0.67, 1.2, 0.3), Vector3(1.0, 1.0, 0.2)]);
        Path w = new Bezier([Vector3(0.0, 0.0, 0.0), Vector3(-0.15, 0.33, 0.1),
                             Vector3(-0.15, 0.67, 0.1), Vector3(0.0, 1.0, 0.2)]);
        Path e = new Bezier([Vector3(1.0, 0.0, 0.0), Vector3(1.15, 0.33, 0.1),
                             Vector3(1.15, 0.67, 0.1), Vector3(1.0, 1.0, 0.2)]);
        auto bp = new AOPatch(s, n, w, e, 6, 6);
        dump_patch("ao.bez", bp);
        dump_nodes("ao.bez", bp, 6, 6);
        dump_sweep("ao.bez", bp);
        dump_area("ao.bez", bp, 10, 10);
        dump_area("ao.bez", bp, 3, 7);
        // D:49-52 dup() each caller path: mutating the caller afterwards is invisible.
        (cast(Bezier)s).B[1] = Vector3(0.33, -0.2, 5.0);
        dump_patch("ao.bez.aftermut", bp);
        writefln("ao.bez.callerstr str=%s", s.toString());
        writefln("ao.bez str.as.base=%s", (cast(ParametricSurface)bp).toString());
    }

    //--- group E: the four open-corner throw branches (D:64-79), in check order ---
    {
        Path s2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
        Path n2 = new Line(Vector3(0.1, 1.0, 0.0), Vector3(1.0, 1.0, 0.0));
        Path w2 = new Line(Vector3(0.1, 0.0, 0.0), Vector3(0.1, 1.0, 0.0));
        Path e2 = new Line(Vector3(1.0, 0.0, 0.0), Vector3(1.0, 1.0, 0.0));
        dump_throw("ao.open.p00", () { new AOPatch(s2, n2, w2, e2); });
    }
    {
        Path s2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
        Path n2 = new Line(Vector3(0.0, 1.0, 0.0), Vector3(1.0, 1.0, 0.0));
        Path w2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(0.0, 1.0, 0.0));
        Path e2 = new Line(Vector3(1.0, 0.1, 0.0), Vector3(1.0, 1.0, 0.0));
        dump_throw("ao.open.p10", () { new AOPatch(s2, n2, w2, e2); });
    }
    {
        Path s2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
        Path n2 = new Line(Vector3(0.0, 1.0, 0.0), Vector3(1.0, 0.9, 0.0));
        Path w2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(0.0, 1.0, 0.0));
        Path e2 = new Line(Vector3(1.0, 0.0, 0.0), Vector3(1.0, 1.0, 0.0));
        dump_throw("ao.open.p11", () { new AOPatch(s2, n2, w2, e2); });
    }
    {
        Path s2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
        Path n2 = new Line(Vector3(0.1, 1.0, 0.0), Vector3(1.0, 1.0, 0.0));
        Path w2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(0.0, 1.0, 0.0));
        Path e2 = new Line(Vector3(1.0, 0.0, 0.0), Vector3(1.0, 1.0, 0.0));
        dump_throw("ao.open.p01", () { new AOPatch(s2, n2, w2, e2); });
    }
    {
        Path s2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
        Path n2 = new Line(Vector3(0.4, 1.0, 0.0), Vector3(1.0, 0.4, 0.0));
        Path w2 = new Line(Vector3(0.3, 0.1, 0.0), Vector3(0.2, 1.0, 0.0));
        Path e2 = new Line(Vector3(1.1, 0.2, 0.0), Vector3(1.0, 1.1, 0.0));
        dump_throw("ao.open.all", () { new AOPatch(s2, n2, w2, e2); });
    }
    // 1.0e-6 off a corner is inside approxEqualVectors' maxAbsDiff -> no throw.
    {
        Path s2 = new Line(Vector3(1.0e-6, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
        Path n2 = new Line(Vector3(0.0, 1.0, 0.0), Vector3(1.0, 1.0, 0.0));
        Path w2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(0.0, 1.0, 0.0));
        Path e2 = new Line(Vector3(1.0, 0.0, 0.0), Vector3(1.0, 1.0, 0.0));
        dump_throw("ao.tol.1e-6", () { new AOPatch(s2, n2, w2, e2); });
    }
    // default-constructed (NaN) corners: the guard fires with "Vector3(nan, nan, nan)".
    dump_throw("ao.nan.corners", () {
        new AOPatch(Vector3(), Vector3(), Vector3(), Vector3());
    });
    // the four-corner constructor delegates (D:43), so it runs the same checks.
    dump_throw("ao.corner.delegates", () {
        new AOPatch(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0), Vector3(1.0, 1.0, 0.0),
                    Vector3(0.5, 1.0, 0.0));
    });

    //--- group F: non-convergence, the stdout warning (D:223-225) ---------------
    writeln("ao.warn before");
    {
        auto big = new AOPatch(Vector3(0.0, 0.0, 0.0), Vector3(1.0e6, 0.0, 0.0),
                               Vector3(1.0e6, 1.0e6, 0.0), Vector3(0.0, 1.0e6, 0.0), 4, 4);
        writefln("ao.warn.big str=%s", big.toString());
        dump_sweep("ao.warn.big", big);
    }
    writeln("ao.warn after-big");
    // A 50x50 background mesh needs more than D:172's 500 Jacobi sweeps, so D:224's
    // warning really is emitted. Measured: rows ao.warn50.* / ao.warnbez.* carry the
    // warning line between the surrounding writeln rows, in stream order.
    writeln("ao.warn before-50");
    {
        auto n50 = new AOPatch(q00, q10, q11, q01, 50, 50);
        writefln("ao.warn50 str=%s", n50.toString());
        dump_sweep("ao.warn50", n50);
        auto d = n50.dup();
        writefln("ao.warn50.dup str=%s", d.toString());
    }
    writeln("ao.warn after-50");
    {
        // a sliver: nearly collinear corners
        auto sl = new AOPatch(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 1.0e-9, 0.0),
                              Vector3(2.0, 0.0, 0.0), Vector3(1.0, 1.0e-9, 1.0), 5, 5);
        writefln("ao.sliver str=%s", sl.toString());
        dump_sweep("ao.sliver", sl);
        dump_area("ao.sliver", sl, 10, 10);
    }
    {
        // crossed (bowtie) corner set: TFI self-overlaps, yet the AO iteration still
        // meets the 1e-6 test on a coarse mesh, so no warning is emitted. Measured.
        auto xq = new AOPatch(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 1.0, 0.0),
                              Vector3(1.0, 0.0, 0.0), Vector3(0.0, 1.0, 0.0), 4, 4);
        writefln("ao.crossed str=%s", xq.toString());
        dump_nodes("ao.crossed", xq, 4, 4);
        dump_area("ao.crossed", xq, 10, 10);
    }
    writeln("ao.warn after-sliver");

    //--- group G: degenerate patches -------------------------------------------
    {
        auto pt = new AOPatch(Vector3(1.0, 2.0, 3.0), Vector3(1.0, 2.0, 3.0),
                              Vector3(1.0, 2.0, 3.0), Vector3(1.0, 2.0, 3.0), 4, 4);
        dump_patch("ao.point", pt);
        dump_sweep("ao.point", pt);
        dump_area("ao.point", pt, 10, 10);
        // a flat rectangle in the (x,y)-plane: the AO mesh should stay as fitted, and
        // z is untouched by the iteration (D:164-165).
        auto rc = new AOPatch(Vector3(0.0, 0.0, 7.5), Vector3(2.0, 0.0, 7.5),
                              Vector3(2.0, 1.0, -1.5), Vector3(0.0, 1.0, 2.25), 4, 4);
        dump_patch("ao.rect", rc);
        dump_nodes("ao.rect", rc, 4, 4);
        dump_area("ao.rect", rc, 10, 10);
        dump_area("ao.rect", rc, 3, 7);
        dump_area("ao.rect", rc, 1, 1);
    }

    //--- group H: the AOPatch fragment of geom/surface/surface_demo.d ------------
    {
        auto d00 = Vector3(0.0, 0.1, 3.0);
        auto d10 = Vector3(1.0, 0.4, 3.0);
        auto d11 = Vector3(1.0, 1.1, 3.0);
        auto d01 = Vector3(0.0, 1.1, 3.0);
        writefln("ao.demo head=%s", "AOPatch demo");
        auto my_AOpatch = new AOPatch(d00, d10, d11, d01);
        writefln("ao.demo patch=%s", "my_AOpatch= " ~ to!string(my_AOpatch));
        auto c = my_AOpatch(0.1, 0.1);
        writefln("ao.demo c=%s", "my_AOpatch(0.1, 0.1)= " ~ to!string(c));
    }
}
