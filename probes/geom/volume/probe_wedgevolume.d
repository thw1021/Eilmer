// probes/geom/volume/probe_wedgevolume.d — what geom/volume/wedgevolume.d
// (118 lines) produces.
//
// WedgeVolume (D:17-81) sweeps a bottom surface about the x-axis by dtheta: opCall
// (D:49-61) is sweep_through_arc(face0123(r,s), t*dtheta) and the constructor derives
// p[0..3] from the face and p[4..7] by sweeping them (D:27-34). sweep_through_arc
// (D:71-80) is the whole module:
//
//     r       = sqrt((p0.y)^^2 + (p0.z)^^2);
//     theta0  = atan2(p0.z, p0.y);
//     theta1  = theta0 + theta;
//     return Vector3(p0.x, r*cos(theta1), r*sin(theta1));
//
// Three measured facts shape this probe:
//  * atan2 is Phobos' own, not libm's (wave-02-brief section 5, T-910), so the port
//    uses eil::atan2 and the corpus carries a signed-zero battery: (y,z) = (0,0),
//    (-0,0), (0,-0), (-0,-0), (0,-1), (-1,0), (-1,-0). Those are exactly the inputs
//    where an atan2 implementation differs (PI vs -PI vs 0 vs -0), and the outputs are
//    0*cos(theta1)/0*sin(theta1), i.e. signed zeros again, so both the quadrant choice
//    and the multiply sign become visible.
//  * `x ^^ 2` here was checked against glibc before writing the port: for 8 sampled
//    doubles (0.1, 0.99999999999999989, 1.2345, 3.0, 1e-8, 1/3, 2.5, DBL_MAX) D's
//    `x^^2`, x*x and std::pow(x,2.0) gave identical bit patterns in 8 of 8 rows, and
//    sqrt matched std::sqrt in 8 of 8; sin/cos over 12 sampled arguments matched
//    glibc bit-for-bit in 12 of 12 (bit patterns printed as %016x). So `sqrt`, `sin`,
//    `cos` and the `^^2` of this module are not a divergence source; only atan2 is,
//    and it is wrapped (substrate/include/eil/math.hpp:149). The port still writes
//    eil::pow for `^^` because wave-02-brief section 5 forbids expanding it.
//  * dtheta = 0.0 does NOT return the face unchanged: p[4] is (x, r*cos(theta0),
//    r*sin(theta0)) with theta0 itself reconstructed from atan2, so it is a round trip
//    through sqrt/atan2/cos/sin, not an identity. Rows wv.d0.* measure that against
//    the face point, and the full 216-point sweep of the zero-dtheta wedge is dumped
//    for the same reason — a port that short-circuits theta==0 would print p[] that
//    looks "better" (exactly the input) and differ from D everywhere.
//
// What else is dumped: toString() (D:63-68, which prints dtheta through to!string,
// i.e. %.6g, while the corpus also carries each dtheta at %.17g and as a bit pattern);
// every stored member (face0123 through to!string, dtheta, all eight corners); dup()
// (D:44-47, which re-runs the constructor and re-derives p[]) and the copy constructor
// (D:37-42, which copies the stored p[]), including after dtheta and p[0] are
// overwritten through the public fields (D:19-21) so the two diverge; the nine
// inherited finite differences at all 216 sweep points (the six-parameter values are
// those because the base stencils branch on `x + 0.001 > 1.0` / `x - 0.001 < 0.0` and
// 0.999 + 0.001 rounds to exactly 1.0 — probes/geom/volume/probe_parametricvolume.d);
// and wedges whose dtheta is NaN / double.init, where cos and sin of NaN reach the
// print (the corpus keeps whatever sign of NaN D prints).
//
// No throw path exists in this module: grep over wedgevolume.d's 118 lines finds no
// `throw`, no `enforce`, no guard. A null face argument would reach `face0123.dup()`
// (D:25) — an uncatchable segfault, measured for SubRangedVolume at
// src/geom/volume/subrangedvolume.hpp:51-56 — so it is not in the corpus.
//
// The unittest's writeln (D:94) is reproduced as corpus rows rather than obtained by
// compiling with -unittest: measured, `ldc2 -unittest` on this probe does not produce
// a binary at all — it stops with "only one entry point `main` is allowed", naming
// /tmp/gdtk_shadow/util/test_runner.d(254) as the other main (pulled in through the
// geom.misc.univariatefunctions closure), so there is no -unittest run to capture.
//
// The D unittest (D:83-118) has one compiled assertion, D:96. The complex-step block
// (D:99-117, including its assert at D:116) is under `version (complex_numbers)`,
// which is not enabled (backlog T-904), so it is carried but not executed.
//
// Build and capture:
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/volume \
//     probes/geom/volume/probe_wedgevolume.d -of=/tmp/probe_wv -od/tmp/probe_obj_vol2
//   /tmp/probe_wv > baselines/e587b235/probes/geom_volume_wedgevolume.txt
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
import geom.volume.wedgevolume;
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

// A surface that ignores (r,s) and always returns the same point, so a signed zero can
// reach sweep_through_arc without being erased by the bilinear interpolation.
class ConstSurface : ParametricSurface {
    Vector3 q;

    this(Vector3 q) { this.q = q; }
    override Vector3 opCall(double r, double s) const { return q; }
    override ParametricSurface dup() const { return new ConstSurface(q); }
    override string toString() const { return format("ConstSurface(%s)", to!string(q)); }
}

string bits(double x)
{
    return format("%016x", *cast(ulong*)&x);
}

void dump3(const char[] tag, Vector3 v)
{
    writefln("%s x=%.17g %s y=%.17g %s z=%.17g %s", tag, v.x, bits(v.x), v.y, bits(v.y),
             v.z, bits(v.z));
}

void dump_members(const char[] pfx, WedgeVolume v)
{
    writefln("%s.str=%s", pfx, v.toString());
    writefln("%s.face0123=%s", pfx, to!string(v.face0123));
    writefln("%s.dtheta=%.17g %s", pfx, v.dtheta, bits(v.dtheta));
    foreach (i; 0 .. 8) {
        dump3(format("%s.p[%d]", pfx, i), v.p[i]);
    }
}

void dump_sweep(const char[] pfx, WedgeVolume v)
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
    // Corners in three different (y,z) quadrants plus one on the axis, so every
    // atan2 branch in sweep_through_arc is used by at least one corner.
    auto f = new ProbeSurface(Vector3(0.25, 1.0, -0.5), Vector3(1.5, -0.75, 0.4),
                              Vector3(-0.5, 0.3, 1.2), Vector3(0.9, -1.1, -0.9));

    auto v = new WedgeVolume(f, to!number(0.6));
    dump_members("wv.main", v);
    // The face's own four corners, to show what p[0..3] is copied from (D:27-30).
    foreach (i, q; [f(0.0,0.0), f(1.0,0.0), f(1.0,1.0), f(0.0,1.0)]) {
        dump3(format("wv.main.faceCorner[%d]", i), q);
    }

    dump_sweep("wv.main", v);

    // dup() re-derives p[] through the constructor (D:44-47); the copy constructor
    // (D:37-42) copies the stored p[].
    ParametricVolume basev = v;
    auto dv = cast(WedgeVolume) basev.dup();
    writefln("wv.dup.str=%s", dv.toString());
    writefln("wv.dup.isFresh=%s", (cast(WedgeVolume) dv !is v));
    writefln("wv.dup.textSame=%s", (dv.toString() == v.toString()));
    writefln("wv.dup.faceSameHandle=%s", (cast(ProbeSurface) dv.face0123 !is f));
    foreach (i; 0 .. 8) {
        dump3(format("wv.dup.p[%d]", i), dv.p[i]);
    }
    auto cv = new WedgeVolume(v);
    writefln("wv.copy.str=%s", cv.toString());
    writefln("wv.copy.textSame=%s", (cv.toString() == v.toString()));
    writefln("wv.copy.isFresh=%s", (cast(WedgeVolume) cv !is v));
    foreach (i; 0 .. 8) {
        dump3(format("wv.copy.p[%d]", i), cv.p[i]);
    }

    // ---- the zero-dtheta wedge ---------------------------------------------
    auto v0 = new WedgeVolume(f, to!number(0.0));
    dump_members("wv.d0", v0);
    foreach (i; 0 .. 4) {
        writefln("wv.d0.p[%d+4].equals.p[%d]=%s", i, i, (v0.p[i+4] == v0.p[i]));
    }
    // opCall at t=0 is sweep_through_arc(face(r,s), 0.0): the round trip through
    // sqrt/atan2/cos/sin, compared with the face point itself.
    bool same_at_t0 = true;
    foreach (rr; [0.0, 0.125, 0.5, 1.0]) {
        foreach (ss; [0.0, 0.375, 0.5, 1.0]) {
            auto q = v0(rr, ss, 0.0);
            auto w = f(rr, ss);
            same_at_t0 = same_at_t0 && (q == w);
            if (rr == 0.125 && ss == 0.375) {
                dump3("wv.d0.probe.q", q);
                dump3("wv.d0.probe.w", w);
            }
        }
    }
    writefln("wv.d0.identityAtT0=%s", same_at_t0);
    dump_sweep("wv.d0", v0);

    // ---- dtheta battery -----------------------------------------------------
    number[8] thetas = [to!number(-0.6), to!number(0.1), PI/4.0, PI, 2.0*PI,
                        to!number(1.0e-20), number.init, to!number(0.0) - to!number(0.0)];
    foreach (i, th; thetas) {
        auto vw = new WedgeVolume(f, th);
        dump_members(format("wv.th[%d]", i), vw);
    }
    // A negative-zero dtheta, and NaN reached through the public field after
    // construction (D:20 makes dtheta public).
    v.dtheta = to!number(-0.0);
    dump_members("wv.negZero", v);
    v.dtheta = to!number(double.nan);
    dump_members("wv.nanTheta", v);
    v.dtheta = to!number(0.6);

    // ---- signed-zero / axis battery through ConstSurface ---------------------
    Vector3[7] axis = [Vector3(0.5, 0.0, 0.0), Vector3(0.5, -0.0, 0.0),
                       Vector3(0.5, 0.0, -0.0), Vector3(0.5, -0.0, -0.0),
                       Vector3(0.5, 0.0, -1.0), Vector3(0.5, -1.0, 0.0),
                       Vector3(0.5, -1.0, -0.0)];
    foreach (i, q; axis) {
        auto fa = new ConstSurface(q);
        auto va = new WedgeVolume(fa, to!number(0.6));
        dump_members(format("wv.axis[%d]", i), va);
        // opCall at t=1.0 must equal p[4..7] pattern-wise; and at t=0.5 half a sweep.
        dump3(format("wv.axis[%d].ptHalf", i), va(0.25, 0.75, 0.5));
    }

    // The face itself replaced by a null handle in the field (D:19 is public): the
    // text must print "null" while the corners keep their old values. toString() is
    // the only thing called here — opCall would segfault.
    v.face0123 = null;
    writefln("wv.nullface.str=%s", v.toString());
    foreach (i; 0 .. 8) {
        dump3(format("wv.nullface.p[%d]", i), v.p[i]);
    }

    // ---- the D unittest (D:83-118) ------------------------------------------
    Vector3[8] pp;
    pp[0] = Vector3(0.0, 0.1, 0.0);
    pp[1] = Vector3(1.0, 0.1, 0.0);
    pp[2] = Vector3(1.0, 1.1, 0.0);
    pp[3] = Vector3(0.0, 1.1, 0.0);
    ParametricSurface my_face = new CoonsPatch(pp[0], pp[1], pp[2], pp[3]);
    auto my_box = new WedgeVolume(my_face, to!number(0.1));
    auto d = my_box(0.1, 0.1, 0.5);
    // D:94 writeln("my_box=", my_box, " d=", d) — reproduced as a corpus row.
    writefln("my_box=%s d=%s", my_box, d);
    dump3("wv.ut.d", d);
    writefln("wv.ut.expected y=%.17g z=%.17g", 0.2 * cos(0.05), 0.2 * sin(0.05));
    writefln("wv.ut.approxEqualVectors=%s",
             approxEqualVectors(d, Vector3(0.1, 0.2 * cos(0.05), 0.2 * sin(0.05))));
    writefln("wv.ut.residual dx=%.17g dy=%.17g dz=%.17g", d.x - 0.1,
             d.y - 0.2*cos(0.05), d.z - 0.2*sin(0.05));
    writefln("wv.ut.str=%s", my_box.toString());
    foreach (i; 0 .. 8) {
        dump3(format("wv.ut.p[%d]", i), my_box.p[i]);
    }
    // The version(complex_numbers) block (D:99-117) is not compiled here either:
    // ldc2 reports `number` as double without the version identifier, so complex(0.0,
    // 1.0e-20) has no type to bind to. Recorded so the port's #ifdef has a referent.
    version (complex_numbers) {
        number hIm = complex(0.0, 1.0e-20);
        writefln("wv.ut.cmplx.hIm.im=%.17g", hIm.im);
    } else {
        writefln("wv.ut.cmplx.notCompiled=true");
    }
}
