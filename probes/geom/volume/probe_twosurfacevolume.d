// probes/geom/volume/probe_twosurfacevolume.d — what
// geom/volume/twosurfacevolume.d (153 lines) produces.
//
// TwoSurfaceVolume is a ruled volume between two surfaces: it stores dups of the two
// faces, a one-character `ruled_direction` ("i","j","k" or their aliases "r","s","t",
// D:48) and the eight corner points the constructor derives from the faces (D:56-80).
// opCall (D:96-124) interpolates between the two faces along the ruled direction.
//
// The three branch groups are NOT symmetric, and one of them is a quirk that must be
// transcribed, not fixed:
//  * "t"/"k" (D:118): face0(r,s).scale(1.0-t) + face1(r,s).scale(t) — the expected
//    ruled interpolation, faces parameterised by (r,s).
//  * "s"/"j" (D:114): face0(r,t).scale(1.0-s) + face1(r,t).scale(s) — faces by (r,t).
//  * "r"/"i" (D:110): `face0(s, t).scale(1.0-1) + face1(s, t).scale(r)` — the scalar
//    on face0 is the literal expression `1.0-1`, i.e. exactly 0.0, NOT (1.0-r). The
//    "r"/"i" branch therefore discards face0 entirely: opCall(r,s,t) is just
//    face1(s,t) scaled by r, and the volume collapses to the origin at r=0. Measured
//    here (row tsv.i.discards_face0 compares the branch result against
//    face1(s,t).scale(r) directly, and tsv.i.face0_unused shows that changing face0
//    to a different surface leaves every "i" point unchanged), so the port keeps
//    `1.0-1` verbatim instead of "correcting" it (wave-02-brief rule 1).
//
// What is dumped:
//  * toString() (D:126-133) — note ruled_direction is appended RAW (D:130), not
//    through to!string, so no quotes appear in the text.
//  * every stored member: face0 and face1 through to!string (their own toString),
//    ruled_direction, and the inherited Vector3[8] p — all eight corners, for each of
//    the six accepted spellings, because the constructor writes them in a different
//    index order per branch (D:60-61, :67-68, :74-76) and that permutation is the
//    module's whole geometric contract.
//  * the two constructors: dup() (D:91-94) rebuilds through the three-argument
//    constructor, so it re-derives p from the faces; the copy constructor (D:83-89)
//    copies the stored p instead (`foreach(i; 0 .. 8) this.p[i] = other.p[i].dup()`),
//    which is why both are dumped — after a corner mutation they differ. D's p[i] is
//    a Vector3 struct so .dup() is a value copy.
//  * the alias pairs: "r" vs "i", "s" vs "j", "t" vs "k" must produce identical text
//    and identical points (rows tsv.alias.*).
//  * opCall over the 216-point sweep (cross product of 0.0, 0.001, 0.5, 0.999, 0.9995,
//    1.0 per parameter) and all nine inherited finite differences at every point, for
//    each of the three distinct branches. The six values are those because the base
//    stencils branch on `x + 0.001 > 1.0` / `x - 0.001 < 0.0` and 0.999+0.001 rounds
//    to exactly 1.0 — see probes/geom/volume/probe_parametricvolume.d.
//  * both throw paths, printing WHICH throwable type escapes and the message:
//    D:79 (constructor, default case) and D:121 (opCall, default case, reachable
//    because ruled_direction is a public field that can be reassigned after
//    construction). Each is also caught as Exception and as Throwable, because in
//    this port Exception and Error are siblings and a `catch (Exception)` that hides
//    an Error is a behaviour change.
//
// The faces are the probe's own bilinear ProbeSurface (same shape as
// probes/geom/surface/probe_parametricsurface.d), so this corpus does not depend on
// any surface subclass port landing first. The D unittest (D:136-153) is replayed at
// the end with the CoonsPatch faces D uses (geom/surface/coonspatch.d, whose
// corner argument order is p00, p10, p11, p01 — D:31).
//
// Build and capture (see probes/geom/surface/probe_parametricsurface.d for why the
// reference tree is copied and @nogc removed):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/volume
//     probes/geom/volume/probe_twosurfacevolume.d -of=/tmp/probe_tsv
//     -od=/tmp/w2_volA/obj_tsv
//   /tmp/probe_tsv > baselines/e587b235/probes/geom_volume_twosurfacevolume.txt
import std.algorithm;
import std.array;
import std.conv;
import std.format;
import std.math;
import std.stdio;

import geom.elements;
import geom.surface.coonspatch;
import geom.surface.parametricsurface;
import geom.volume.parametricvolume;
import geom.volume.twosurfacevolume;
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

void dump3(const char[] tag, Vector3 v)
{
    writefln("%s x=%.17g y=%.17g z=%.17g", tag, v.x, v.y, v.z);
}

// The members the constructor derives, for one accepted spelling.
void dump_members(const char[] pfx, TwoSurfaceVolume v)
{
    writefln("%s.str=%s", pfx, v.toString());
    writefln("%s.face0=%s", pfx, to!string(v.face0));
    writefln("%s.face1=%s", pfx, to!string(v.face1));
    writefln("%s.ruled_direction=%s", pfx, v.ruled_direction);
    foreach (i; 0 .. 8) {
        dump3(format("%s.p[%d]", pfx, i), v.p[i]);
    }
}

// toString + corners + the full 216-point sweep of opCall and the nine differences.
void dump_sweep(const char[] pfx, TwoSurfaceVolume v)
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
    // Two sheared, tilted, separated faces, so (a) every derivative component of
    // every difference is non-zero in the "j"/"k" branches, (b) the eight corners are
    // eight distinct points, and (c) the "i" branch's discarded face0 is provably
    // different from the face it keeps.
    auto f0 = new ProbeSurface(Vector3(0.5, -0.25, 0.1), Vector3(2.0, 0.75, -0.3),
                               Vector3(-0.5, 1.5, 0.9), Vector3(1.25, 2.5, 0.05));
    auto f1 = new ProbeSurface(Vector3(0.1, 0.2, 3.0), Vector3(2.5, -0.4, 2.6),
                               Vector3(-0.3, 1.9, 3.9), Vector3(1.6, 2.1, 3.05));

    string[6] dirs = ["t", "k", "s", "j", "r", "i"];
    TwoSurfaceVolume[6] vols;
    foreach (i, d; dirs) {
        vols[i] = new TwoSurfaceVolume(f0, f1, d);
        dump_members("tsv." ~ d, vols[i]);
    }

    // The alias pairs: same text, same corners, same points.
    writefln("tsv.alias.t_is_k=%s", (vols[0].toString() == vols[1].toString()));
    writefln("tsv.alias.s_is_j=%s", (vols[2].toString() == vols[3].toString()));
    writefln("tsv.alias.r_is_i=%s", (vols[4].toString() == vols[5].toString()));
    bool alias_pts_same = true;
    foreach (rr; [0.0, 0.25, 0.5, 0.9995, 1.0]) {
        foreach (ss; [0.0, 0.5, 1.0]) {
            foreach (tt; [0.0, 0.3, 1.0]) {
                alias_pts_same = alias_pts_same && (vols[0](rr,ss,tt) == vols[1](rr,ss,tt));
                alias_pts_same = alias_pts_same && (vols[2](rr,ss,tt) == vols[3](rr,ss,tt));
                alias_pts_same = alias_pts_same && (vols[4](rr,ss,tt) == vols[5](rr,ss,tt));
            }
        }
    }
    writefln("tsv.alias.pointsAllSame=%s", alias_pts_same);

    // The "i" branch quirk, measured twice.
    auto vi = vols[5];
    auto q = vi(0.375, 0.25, 0.625);
    auto expect = f1(0.25, 0.625).scale(0.375);   // face1(s,t).scale(r), D:110
    dump3("tsv.i.point", q);
    dump3("tsv.i.face1_scaled", expect);
    writefln("tsv.i.discards_face0=%s", (q == expect));
    auto i_at_zero = vi(0.0, 0.25, 0.625);
    writefln("tsv.i.collapses_at_r0=%s", (i_at_zero.x == 0.0 && i_at_zero.y == 0.0
                                          && i_at_zero.z == 0.0));
    // Replace face0 with a completely different surface: every "i" point is unchanged.
    auto other0 = new ProbeSurface(Vector3(-7.0, -8.0, -9.0), Vector3(7.0, 8.0, 9.0),
                                   Vector3(11.0, 12.0, 13.0), Vector3(-11.0, -12.0, -13.0));
    auto vi2 = new TwoSurfaceVolume(other0, f1, "i");
    bool same = true;
    foreach (rr; [0.0, 0.375, 1.0]) {
        foreach (ss; [0.0, 0.5, 1.0]) {
            same = same && (vi(rr,ss,0.625) == vi2(rr,ss,0.625));
        }
    }
    writefln("tsv.i.face0_unused=%s", same);
    // ... while the "k" branch does use both faces.
    auto vk = vols[0];
    writefln("tsv.k.face0_used=%s", (vk(0.4, 0.3, 0.6) != (new TwoSurfaceVolume(other0, f1,
                                                                                "t"))(0.4, 0.3, 0.6)));

    // dup() through the base handle, and the copy constructor.
    ParametricVolume basev = vk;
    auto dk = cast(TwoSurfaceVolume) basev.dup();
    writefln("tsv.k.dup.str=%s", dk.toString());
    writefln("tsv.k.dup.isFresh=%s", (cast(TwoSurfaceVolume) dk !is vk));
    writefln("tsv.k.dup.textSame=%s", (dk.toString() == vk.toString()));
    foreach (i; 0 .. 8) {
        dump3(format("tsv.k.dup.p[%d]", i), dk.p[i]);
    }

    auto ck = new TwoSurfaceVolume(vk);          // D:83-89 copy constructor
    writefln("tsv.k.copy.str=%s", ck.toString());
    writefln("tsv.k.copy.textSame=%s", (ck.toString() == vk.toString()));
    writefln("tsv.k.copy.isFresh=%s", (cast(TwoSurfaceVolume) ck !is vk));
    foreach (i; 0 .. 8) {
        dump3(format("tsv.k.copy.p[%d]", i), ck.p[i]);
    }

    // Full sweeps for the three distinct branches.
    dump_sweep("tsv.k", vols[0]);
    dump_sweep("tsv.j", vols[3]);
    dump_sweep("tsv.i", vols[5]);

    // ---- throw paths ----------------------------------------------------------
    // D:79, the constructor's default case.
    try {
        auto bad = new TwoSurfaceVolume(f0, f1, "q");
        writefln("tsv.ctorThrow.noThrow=%s", bad.toString());
    } catch (Exception e) {
        writefln("tsv.ctorThrow.asException=%s msg=[%s]", e.classinfo.name, e.msg);
    } catch (Throwable e) {
        writefln("tsv.ctorThrow.asThrowable=%s msg=[%s]", e.classinfo.name, e.msg);
    }
    // The same object re-read as Throwable, to show which side of the hierarchy it
    // is on (Exception and Error are siblings in this port).
    try {
        auto bad = new TwoSurfaceVolume(f0, f1, "");
        writefln("tsv.ctorThrowEmpty.noThrow=%s", bad.toString());
    } catch (Throwable e) {
        writefln("tsv.ctorThrowEmpty=%s isException=%s isError=%s msg=[%s]",
                 e.classinfo.name, (cast(Exception) e !is null), (cast(Error) e !is null),
                 e.msg);
    }
    // D:121, opCall's default case, reached by reassigning the public field.
    vk.ruled_direction = "q";
    try {
        auto p = vk(0.5, 0.5, 0.5);
        writefln("tsv.opcallThrow.noThrow x=%.17g", p.x);
    } catch (Exception e) {
        writefln("tsv.opcallThrow.asException=%s msg=[%s]", e.classinfo.name, e.msg);
    }
    // ... and dup() now re-enters the constructor with that same bad direction.
    try {
        auto d2 = vk.dup();
        writefln("tsv.dupThrow.noThrow=%s", d2.toString());
    } catch (Exception e) {
        writefln("tsv.dupThrow.asException=%s msg=[%s]", e.classinfo.name, e.msg);
    }
    // The corners stay intact after the direction is corrupted: the p[] array is
    // only written by the constructor, so the object keeps its original corners.
    vk.ruled_direction = "t";
    foreach (i; 0 .. 8) {
        dump3(format("tsv.k.p[%d].afterDirectionRepair", i), vk.p[i]);
    }

    // ---- the D unittest (D:136-153) ------------------------------------------
    Vector3[8] pp;
    pp[0] = Vector3(0.0, 0.1, 0.0);
    pp[1] = Vector3(1.0, 0.1, 0.0);
    pp[2] = Vector3(1.0, 1.1, 0.0);
    pp[3] = Vector3(0.0, 1.1, 0.0);
    pp[4] = Vector3(0.0, 0.1, 3.0);
    pp[5] = Vector3(1.0, 0.1, 3.0);
    pp[6] = Vector3(1.0, 1.1, 3.0);
    pp[7] = Vector3(0.0, 1.1, 3.0);

    ParametricSurface my_face_bottom = new CoonsPatch(pp[0], pp[1], pp[2], pp[3]);
    ParametricSurface my_face_top = new CoonsPatch(pp[4], pp[5], pp[6], pp[7]);
    auto my_box = new TwoSurfaceVolume(my_face_bottom, my_face_top, "t");
    auto u = my_box(0.1, 0.1, 0.5);
    dump3("tsv.ut.d", u);
    writefln("tsv.ut.approxEqualVectors=%s", approxEqualVectors(u, Vector3(0.1, 0.2, 1.5)));
    writefln("tsv.ut.residual dx=%.17g dy=%.17g dz=%.17g", u.x - 0.1, u.y - 0.2, u.z - 1.5);
    writefln("tsv.ut.str=%s", my_box.toString());
    foreach (i; 0 .. 8) {
        dump3(format("tsv.ut.p[%d]", i), my_box.p[i]);
    }
}
