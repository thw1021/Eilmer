// probes/geom/surface/probe_sweptpathpatch.d — what geom/surface/sweptpathpatch.d
// produces.
//
// SweptPathPatch (D:11-52) is the simplest patch in the hierarchy: cA is swept along cB,
// so opCall(r,s) = cB(r) + cA(s) - cA(0.0) (D:44). It has no unittest block and throws
// nowhere of its own, so the corpus is all about (a) which boundary each corner comes
// from, (b) the two constructors' different treatment of the corners, and (c) the fact
// that a patch built from paths whose dpdt is the base finite difference inherits that
// arithmetic in every caller that differentiates the surface.
//
// What is measured rather than reasoned:
//  * the corner assignment (D:21-24): p00 = cB(0.0), p10 = cB(1.0),
//    p01 = cB(0.0) + cA(1.0) - cA(0.0), p11 = cB(1.0) + cA(1.0) - cA(0.0). cB is the
//    r-direction (south) edge and cA the s-direction (west) edge, and the two swept
//    corners are vector sums, so a port that swapped cA/cB or that dropped the -cA(0.0)
//    shows up in exactly two of the four corner rows.
//  * opCall's evaluation order and association: (cB(r) + cA(s)) - cA(0.0) as written at
//    D:44, not cB(r) + (cA(s) - cA(0.0)). The two differ in the last bit for the Arc and
//    Polyline boundaries, so the sweep is dumped at %.17g.
//  * the r/s sweep uses 0.0, 0.0005, 0.001, 0.999, 0.9995, 1.0 — the points where
//    Path.dpdt (geom/gpath/path.d:40-53) switches between its forward one-sided, central
//    and backward one-sided stencils with dt = 0.001 (and 0.999 + 0.001 == 1.0 exactly, so
//    the comparison is `>`), plus one point in each parametric quadrant. The patch itself
//    does not call dpdt, but its callers differentiate the edges, and the boundary Paths
//    used here (Arc, Polyline) do NOT override dpdt, so their tangents are dumped too.
//  * dup() (D:37-40) re-enters the constructor and RE-DERIVES the corners from the dups,
//    while the copy constructor (D:27-35) copies them verbatim. To make that
//    distinguishable, one patch has its corners corrupted after construction and is then
//    both copied and duped.
//  * area() at the D defaults (10,10) and an asymmetric (3,7), for a planar case (where
//    the answer is the swept length times the edge length) and a twisted one.
//  * the boundary-paths' own toString()/classString() are dumped, because toString() of
//    the patch is "SweptPathPatch(cA=" ~ to!string(cA) ~ ", cB=" ~ to!string(cB) ~ ")"
//    and interpolates them whole (D:50).
//  * a null Path argument, which D accepts at compile time and which dies inside the
//    virtual dup() (D:19) — dumped last, after a stdout flush, because whether it is an
//    Error or a hard crash is exactly the kind of thing this project has been burned by
//    guessing.
//
// Build and capture (see probes/geom/surface/probe_parametricsurface.d for why the
// reference tree is copied with @nogc stripped):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface \
//     probes/geom/surface/probe_sweptpathpatch.d -of=/tmp/probe_spp -od/tmp/probe_obj_w2_surfA
//   /tmp/probe_spp > baselines/e587b235/probes/geom_surface_sweptpathpatch.txt
import std.array;
import std.conv;
import std.format;
import std.math;
import std.stdio;

import geom.elements;
import geom.gpath;
import geom.surface.sweptpathpatch;
import geom.surface.parametricsurface;
import nm.number;

void dump_patch(const char[] tag, SweptPathPatch sp) {
    writefln("%s str=%s", tag, sp.toString());
    writefln("%s classname=%s", tag, split(to!string(typeid(sp).name), ".")[$-1]);
    foreach (name; ["p00", "p10", "p11", "p01"]) {
        Vector3 v = name == "p00" ? sp.p00 : (name == "p10" ? sp.p10
                       : (name == "p11" ? sp.p11 : sp.p01));
        writefln("%s %s x=%.17g y=%.17g z=%.17g", tag, name, v.x, v.y, v.z);
    }
    foreach (name; ["cA", "cB"]) {
        Path b = (name == "cA") ? sp.cA : sp.cB;
        writefln("%s %s class=%s str=%s", tag, name, b.classString(), b.toString());
        foreach (t; [0.0, 0.5, 1.0]) {
            auto p = b(t);
            writefln("%s %s t=%.17g x=%.17g y=%.17g z=%.17g", tag, name, t, p.x, p.y, p.z);
        }
        // the base finite differences, at the branch points
        foreach (t; [0.0, 0.0005, 0.001, 0.999, 0.9995, 1.0]) {
            auto d1 = b.dpdt(t);
            auto d2 = b.d2pdt2(t);
            writefln("%s %s dpdt t=%.17g x=%.17g y=%.17g z=%.17g", tag, name, t, d1.x, d1.y, d1.z);
            writefln("%s %s d2pdt2 t=%.17g x=%.17g y=%.17g z=%.17g", tag, name, t, d2.x, d2.y,
                     d2.z);
        }
    }
}

void dump_sweep(const char[] tag, ParametricSurface ps, const double[] rs, const double[] ss) {
    foreach (r; rs) {
        foreach (s; ss) {
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
    const double[] fd = [0.0, 0.0005, 0.001, 0.999, 0.9995, 1.0];
    const double[] quad = [0.0, 0.25, 0.5, 0.75, 1.0];
    const double[] s3 = [0.0, 0.5, 1.0];

    //--- group A: the surface_demo.d patch (Line swept along Line) --------------
    Path aA = new Line(Vector3(0.0, 0.0, 0.0), Vector3(0.0, 1.0, 0.0));
    Path aB = new Line(Vector3(1.0, 0.25, 0.0), Vector3(2.0, 0.25, 0.0));
    auto demo = new SweptPathPatch(aA, aB);
    dump_patch("spp.demo", demo);
    dump_sweep("spp.demo", demo, quad, quad);
    dump_sweep("spp.demo", demo, fd, s3);
    dump_area("spp.demo", demo, 10, 10);
    dump_area("spp.demo", demo, 3, 7);
    // surface_demo.d:48-49 exact stdout
    writefln("spp.demo strline=%s", "spp= " ~ to!string(demo));
    writefln("spp.demo cline=%s", "spp(0.5, 0.5)= " ~ to!string(demo(0.5, 0.5)));

    // dup() and the copy constructor of the planar patch
    auto dd = demo.dup();
    dump_patch("spp.demo.dup", dd);
    dump_sweep("spp.demo.dup", dd, quad, s3);
    dump_area("spp.demo.dup", dd, 10, 10);
    auto cc = new SweptPathPatch(demo);
    dump_patch("spp.demo.copy", cc);
    dump_sweep("spp.demo.copy", cc, quad, s3);
    dump_area("spp.demo.copy", cc, 3, 7);

    // D:19-20 dup the caller's paths, so mutating them afterwards changes nothing.
    (cast(Line)aA).p1 = Vector3(0.0, 7.0, 0.0);
    dump_patch("spp.demo.aftermut", demo);
    writefln("spp.demo.callerstr str=%s", aA.toString());

    //--- group B: Arc boundaries (base Path.dpdt) ------------------------------
    Path bA = new Arc(Vector3(0.0, 0.0, 0.1), Vector3(0.0, 1.0, 0.1), Vector3(0.5, 0.5, 0.1));
    Path bB = new Arc(Vector3(1.0, 0.0, 0.0), Vector3(2.0, 0.0, 0.0), Vector3(1.5, 0.5, 0.0));
    auto arc = new SweptPathPatch(bA, bB);
    dump_patch("spp.arc", arc);
    dump_sweep("spp.arc", arc, quad, quad);
    dump_sweep("spp.arc", arc, fd, s3);
    dump_area("spp.arc", arc, 10, 10);
    dump_area("spp.arc", arc, 3, 7);
    writefln("spp.arc str.as.base=%s", (cast(ParametricSurface)arc).toString());

    //--- group C: Polyline boundaries (also base Path.dpdt, but piecewise) -----
    Vector3[] pa = [Vector3(0.0, 0.0, 0.0), Vector3(0.2, 0.5, 0.1), Vector3(-0.1, 1.0, 0.3)];
    Vector3[] pb = [Vector3(1.0, 0.0, 0.0), Vector3(1.7, 0.1, -0.2), Vector3(2.0, 0.3, 0.1),
                    Vector3(2.4, 0.0, 0.4)];
    auto poly = new SweptPathPatch(new Polyline(pa), new Polyline(pb));
    dump_patch("spp.poly", poly);
    dump_sweep("spp.poly", poly, quad, quad);
    dump_sweep("spp.poly", poly, fd, s3);
    dump_area("spp.poly", poly, 10, 10);
    dump_area("spp.poly", poly, 3, 7);

    //--- group D: a Bezier swept along an Arc ----------------------------------
    Path cA = new Bezier([Vector3(0.0, 0.0, 0.0), Vector3(0.4, 0.1, 0.2), Vector3(0.1, 0.9, 0.1),
                          Vector3(0.0, 1.0, 0.3)]);
    auto mixed = new SweptPathPatch(cA, bB);
    dump_patch("spp.mixed", mixed);
    dump_sweep("spp.mixed", mixed, quad, quad);
    dump_sweep("spp.mixed", mixed, fd, s3);
    dump_area("spp.mixed", mixed, 10, 10);

    //--- group E: degenerate edges -------------------------------------------
    // a zero-length swept path: the patch collapses onto cB.
    Path zA = new Line(Vector3(0.5, 0.5, 0.5), Vector3(0.5, 0.5, 0.5));
    auto z = new SweptPathPatch(zA, aB);
    dump_patch("spp.zerospan", z);
    dump_sweep("spp.zerospan", z, quad, s3);
    dump_area("spp.zerospan", z, 10, 10);
    // a zero-length sweep path: everything lands on cA(s) - cA(0).
    Path zB = new Line(Vector3(1.0, 1.0, 1.0), Vector3(1.0, 1.0, 1.0));
    auto zs = new SweptPathPatch(aA, zB);
    dump_patch("spp.zerosweep", zs);
    dump_sweep("spp.zerosweep", zs, quad, s3);
    dump_area("spp.zerosweep", zs, 10, 10);

    // corners corrupted after construction: the copy keeps them, dup() recomputes.
    auto corrupt = new SweptPathPatch(aA, aB);
    corrupt.p00 = Vector3(11.0, 12.0, 13.0);
    corrupt.p10 = Vector3(14.0, 15.0, 16.0);
    corrupt.p11 = Vector3(17.0, 18.0, 19.0);
    corrupt.p01 = Vector3(20.0, 21.0, 22.0);
    auto copied = new SweptPathPatch(corrupt);
    dump_patch("spp.corrupt.copy", copied);
    auto reduped = cast(SweptPathPatch)corrupt.dup();
    dump_patch("spp.corrupt.dup", reduped);

    //--- group F: NaN corners, r/s outside the unit interval -------------------
    auto nanc = new SweptPathPatch(new Line(Vector3(), Vector3(1.0, 0.0, 0.0)), aB);
    dump_patch("spp.nan", nanc);
    dump_sweep("spp.nan", nanc, [0.0, 0.5, 1.0], s3);
    dump_area("spp.nan", nanc, 10, 10);
    dump_sweep("spp.demo.outrange", demo, [-0.5, 1.5], [0.0, 0.5, 1.0]);

    //--- group G: throwing boundary paths -------------------------------------
    // SweptPathPatch throws nothing itself (0 matches for throw|enforce|assert in its 54
    // lines), but its constructor evaluates both boundaries, so a bad Arc takes it down.
    dump_throw("spp.badarc.radii", () {
        Path bad = new Arc(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0), Vector3(0.4, 0.2, 0.0));
        new SweptPathPatch(bad, aB);
    });
    dump_throw("spp.badarc.collinear", () {
        Path bad = new Arc(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0), Vector3(0.5, 0.0, 0.0));
        new SweptPathPatch(aB, bad);
    });
    dump_throw("spp.badbezier", () {
        Path bad = new Bezier([Vector3(1.0, 0.0, 0.0)]);
        new SweptPathPatch(bad, aB);
    });
    // an empty Polyline
    dump_throw("spp.badpolyline", () {
        Vector3[] none;
        new SweptPathPatch(new Polyline(none), aB);
    });
    // opCall on a patch whose boundary throws only when evaluated away from the corners
    dump_throw("spp.opcall.boundarythrows", () {
        Path bad = new Arc(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0), Vector3(0.4, 0.2, 0.0));
        // corners come from cB and from cA(0.0), which is where an Arc throws first, so
        // force evaluation through a patch built on a good Arc and then corrupt the field.
        auto gp = new SweptPathPatch(bB, aB);
        gp.cA = bad;
        gp(0.5, 0.5);
    });

    //--- group H: the null Path argument, measured and then left out -------------
    //
    // `new SweptPathPatch(null, cB)` compiles in D (class parameters are references and
    // null is a legal value) and dies inside the virtual `cA.dup()` at D:19. Measured with
    // this probe: the process takes SIGSEGV — exit status 139, core dumped, no exception
    // object, nothing catchable by either the Error or the Exception clause above. That is
    // recorded here rather than emitted as a row, because the corpus has to be capturable
    // by a re-run: the row count of a probe that segfaults depends on the stdio buffer, and
    // a baseline that cannot be regenerated byte for byte is not a gate.
    //
    // Consequence for the port: no null check is added. D has none, and the C++ equivalent
    // (a null std::shared_ptr<Path> dereferenced in the constructor) is undefined behaviour
    // of the same shape. A guard here would turn D's crash into a message, which is a
    // behaviour change this project does not make during transcription.
}
