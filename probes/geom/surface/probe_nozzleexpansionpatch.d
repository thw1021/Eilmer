// probes/geom/surface/probe_nozzleexpansionpatch.d — what
// geom/surface/nozzleexpansionpatch.d (135 lines) produces.
//
// The patch is defined by one Path, the north edge (D:36); every other point is
// derived. opCall (D:53-128) is a four-way branch: r==0 and r==1 return points on the
// two straight end edges, s==1 returns the north edge itself, and everything else —
// including the south edge — is built from a quadratic Bezier whose control points
// come from the WALL NORMAL at cNorth(r) (a one-sided or central finite difference
// with eps = 0.0001, D:79-93), scaled by two end tweaks (D:101-111).
//
// Why measuring rather than reasoning:
//  * the branch ORDER is r==0, then r==1, then s==1, then interior. So (0,1) and
//    (1,1) are the EAST/WEST rows, not the north-edge row a reader would expect. Both
//    corners are dumped.
//  * the two transcendental calls are the whole behaviour: `atan` (D:83, :87, :91) and
//    `tan` (D:114). Measured against ldc2 1.40 (this probe's group 0, plus a standalone
//    21-point sweep): Phobos' `atan(double)` is its own atanImpl and its bits agree with
//    glibc on those samples, but Phobos' `tan(double)` is ALSO its own tanImpl
//    (std/math/trigonometry.d:194 -> :326) and DIFFERS from glibc's tan — at
//    x = 0.90000000000000002 D gives 1.2601582175503390 / bits 3ff4299ba9c2a137 where
//    glibc gives ...392 / bits 3ff4299ba9c2a138, i.e. 1 ULP, which is enough to break a
//    byte comparison because tan() feeds mid_pt_x directly (D:114). The port therefore
//    mirrors tanImpl!double locally (src/geom/surface/nozzleexpansionpatch.cpp d_tan) —
//    there is no eil::tan in the substrate. Group 0 dumps atan and tan with bits so the
//    mirror is tested, not trusted.
//  * eps branches: r < eps uses a FORWARD difference, 1-r < eps a BACKWARD one,
//    otherwise central (D:81-93). Both one-sided branches are hit with r = 1e-9 and
//    r = 1-1e-9, and with r just outside them (2e-4).
//  * the 2% / 98% tweaks (D:101-111) divide by (0.02*north_length); a VERTICAL north
//    edge makes north_length 0, so the divisions are 0/0 and the group-4 rows print the
//    NaN/inf that results rather than avoiding it.
//  * the south edge (s == 0) is inside the "else" branch (D:118-121) and returns the
//    axis point, which is NOT the x-axis unless the tweaks leave it there — dumped.
//  * `p.z` is set to 0.0 before every branch (D:56) while p.x/p.y start as Vector3.init
//    (NaN), so a branch that forgot to write one would print nan. Every row prints all
//    three components.
//  * there is no enforce and no throw in the module (measured: `grep -n "throw\|enforce"
//    nozzleexpansionpatch.d` finds nothing), and no unittest block and no classString
//    override, so the only "guard" rows are the singular-division edges of group 4.
//
// Build and capture (one line, so the header comment stays a single // comment):
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface probes/geom/surface/probe_nozzleexpansionpatch.d -of=/tmp/probe_nep -od=/tmp/w2_surfB/obj_nep
//   then /tmp/probe_nep > baselines/e587b235/probes/geom_surface_nozzleexpansionpatch.txt
import std.algorithm;
import std.array;
import std.conv;
import std.format;
import std.math;
import std.stdio;

import geom.elements;
import geom.gpath;
import geom.surface.nozzleexpansionpatch;
import geom.surface.parametricsurface;
import nm.number;

ulong dbits(double x) { return *cast(ulong*)&x; }

void dump_v3(string key, const Vector3 v) {
    writefln("%s x=%.17g y=%.17g z=%.17g", key, v.x, v.y, v.z);
}

void dump_area(string key, ParametricSurface ps, int nr, int ns) {
    auto a = ps.area(nr, ns);
    writefln("%s nr=%d ns=%d area x=%.17g y=%.17g z=%.17g", key, nr, ns, a.x, a.y, a.z);
}

void sweep(string key, NozzleExpansionPatch p) {
    foreach (r; [0.0, 1e-9, 2e-4, 0.25, 0.5, 0.75, 0.9998, 1.0 - 1e-9, 1.0]) {
        foreach (s; [0.0, 0.25, 0.5, 1.0]) {
            dump_v3(format("%s.opCall r=%.17g s=%.17g", key, r, s), p(r, s));
        }
    }
}

void main()
{
    // --- group 0: the two transcendentals, with bit patterns ----------------------
    // wall_angle = atan(slope) and the code then takes tan(wall_angle), so tan(atan())
    // is what feeds mid_pt_x. Slopes span the range a nozzle wall takes (shallow to
    // ~51 degrees, i.e. wall_angle ~0.9 rad) plus both infinities and a NaN.
    foreach (m; [0.0, 0.001, 0.01, 0.05, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9,
                 1.0, 1.1, 1.26, 1.5, 2.0, 3.0, 5.0, 10.0, 100.0, -0.1, -0.5, -1.26, -3.0]) {
        double a = atan(m);
        double t = tan(a);
        writefln("nep.kern atan(m=%.17g)=%.17g abits=%016x tan(that)=%.17g tbits=%016x", m, a,
                 dbits(a), t, dbits(t));
    }
    // tan on its own argument range, including the singular region and special values.
    foreach (a; [0.0, 0.05, 0.25, 0.5, 0.75, 0.9, 1.0, 1.2, 1.4, 1.5707963267948966,
                 -0.9, 3.0, 6.0, 1.0 / 0.0, 0.0 / 0.0]) {
        double t = tan(a);
        writefln("nep.kern2 tan(%.17g)=%.17g bits=%016x", a, t, dbits(t));
    }

    // --- group 1: toString() and the single data member ---------------------------
    // A nozzle-like north edge: starts flat at the throat, steepens, and leaves the
    // exit with a non-zero slope (so both the 2% and the 98% tweaks fire).
    auto np = new NozzleExpansionPatch(
        new Bezier([Vector3(1.0, 0.2, 0.0), Vector3(2.0, 0.25, 0.0), Vector3(3.0, 0.9, 0.0),
                    Vector3(4.5, 2.2, 0.0)]));
    writefln("nep.bez str=%s", np.toString());
    writefln("nep.bez cNorth=%s", to!string(np.cNorth));
    sweep("nep.bez", np);

    // A straight north edge at a moderate angle: constant wall_angle.
    auto ln = new NozzleExpansionPatch(new Line(Vector3(0.5, 0.1, 0.0), Vector3(3.0, 1.6, 0.0)));
    writefln("nep.line str=%s", ln.toString());
    sweep("nep.line", ln);

    // A steep north edge whose wall_angle lands near 0.9 rad, the region where Phobos'
    // tan and glibc's differ by 1 ULP.
    auto steep = new NozzleExpansionPatch(
        new Bezier([Vector3(0.0, 0.0, 0.0), Vector3(0.2, 0.9, 0.0), Vector3(0.5, 1.8, 0.0),
                    Vector3(0.8, 2.6, 0.0)]));
    writefln("nep.steep str=%s", steep.toString());
    sweep("nep.steep", steep);

    // A converging edge (negative slope), so wall_angle and tan(wall_angle) are
    // negative and the sign branch of tanImpl is exercised.
    auto conv = new NozzleExpansionPatch(new Line(Vector3(1.0, 2.0, 0.0), Vector3(4.0, 0.5, 0.0)));
    writefln("nep.conv str=%s", conv.toString());
    sweep("nep.conv", conv);

    // --- group 2: dup() and the copy constructor ---------------------------------
    auto dp = cast(NozzleExpansionPatch) np.dup();
    writefln("nep.dup str=%s", dp.toString());
    dump_v3("nep.dup.opCall", dp(0.37, 0.63));
    auto cp = new NozzleExpansionPatch(np);
    writefln("nep.copy str=%s", cp.toString());
    dump_v3("nep.copy.opCall", cp(0.37, 0.63));
    // dup of a dup stays a NozzleExpansionPatch with the same cNorth text.
    writefln("nep.dup.dup str=%s", cast(NozzleExpansionPatch) dp.dup());

    // --- group 3: area() from the base class -------------------------------------
    dump_area("nep.bez.area.default", np, 10, 10);
    dump_area("nep.bez.area.r3s7", np, 3, 7);
    dump_area("nep.bez.area.r7s3", np, 7, 3);
    dump_area("nep.line.area.default", ln, 10, 10);
    dump_area("nep.steep.area.r5s5", steep, 5, 5);
    dump_area("nep.conv.area.r2s9", conv, 2, 9);

    // --- group 4: singular divisions ---------------------------------------------
    // Vertical north edge: north_length = 0, so 0.02*north_length = 0 and the two
    // tweak divisions are 0/0; the wall normal is atan(dy/0) = atan(inf) = PI/2 and
    // tan(PI/2) is the finite but huge value Phobos returns.
    auto vert = new NozzleExpansionPatch(new Line(Vector3(1.0, 0.0, 0.0), Vector3(1.0, 2.0, 0.0)));
    writefln("nep.vert str=%s", vert.toString());
    sweep("nep.vert", vert);
    // Horizontal north edge on the axis: y is 0 everywhere, so every wall_angle is 0.
    auto flat = new NozzleExpansionPatch(new Line(Vector3(2.0, 0.0, 0.0), Vector3(5.0, 0.0, 0.0)));
    writefln("nep.flat str=%s", flat.toString());
    sweep("nep.flat", flat);
    // A zero-length north edge: north_length = 0 and both differences are 0, so the
    // slope is 0/0 = nan and the whole interior is nan.
    auto nul = new NozzleExpansionPatch(new Line(Vector3(1.0, 1.0, 0.0), Vector3(1.0, 1.0, 0.0)));
    writefln("nep.null-edge str=%s", nul.toString());
    sweep("nep.nulledge", nul);
    // An extremely short but non-zero edge: the 2%/98% scales blow up.
    auto tiny = new NozzleExpansionPatch(new Line(Vector3(0.0, 0.0, 0.0), Vector3(1e-12, 1e-12, 0.0)));
    writefln("nep.tiny str=%s", tiny.toString());
    sweep("nep.tiny", tiny);
    // Out-of-range parameters: no validation, and r<eps also fires for r<0.
    dump_v3("nep.bez.r=-0.5", np(-0.5, 0.5));
    dump_v3("nep.bez.r=2", np(2.0, 0.5));
    dump_v3("nep.bez.s=2", np(0.5, 2.0));
    dump_v3("nep.bez.nan", np(0.0 / 0.0, 0.5));
    dump_v3("nep.bez.inf", np(1.0 / 0.0, 0.5));
}
