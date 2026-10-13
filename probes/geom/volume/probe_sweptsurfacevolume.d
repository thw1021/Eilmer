// probes/geom/volume/probe_sweptsurfacevolume.d — what
// geom/volume/sweptsurfacevolume.d (83 lines) produces.
//
// SweptSurfaceVolume (D:12-65) is a bottom surface translated along a path: it stores
// dups of the face (D:14) and the path (D:15) plus the eight corners the constructor
// derives (D:18, D:24-31), and opCall (D:46-57) is
//
//     edge04(t) + face0123(r,s) - face0123(0.0,0.0)
//
// — the same expression the corners use, but with (r,s) free. Two things about that
// are observable and are measured rather than assumed:
//  * p[0] is edge04(0.0) ALONE (D:24) while opCall(0,0,0) evaluates
//    edge04(0.0) + face0123(0.0,0.0) - face0123(0.0,0.0), so p[0] and opCall(0,0,0)
//    agree only if (b + a) - a == b holds exactly for that a. Rows ssv.anchor.* ask
//    that question for all eight parameter corners (D:24-31 pairs with the eight
//    (r,s,t) corners), so a port that anchors p[0] with the subtraction, or drops it
//    elsewhere, prints different corners.
//  * the subtraction of face0123(0.0,0.0) is inside opCall, so it is carried into the
//    nine inherited finite differences too: every stencil evaluates the whole
//    expression. That is why the sweep dumps all nine at every point.
//
// Why the sweep is what it is: every inherited accessor branches on `x + d > 1.0` then
// `x - d < 0.0` with d = 0.001, and 0.999 + 0.001 rounds to exactly 1.0 (measured in
// probes/geom/volume/probe_parametricvolume.d). The sweep is therefore the cross
// product of 0.0, 0.001, 0.5, 0.999, 0.9995, 1.0 over (r,s,t) — 216 points per
// accessor — which is what separates `>` from `>=` at the two interior-on-the-edge
// points and makes the mixed partials' inner one-sided branches observable.
//
// Also measured, deliberately not cleaned up:
//  * toString() (D:59-64) prints face0123 and edge04 through to!string, i.e. through
//    their own virtual toString(); a NULL handle prints "null" (rows ssv.nullface.str,
//    reachable because both fields are public — D:14-15 — and reassignable).
//  * dup() (D:41-44) rebuilds through the two-argument constructor, which RE-DERIVES
//    the eight corners; the copy constructor (D:34-39) copies the stored p[] instead
//    (`foreach(i; 0 .. 8) this.p[i] = other.p[i].dup()`). After p[0] is overwritten
//    the two differ, which is what rows ssv.mu.* measure.
//  * this module has NO guard/enforce/throw path of its own — grep over its 83 lines
//    finds no `throw`, no `enforce`, no `assert` outside the unittest (D:82 is the
//    unittest's single assertion). The only failure mode D:22-23 can reach is a null
//    handle argument, and ldc2 1.40 gives no catchable throwable there: a scratch
//    program calling `new SweptSurfaceVolume(null, new Line(a,b))` dies with
//    "Segmentation fault (core dumped)" and exit status 139, exactly as measured for
//    SubRangedVolume (src/geom/volume/subrangedvolume.hpp:51-56). It is not in this
//    corpus because it would kill the probe process.
//  * two degenerate volumes get the same full sweep: a zero-length sweep path (edge04
//    constant) and a collapsed bottom face (all four face corners identical), because
//    those are the cases where a port might special-case the translation.
//
// The face and the path are the probe's own bilinear ProbeSurface and quadratic
// ProbePath (t*t, never `^^`, so the corpus cannot be polluted by pow-vs-multiply
// differences — wave-02-brief section 5). The D unittest (D:67-83) is replayed last
// with the CoonsPatch + Line D itself uses.
//
// Build and capture (the reference tree is copied and @nogc stripped because
// `import geom.gpath` drags modules that cannot compile against a read-only tree;
// see probes/geom/surface/probe_parametricsurface.d):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/volume \
//     probes/geom/volume/probe_sweptsurfacevolume.d -of=/tmp/probe_ssv \
//     -od/tmp/probe_obj_vol2
//   /tmp/probe_ssv > baselines/e587b235/probes/geom_volume_sweptsurfacevolume.txt
import std.conv;
import std.format;
import std.math;
import std.stdio;

import geom.elements;
import geom.gpath.line;
import geom.gpath.path;
import geom.surface.coonspatch;
import geom.surface.parametricsurface;
import geom.volume.parametricvolume;
import geom.volume.sweptsurfacevolume;
import nm.number;

class ProbeSurface : ParametricSurface {
    Vector3 p00, p10, p01, p11;

    this(Vector3 p00, Vector3 p10, Vector3 p01, Vector3 p11) {
        this.p00 = p00; this.p10 = p10; this.p01 = p01; this.p11 = p11;
    }
    override Vector3 opCall(double r, double s) const {
        Vector3 a = p00 + (p10 - p00) * r;
        Vector3 b = p01 + (p11 - p01) * r;
        return a + (b - a) * s;
    }
    override ParametricSurface dup() const {
        return new ProbeSurface(p00, p10, p01, p11);
    }
    override string toString() const {
        return format("ProbeSurface(%s, %s, %s, %s)", to!string(p00), to!string(p10),
                      to!string(p01), to!string(p11));
    }
}

class ProbePath : Path {
    Vector3 a, b, c;

    this(Vector3 a, Vector3 b, Vector3 c) {
        this.a = a; this.b = b; this.c = c;
    }
    override Vector3 opCall(double t) const {
        return a + b*t + c*(t*t);
    }
    override Path dup() const {
        return new ProbePath(a, b, c);
    }
    override string toString() const {
        return format("ProbePath(%s, %s, %s)", to!string(a), to!string(b), to!string(c));
    }
    override string classString() const {
        return "ProbePath";
    }
}

void dump3(const char[] tag, Vector3 v)
{
    writefln("%s x=%.17g y=%.17g z=%.17g", tag, v.x, v.y, v.z);
}

void dump_members(const char[] pfx, SweptSurfaceVolume v)
{
    writefln("%s.str=%s", pfx, v.toString());
    writefln("%s.face0123=%s", pfx, to!string(v.face0123));
    writefln("%s.edge04=%s", pfx, to!string(v.edge04));
    foreach (i; 0 .. 8) {
        dump3(format("%s.p[%d]", pfx, i), v.p[i]);
    }
}

void dump_sweep(const char[] pfx, SweptSurfaceVolume v)
{
    double[6] pts = [0.0, 0.001, 0.5, 0.999, 0.9995, 1.0];
    foreach (r; pts) {
        foreach (s; pts) {
            foreach (t; pts) {
                const string tg = format(" r=%s s=%s t=%s", to!string(r), to!string(s),
                                         to!string(t));
                dump3(pfx ~ ".pt" ~ tg, v(r,s,t));
                dump3(pfx ~ ".dVdr" ~ tg,    v.dVdr(r,s,t));
                dump3(pfx ~ ".dVds" ~ tg,    v.dVds(r,s,t));
                dump3(pfx ~ ".dVdt" ~ tg,    v.dVdt(r,s,t));
                dump3(pfx ~ ".d2Vdr2" ~ tg,  v.d2Vdr2(r,s,t));
                dump3(pfx ~ ".d2Vds2" ~ tg,  v.d2Vds2(r,s,t));
                dump3(pfx ~ ".d2Vdt2" ~ tg,  v.d2Vdt2(r,s,t));
                dump3(pfx ~ ".d2Vdrds" ~ tg, v.d2Vdrds(r,s,t));
                dump3(pfx ~ ".d2Vdrdt" ~ tg, v.d2Vdrdt(r,s,t));
                dump3(pfx ~ ".d2Vdsdt" ~ tg, v.d2Vdsdt(r,s,t));
            }
        }
    }
}

void main()
{
    // A sheared, tilted bottom face and a curved sweep path: no derivative component
    // of any of the nine differences vanishes, and edge04(0.0) != edge04(1.0).
    auto f = new ProbeSurface(Vector3(0.5, -0.25, 0.1), Vector3(2.0, 0.75, -0.3),
                              Vector3(-0.5, 1.5, 0.9), Vector3(1.25, 2.5, 0.05));
    auto e = new ProbePath(Vector3(-1.5, 0.75, 2.25), Vector3(2.5, -1.0, 0.5),
                           Vector3(0.75, 1.25, -0.25));
    auto v = new SweptSurfaceVolume(f, e);
    dump_members("ssv.main", v);

    // p[i] (D:24-31) against opCall at the matching parameter corner (D:55). The eight
    // corners are (r,s,t) in the volume's own numbering: p0=(0,0,0) p1=(1,0,0)
    // p2=(1,1,0) p3=(0,1,0) p4=(0,0,1) p5=(1,0,1) p6=(1,1,1) p7=(0,1,1).
    double[8] cr = [0.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0];
    double[8] cs = [0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0];
    double[8] ct = [0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0];
    foreach (i; 0 .. 8) {
        auto q = v(cr[i], cs[i], ct[i]);
        dump3(format("ssv.anchor.p[%d]", i), v.p[i]);
        dump3(format("ssv.anchor.opCall[%d]", i), q);
        writefln("ssv.anchor.equal[%d]=%s", i, (v.p[i] == q));
    }

    // dup() through the base handle, and the copy constructor — both before any
    // mutation, so they agree here.
    ParametricVolume basev = v;
    auto dv = cast(SweptSurfaceVolume) basev.dup();
    writefln("ssv.dup.str=%s", dv.toString());
    writefln("ssv.dup.isFresh=%s", (cast(SweptSurfaceVolume) dv !is v));
    writefln("ssv.dup.textSame=%s", (dv.toString() == v.toString()));
    writefln("ssv.dup.faceSameHandle=%s", (cast(ProbeSurface) dv.face0123 !is f));
    writefln("ssv.dup.edgeSameHandle=%s", (cast(ProbePath) dv.edge04 !is e));
    foreach (i; 0 .. 8) {
        dump3(format("ssv.dup.p[%d]", i), dv.p[i]);
    }

    auto cv = new SweptSurfaceVolume(v);          // D:34-39 copy constructor
    writefln("ssv.copy.str=%s", cv.toString());
    writefln("ssv.copy.textSame=%s", (cv.toString() == v.toString()));
    writefln("ssv.copy.isFresh=%s", (cast(SweptSurfaceVolume) cv !is v));
    foreach (i; 0 .. 8) {
        dump3(format("ssv.copy.p[%d]", i), cv.p[i]);
    }

    // Full sweeps: main volume, then the two degenerate ones.
    dump_sweep("ssv.main", v);

    // Degenerate 1: a zero-length sweep path — every t gives the same translation.
    auto eSame = new ProbePath(Vector3(1.0, 2.0, 3.0), Vector3(0.0, 0.0, 0.0),
                               Vector3(0.0, 0.0, 0.0));
    auto vFlat = new SweptSurfaceVolume(f, eSame);
    dump_members("ssv.flat", vFlat);
    dump_sweep("ssv.flat", vFlat);

    // Degenerate 2: a collapsed bottom face — face0123(r,s) is constant for all (r,s).
    auto c0 = Vector3(0.3, -1.1, 2.4);
    auto fPoint = new ProbeSurface(c0, c0, c0, c0);
    auto vPoint = new SweptSurfaceVolume(fPoint, e);
    dump_members("ssv.pointface", vPoint);
    dump_sweep("ssv.pointface", vPoint);

    // After overwriting p[0], the copy constructor carries the mutation and dup()
    // re-derives the corner from face0123/edge04 (D:24), so the two diverge.
    v.p[0] = Vector3(99.0, -99.0, 0.5);
    auto cv2 = new SweptSurfaceVolume(v);
    auto dv2 = cast(SweptSurfaceVolume) v.dup();
    dump3("ssv.mu.copy.p[0]", cv2.p[0]);
    dump3("ssv.mu.dup.p[0]", dv2.p[0]);
    writefln("ssv.mu.copyKeepsMutation=%s", (cv2.p[0] == Vector3(99.0, -99.0, 0.5)));
    writefln("ssv.mu.dupReDerived=%s", (dv2.p[0] == e(0.0)));

    // toString() with both handles nulled: D prints "null" for a null class handle.
    v.face0123 = null;
    v.edge04 = null;
    writefln("ssv.nullface.str=%s", v.toString());

    // ---- the D unittest (D:67-83) ------------------------------------------
    Vector3[8] pp;
    pp[0] = Vector3(0.0, 0.1, 0.0);
    pp[1] = Vector3(1.0, 0.1, 0.0);
    pp[2] = Vector3(1.0, 1.1, 0.0);
    pp[3] = Vector3(0.0, 1.1, 0.0);
    pp[4] = Vector3(0.0, 0.1, 3.0);
    pp[5] = Vector3(1.0, 0.1, 3.0);
    pp[6] = Vector3(1.0, 1.1, 3.0);
    pp[7] = Vector3(0.0, 1.1, 3.0);

    ParametricSurface my_face = new CoonsPatch(pp[0], pp[1], pp[2], pp[3]);
    auto my_box = new SweptSurfaceVolume(my_face, new Line(pp[0], pp[4]));
    auto d = my_box(0.1, 0.1, 0.5);
    dump3("ssv.ut.d", d);
    writefln("ssv.ut.approxEqualVectors=%s", approxEqualVectors(d, Vector3(0.1, 0.2, 1.5)));
    writefln("ssv.ut.residual dx=%.17g dy=%.17g dz=%.17g", d.x - 0.1, d.y - 0.2, d.z - 1.5);
    writefln("ssv.ut.str=%s", my_box.toString());
    foreach (i; 0 .. 8) {
        dump3(format("ssv.ut.p[%d]", i), my_box.p[i]);
    }
    auto utDup = cast(SweptSurfaceVolume) my_box.dup();
    writefln("ssv.ut.dup.textSame=%s", (utDup.toString() == my_box.toString()));
    foreach (i; 0 .. 8) {
        dump3(format("ssv.ut.dup.p[%d]", i), utDup.p[i]);
    }
}
