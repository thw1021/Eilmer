// probes/geom/surface/probe_channelpatch.d — what geom/surface/channelpatch.d produces.
//
// ChannelPatch (D:13-105) spans two "channel" curves cSouth and cNorth with a bridging
// Path built at each r: a Line when ruled, a cubic Bezier whose middle control points sit
// along the inward surface normals when not (D:64-104). pure2D flattens everything to
// z = 0 on the way in and on the way out.
//
// What is measured rather than reasoned:
//  * there is no throw/enforce/assert anywhere in channelpatch.d (measured with
//    `grep -cE 'throw|enforce|assert\(' geom/surface/channelpatch.d` -> 0), so the
//    failure modes of this class are NaN propagations, and those are dumped instead of
//    avoided: parallel tangents make cross(tangent, pB-pA) the zero vector, whose
//    normalize() divides by zero; coincident boundaries additionally make L = 0, so the
//    control point becomes pA + 0*nan.
//  * make_bridging_path is PUBLIC (D:64) and returns a Path, so its classString(),
//    toString() and control points are dumped directly. The cubic's two middle points are
//    pA + L/3.0*nA and pB + L/3.0*nB — the only place the normal construction (oopvA,
//    oopvB, nA, nB) becomes observable, and a sign error there is a visible coordinate.
//  * tangentA/tangentB come from cSouth.dpdt(r) and cNorth.dpdt(r). Line, Bezier and
//    Polynomial override dpdt with exact derivatives (geom/gpath/{line,bezier,polynomial}.d),
//    but Arc does not, so Arc-bounded channels run Path.dpdt (geom/gpath/path.d:40-53) and
//    its three finite-difference branches. The r sweep therefore uses
//    0.0, 0.0005, 0.001, 0.999, 0.9995, 1.0 — the forward one-sided, central, backward
//    one-sided switch points (dt = 0.001, and 0.999+0.001 == 1.0 exactly, so the test is
//    `>` not `>=`).
//  * nA = cross(oopvA, tangentA) while nB = cross(tangentB, oopvB): the operand order
//    DIFFERS between the two ends (D:96-97), which is what makes both normals face the
//    same way across the channel. Dumping the control points at %.17g distinguishes the
//    two spellings; a "tidied up" port that uses the same order for both shows here.
//  * corners p00..p11 (D:27-30) come from cSouth/cNorth at 0 and 1 and are NOT
//    cross-checked (unlike CoonsPatch), so they are dumped for every construction.
//  * area() is the base sum (parametricsurface.d:40-66) at the D defaults (10,10) and an
//    asymmetric (3,7).
//  * D:21 `this(const Path cSouth, const Path cNorth, bool ruled=false, bool pure2D=false)`
//    — both flags default to false, so the surface_demo.d construction (D:40 of
//    surface_demo.d: `new ChannelPatch(cA, cB)`) is the NON-ruled, non-2D case; its exact
//    stdout text is carried as the chp.demo.* rows.
//  * dup() (D:45-48) re-enters the checking-free constructor and re-derives the corners;
//    the copy constructor (D:33-43) copies them. Both are dumped, plus an alias check that
//    mutates the caller's Line afterwards (D:23-24 dup each path).
//
// Build and capture (see probes/geom/surface/probe_parametricsurface.d for why the
// reference tree is copied with @nogc stripped):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface \
//     probes/geom/surface/probe_channelpatch.d -of=/tmp/probe_chp -od/tmp/probe_obj_w2_surfA
//   /tmp/probe_chp > baselines/e587b235/probes/geom_surface_channelpatch.txt
import std.array;
import std.conv;
import std.format;
import std.math;
import std.stdio;

import geom.elements;
import geom.gpath;
import geom.surface.channelpatch;
import geom.surface.parametricsurface;
import nm.number;

void dump_corner(const char[] tag, const char[] name, Vector3 v) {
    writefln("%s %s x=%.17g y=%.17g z=%.17g", tag, name, v.x, v.y, v.z);
}

void dump_patch(const char[] tag, ChannelPatch cp) {
    writefln("%s str=%s", tag, cp.toString());
    writefln("%s classname=%s", tag, split(to!string(typeid(cp).name), ".")[$-1]);
    writefln("%s ruled=%s pure2D=%s", tag, cp.ruled, cp.pure2D);
    dump_corner(tag, "p00", cp.p00);
    dump_corner(tag, "p10", cp.p10);
    dump_corner(tag, "p11", cp.p11);
    dump_corner(tag, "p01", cp.p01);
    foreach (name; ["cSouth", "cNorth"]) {
        Path b = (name == "cSouth") ? cp.cSouth : cp.cNorth;
        writefln("%s %s class=%s str=%s", tag, name, b.classString(), b.toString());
        foreach (t; [0.0, 0.5, 1.0]) {
            auto p = b(t);
            writefln("%s %s t=%.17g x=%.17g y=%.17g z=%.17g", tag, name, t, p.x, p.y, p.z);
        }
    }
}

/// The bridging path, dumped as far as its own control data: class, text, and the
/// numbers that carry the normal construction.
void dump_bridge(const char[] tag, ChannelPatch cp, double r) {
    Path br = cp.make_bridging_path(r);
    writefln("%s bridge r=%.17g class=%s str=%s", tag, r, br.classString(), br.toString());
    if (auto l = cast(Line)br) {
        writefln("%s bridge r=%.17g p0 x=%.17g y=%.17g z=%.17g", tag, r, l.p0.x, l.p0.y, l.p0.z);
        writefln("%s bridge r=%.17g p1 x=%.17g y=%.17g z=%.17g", tag, r, l.p1.x, l.p1.y, l.p1.z);
    } else if (auto b = cast(Bezier)br) {
        foreach (i; 0 .. b.B.length) {
            writefln("%s bridge r=%.17g B[%d] x=%.17g y=%.17g z=%.17g", tag, r, i, b.B[i].x,
                     b.B[i].y, b.B[i].z);
        }
    }
    foreach (s; [0.0, 0.5, 1.0]) {
        auto p = br(s);
        writefln("%s bridge r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g", tag, r, s, p.x, p.y, p.z);
    }
    // dpdt of the two boundaries at this r: the operands the normals are built from.
    writefln("%s tangentA r=%.17g x=%.17g y=%.17g z=%.17g", tag, r, cp.cSouth.dpdt(r).x,
             cp.cSouth.dpdt(r).y, cp.cSouth.dpdt(r).z);
    writefln("%s tangentB r=%.17g x=%.17g y=%.17g z=%.17g", tag, r, cp.cNorth.dpdt(r).x,
             cp.cNorth.dpdt(r).y, cp.cNorth.dpdt(r).z);
}

void dump_sweep(const char[] tag, ParametricSurface cp, const double[] rs, const double[] ss) {
    foreach (r; rs) {
        foreach (s; ss) {
            auto p = cp(r, s);
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
    const double[] qs = [0.0, 0.2, 0.5, 0.75, 1.0];
    // The Path.dpdt branch points, for the Arc-bounded cases.
    const double[] fd_rs = [0.0, 0.0005, 0.001, 0.5, 0.999, 0.9995, 1.0];
    const double[] ss3 = [0.0, 0.5, 1.0];

    //--- group A: the surface_demo.d channel (ruled=false, pure2D=false) ---------
    Path aA = new Line(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
    Path aB = new Line(Vector3(0.0, 0.25, 0.0), Vector3(1.0, 1.0, 0.0));
    auto demo = new ChannelPatch(aA, aB);
    dump_patch("chp.demo", demo);
    dump_sweep("chp.demo", demo, qs, qs);
    dump_bridge("chp.demo", demo, 0.5);
    dump_area("chp.demo", demo, 10, 10);
    dump_area("chp.demo", demo, 3, 7);
    // surface_demo.d:41-42 exact stdout
    writefln("chp.demo strline=%s", "chanp= " ~ to!string(demo));
    writefln("chp.demo cline=%s", "chanp(0.5, 0.5)= " ~ to!string(demo(0.5, 0.5)));

    //--- group B: the four (ruled, pure2D) combinations --------------------------
    // z-bearing boundaries so pure2D's three `p.z = 0.0` sites are observable.
    Path sB = new Line(Vector3(0.0, 0.0, 0.7), Vector3(1.0, 0.0, -0.3));
    Path nB = new Line(Vector3(0.0, 1.0, 0.2), Vector3(1.0, 1.0, 0.9));
    foreach (ruled; [false, true]) {
        foreach (pure2D; [false, true]) {
            string t = format("chp.f%d%d", ruled ? 1 : 0, pure2D ? 1 : 0);
            auto cp = new ChannelPatch(sB, nB, ruled, pure2D);
            dump_patch(t, cp);
            dump_sweep(t, cp, qs, qs);
            dump_bridge(t, cp, 0.25);
            dump_bridge(t, cp, 1.0);
            dump_area(t, cp, 10, 10);
            dump_area(t, cp, 3, 7);
        }
    }

    //--- group C: Arc boundaries (Path.dpdt finite differences) -----------------
    // Arc(a, b, c) takes a start point, an end point and the CENTRE (arc.d:18-20), and
    // its own validation is lazy: it runs inside opCall, which ChannelPatch's constructor
    // reaches through cSouth(0.0) (channelpatch.d:27). These two arcs have |a-c| == |b-c|
    // exactly and are coplanar, so they evaluate.
    Path sA = new Arc(Vector3(0.0, 0.0, 0.1), Vector3(1.0, 0.0, 0.1), Vector3(0.5, 0.5, 0.1));
    Path nA = new Arc(Vector3(0.0, 1.0, 0.4), Vector3(1.0, 1.0, 0.4), Vector3(0.5, 0.5, 0.4));
    auto arcp = new ChannelPatch(sA, nA);
    dump_patch("chp.arc", arcp);
    foreach (r; fd_rs) {
        dump_bridge("chp.arc", arcp, r);
    }
    dump_sweep("chp.arc", arcp, fd_rs, ss3);
    dump_sweep("chp.arc", arcp, qs, qs);
    dump_area("chp.arc", arcp, 10, 10);
    dump_area("chp.arc", arcp, 3, 7);

    // the same boundaries, ruled: the finite differences must not appear at all.
    auto arcr = new ChannelPatch(sA, nA, true);
    dump_patch("chp.arc.ruled", arcr);
    foreach (r; fd_rs) {
        dump_bridge("chp.arc.ruled", arcr, r);
    }
    dump_sweep("chp.arc.ruled", arcr, fd_rs, ss3);
    dump_area("chp.arc.ruled", arcr, 10, 10);

    // pure2D over the Arc boundaries: the z flattening happens for pA/pB AND p1/p2.
    auto arcp2 = new ChannelPatch(sA, nA, false, true);
    dump_patch("chp.arc.pure2d", arcp2);
    foreach (r; fd_rs) {
        dump_bridge("chp.arc.pure2d", arcp2, r);
    }
    dump_sweep("chp.arc.pure2d", arcp2, fd_rs, ss3);
    dump_area("chp.arc.pure2d", arcp2, 10, 10);

    //--- group D: the zero-cross-product / zero-L branches ----------------------
    // parallel tangent and bridge direction: cross(tangent, pB-pA) == 0, so both
    // normals come from normalize() of the zero vector.
    Path sP = new Line(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
    Path nP = new Line(Vector3(2.0, 0.0, 0.0), Vector3(3.0, 0.0, 0.0));
    auto par = new ChannelPatch(sP, nP);
    dump_patch("chp.parallel", par);
    dump_bridge("chp.parallel", par, 0.5);
    dump_sweep("chp.parallel", par, [0.5], ss3);
    dump_area("chp.parallel", par, 10, 10);
    // the ruled variant of the same geometry is well defined.
    auto parr = new ChannelPatch(sP, nP, true);
    dump_bridge("chp.parallel.ruled", parr, 0.5);
    dump_sweep("chp.parallel.ruled", parr, [0.5], ss3);
    dump_area("chp.parallel.ruled", parr, 10, 10);

    // coincident boundaries: L == 0 as well.
    Path nC = new Line(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
    auto cw = new ChannelPatch(sP, nC);
    dump_patch("chp.coincident", cw);
    dump_bridge("chp.coincident", cw, 0.5);
    dump_sweep("chp.coincident", cw, [0.5], ss3);
    dump_area("chp.coincident", cw, 10, 10);

    // anti-parallel boundaries (pB - pA against the tangent) — still a zero cross product.
    Path nR = new Line(Vector3(2.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
    auto ap = new ChannelPatch(sP, nR);
    dump_bridge("chp.antiparallel", ap, 0.5);
    dump_sweep("chp.antiparallel", ap, [0.5], ss3);

    //--- group E: outside-the-unit r values, which the bridging path accepts ----
    dump_bridge("chp.outr", arcp, -0.5);
    dump_bridge("chp.outr", arcp, 1.5);
    dump_sweep("chp.outr", arcp, [-0.5, 1.5], ss3);

    //--- group F: copy constructor, dup(), aliasing -----------------------------
    auto cpd = new ChannelPatch(sB, nB, false, true);
    auto cpy = new ChannelPatch(cpd);
    dump_patch("chp.copy", cpy);
    dump_sweep("chp.copy", cpy, qs, qs);
    dump_area("chp.copy", cpy, 10, 10);
    auto dp = cast(ChannelPatch)cpd.dup();
    writefln("chp.dup str=%s", dp.toString());
    writefln("chp.dup classname=%s", split(to!string(typeid(dp).name), ".")[$-1]);
    dump_sweep("chp.dup", dp, [0.0, 0.5, 1.0], qs);
    dump_area("chp.dup", dp, 10, 10);
    dump_area("chp.dup", dp, 3, 7);

    // D:23-24 dup the caller's paths, so later mutation must not reach the patch.
    (cast(Line)sB).p1 = Vector3(1.0, 0.0, 9.0);
    dump_patch("chp.aftermut", cpd);
    writefln("chp.callerstr str=%s", sB.toString());

    //--- group F: the flag defaults and the toString() bool spellings -----------
    auto dflt = new ChannelPatch(sP, nP);
    writefln("chp.default flags ruled=%s pure2D=%s", dflt.ruled, dflt.pure2D);
    auto ronly = new ChannelPatch(sP, nP, true);
    writefln("chp.ruledonly flags ruled=%s pure2D=%s", ronly.ruled, ronly.pure2D);
    auto ponly = new ChannelPatch(sP, nP, false, true);
    writefln("chp.pure2donly flags ruled=%s pure2D=%s", ponly.ruled, ponly.pure2D);
    writefln("chp.pure2donly str=%s", ponly.toString());

    // channelpatch.d itself throws nowhere (measured: 0 hits for throw/enforce/assert in
    // its 105 lines), but its constructor EVALUATES both boundaries four times
    // (D:27-30), so a boundary path that throws takes the whole construction down with
    // it. Three cases, each recording which type escapes and its message: an Arc whose
    // radii disagree, a collinear Arc (no plane), and a one-point Bezier, which dies in
    // its own constructor before ChannelPatch ever sees it.
    dump_throw("chp.badarc.radii", () {
        Path bad = new Arc(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0), Vector3(0.4, 0.2, 0.0));
        new ChannelPatch(bad, nP, false, false);
    });
    dump_throw("chp.badarc.collinear", () {
        Path bad = new Arc(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0), Vector3(0.5, 0.0, 0.0));
        new ChannelPatch(bad, nP, true, false);
    });
    dump_throw("chp.badboundary", () {
        Path bad = new Bezier([Vector3(1.0, 0.0, 0.0)]);
        new ChannelPatch(bad, nP, false, false);
    });
    // An Arc boundary that is fine at the corners but throws mid-patch, i.e. the failure
    // surfaces through opCall/make_bridging_path rather than the constructor. The collinear
    // Arc above throws at r=0.0 already, so this uses a north boundary that only gets
    // evaluated at an interior r: a ChannelPatch built on valid corners whose bridging
    // path is asked for r outside the boundary's own definition.
    dump_throw("chp.bridging.northonly", () {
        Path good = new Line(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
        Path badN = new Arc(Vector3(0.0, 1.0, 0.0), Vector3(1.0, 1.0, 0.0), Vector3(0.5, 0.5, 0.0));
        auto cpn = new ChannelPatch(good, badN, false, false);
        cpn.make_bridging_path(0.5);
    });
}
