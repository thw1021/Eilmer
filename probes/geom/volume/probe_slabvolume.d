// probes/geom/volume/probe_slabvolume.d — what
// geom/volume/slabvolume.d (65 lines) produces.
//
// SlabVolume is a SweptSurfaceVolume (D:13) whose sweep path is a Line through the
// bottom face's own first corner: `super(face0123, new Line(p0, p0+dz))` with
// `p0 = face0123(0.0,0.0)` (D:25-26). So the volume is "the face, plus a constant
// thickness vector", and the module's own content is one field (dz, D:15), two
// constructors, dup() and toString().
//
// What is dumped, and why each row can fail:
//  * toString() (D:41-46) — "SlabVolume(face0123=<to!string of the surface handle>,
//    dz=<to!string of a Vector3>)". to!string of a class handle is the object's
//    virtual toString(); to!string of Vector3 is its "Vector3(x, y, z)" text with
//    %.6g fields (wave-02-brief section 5).
//  * EVERY stored member, including the three the module does not declare but
//    inherits from SweptSurfaceVolume (sweptsurfacevolume.d:14-18): face0123, edge04
//    and the Vector3[8] corner array p — p[0..7] are printed individually, because
//    they are built by the *base* constructor from edge04(0.0)/edge04(1.0) and the
//    face differences (sweptsurfacevolume.d:24-31), which is where an off-by-one
//    corner index or a swapped face(r,s) pair would show.
//  * edge04 is dumped as a Path (its own toString, and its two endpoints through
//    opCall) plus the two anchor facts the header comment of SweptSurfaceVolume
//    claims: edge04(0.0) == face(0,0) and edge04(1.0) == face(0,0)+dz.
//  * dup() (D:36-39) goes back through the two-argument constructor, so a dup is a
//    fresh SlabVolume with the same dz; dumped as text and as an identity check.
//  * opCall over the 216-point sweep (cross product of 0.0, 0.001, 0.5, 0.999,
//    0.9995, 1.0 per parameter) and all nine inherited finite differences at every
//    one of those points. The sweep is that set because the base stencils branch on
//    `x + 0.001 > 1.0` / `x - 0.001 < 0.0` and 0.999 + 0.001 rounds to exactly 1.0 —
//    see probes/geom/volume/probe_parametricvolume.d. With a sheared bilinear bottom
//    face, dVdr/dVds/dVdt are non-zero in all three components while the pure second
//    differences are pure cancellation noise (bilinear in r,s; linear in t), so the
//    d2 rows gate the exact subtraction order rather than merely looking plausible.
//  * the D unittest (D:49-65) is replayed with BOTH faces: the CoonsPatch D uses and
//    the four-corner bilinear ProbeSurface this probe otherwise uses. Their point
//    rows are dumped side by side (slab.ut.coon / slab.ut.bilin) — and the corpus
//    answers the substitution question MEASURED, not reasoned: 10 of the 15 point
//    rows are byte-identical, and the 5 that differ are exactly the s=0.5 ones in
//    their y column (Coons gives y=0.60000000000000009, bilinear gives
//    y=0.59999999999999998; the endpoints s=0 and s=1 agree because both faces are
//    the same straight boundary). The unittest point (0.1,0.1,0.5) also differs
//    (y=0.19999999999999993 vs 0.20000000000000004), although approxEqualVectors
//    with D's default tolerances (1.0e-2, 1.0e-5) returns true for both, so the
//    D assertion's OUTCOME is identical either way. This keeps the corpus usable
//    while the surface subclass ports are being written in parallel.
//
// The guard/crash paths, measured rather than reasoned:
//  * there is NO throw and NO assert anywhere in slabvolume.d (measured: `grep -c
//    throw = 0`, `grep -c 'assert(' = 0` apart from the unittest's single assert).
//  * the second constructor `this(ref const(SlabVolume) other)` (D:29-34) is
//    unreachable-by-design: at D:32 it calls `face0123(0.0,0.0)` on the *member*,
//    which super() on D:33 has not yet assigned, so the handle is null and the call
//    is virtual. ldc2 1.40 inserts no null check, so it dies — measured: constructing
//    a SlabVolume from another SlabVolume ends the process with "Segmentation fault
//    (core dumped)", exit status 139, and `catch (Throwable e)` never runs (see
//    probes/geom/volume/probe_slabvolume_copyctor.d, whose whole output is the
//    pre-crash line). It is therefore not a corpus row: a crash produces no bytes.
//    The port keeps the divergence instead of inventing behaviour — see the header
//    note in src/geom/volume/slabvolume.hpp.
//
// Build and capture (see probes/geom/surface/probe_parametricsurface.d for why the
// reference tree is copied and @nogc removed):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/volume
//     probes/geom/volume/probe_slabvolume.d -of=/tmp/probe_slab
//     -od=/tmp/w2_volA/obj_slab
//   /tmp/probe_slab > baselines/e587b235/probes/geom_volume_slabvolume.txt
import std.algorithm;
import std.array;
import std.conv;
import std.format;
import std.math;
import std.stdio;

import geom.elements;
import geom.gpath;
import geom.surface.coonspatch;
import geom.surface.parametricsurface;
import geom.volume.parametricvolume;
import geom.volume.slabvolume;
import geom.volume.sweptsurfacevolume;
import nm.number;

// The same bilinear four-corner patch the surface/volume base probes use. Named
// ProbeSurface so this corpus does not depend on any subclass port landing first.
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

void main()
{
    // A sheared, tilted bottom face and a thickness vector that is NOT along an
    // axis, so no derivative component can vanish and the corner array p[0..7] has
    // eight distinct points.
    auto face = new ProbeSurface(Vector3(0.5, -0.25, 0.1), Vector3(2.0, 0.75, -0.3),
                                 Vector3(-0.5, 1.5, 0.9), Vector3(1.25, 2.5, 0.05));
    Vector3 dz = Vector3(0.3, -0.2, 3.0);
    auto slab = new SlabVolume(face, dz);

    writefln("slab.str=%s", slab.toString());
    writefln("slab.face.str=%s", to!string(slab.face0123));
    writefln("slab.edge04.str=%s", to!string(slab.edge04));
    writefln("slab.edge04.t0.str=%s", to!string(slab.edge04(0.0)));
    writefln("slab.edge04.t1.str=%s", to!string(slab.edge04(1.0)));
    writefln("slab.face00.str=%s", to!string(face(0.0, 0.0)));
    writefln("slab.anchor.edge04_0_is_face00=%s", (slab.edge04(0.0) == face(0.0,0.0)));
    writefln("slab.anchor.edge04_1_is_face00_plus_dz=%s",
             (slab.edge04(1.0) == face(0.0,0.0) + dz));
    // dz, twice: %.17g and through toString(), which is what slab.toString() uses.
    dump3("slab.dz", slab.dz);
    writefln("slab.dz.text=%s", to!string(slab.dz));

    // The inherited corner array, every element.
    foreach (i; 0 .. 8) {
        dump3(format("slab.p[%d]", i), slab.p[i]);
    }

    // dup(): a fresh object built through the two-argument constructor.
    auto d = slab.dup();
    writefln("slab.dup.str=%s", d.toString());
    writefln("slab.dup.isFresh=%s", (cast(SlabVolume)d !is slab));
    dump3("slab.dup.dz", d.dz);
    foreach (i; 0 .. 8) {
        dump3(format("slab.dup.p[%d]", i), d.p[i]);
    }
    // Mutating the source's dz must not reach the dup.
    string before = d.toString();
    slab.dz = Vector3(7.0, 7.0, 7.0);
    writefln("slab.dup.textUnchanged=%s", (d.toString() == before));
    writefln("slab.str.afterMutation=%s", slab.toString());
    slab.dz = dz; // restore for the sweep

    double[6] pts = [0.0, 0.001, 0.5, 0.999, 0.9995, 1.0];
    foreach (r; pts) {
        foreach (s; pts) {
            foreach (t; pts) {
                const string tg = format(" r=%s s=%s t=%s", to!string(r), to!string(s),
                                         to!string(t));
                dump3("slab.pt" ~ tg, slab(r,s,t));
                dump3("slab.dVdr" ~ tg,    slab.dVdr(r,s,t));
                dump3("slab.dVds" ~ tg,    slab.dVds(r,s,t));
                dump3("slab.dVdt" ~ tg,    slab.dVdt(r,s,t));
                dump3("slab.d2Vdr2" ~ tg,  slab.d2Vdr2(r,s,t));
                dump3("slab.d2Vds2" ~ tg,  slab.d2Vds2(r,s,t));
                dump3("slab.d2Vdt2" ~ tg,  slab.d2Vdt2(r,s,t));
                dump3("slab.d2Vdrds" ~ tg, slab.d2Vdrds(r,s,t));
                dump3("slab.d2Vdrdt" ~ tg, slab.d2Vdrdt(r,s,t));
                dump3("slab.d2Vdsdt" ~ tg, slab.d2Vdsdt(r,s,t));
            }
        }
    }

    // ---- the D unittest (D:49-65), replayed with both faces -------------------
    Vector3[8] q;
    q[0] = Vector3(0.0, 0.1, 0.0);
    q[1] = Vector3(1.0, 0.1, 0.0);
    q[2] = Vector3(1.0, 1.1, 0.0);
    q[3] = Vector3(0.0, 1.1, 0.0);
    q[4] = Vector3(0.0, 0.1, 3.0);
    q[5] = Vector3(1.0, 0.1, 3.0);
    q[6] = Vector3(1.0, 1.1, 3.0);
    q[7] = Vector3(0.0, 1.1, 3.0);

    ParametricSurface my_face = new CoonsPatch(q[0], q[1], q[2], q[3]);
    auto my_box = new SlabVolume(my_face, q[4]-q[0]);
    auto u = my_box(0.1, 0.1, 0.5);
    dump3("slab.ut.coon.d", u);
    writefln("slab.ut.coon.approxEqualVectors=%s", approxEqualVectors(u, Vector3(0.1, 0.2, 1.5)));
    writefln("slab.ut.coon.residual dx=%.17g dy=%.17g dz=%.17g", u.x - 0.1, u.y - 0.2,
             u.z - 1.5);
    writefln("slab.ut.coon.str=%s", my_box.toString());
    foreach (rr; [0.0, 0.1, 0.5, 0.9995, 1.0]) {
        foreach (ss; [0.0, 0.5, 1.0]) {
            dump3(format("slab.ut.coon.pt r=%s s=%s t=0.5", to!string(rr), to!string(ss)),
                  my_box(rr, ss, 0.5));
        }
    }

    // The bilinear substitute, same four corners: the face is a flat rectangle here,
    // so a Coons patch over it and the bilinear interpolation must agree.
    ParametricSurface bface = new ProbeSurface(q[0], q[1], q[3], q[2]);
    auto bbox = new SlabVolume(bface, q[4]-q[0]);
    auto ub = bbox(0.1, 0.1, 0.5);
    dump3("slab.ut.bilin.d", ub);
    writefln("slab.ut.bilin.approxEqualVectors=%s", approxEqualVectors(ub, Vector3(0.1, 0.2, 1.5)));
    foreach (rr; [0.0, 0.1, 0.5, 0.9995, 1.0]) {
        foreach (ss; [0.0, 0.5, 1.0]) {
            dump3(format("slab.ut.bilin.pt r=%s s=%s t=0.5", to!string(rr), to!string(ss)),
                  bbox(rr, ss, 0.5));
        }
    }
}
