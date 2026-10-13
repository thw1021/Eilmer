// probes/geom/gpath/probe_bezier.d — what geom/gpath/bezier.d produces.
//
// Bezier evaluates by de Casteljau (bezier.d:123-135) and overrides dpdt/d2pdt2
// with the *derivative curves* C and D, which set_deriv_control_points() builds
// from B (bezier.d:103-122). The C/D arrays are protected in D, so the probe reads
// them through the exact derivatives they encode — dpdt(t) and d2pdt2(t) at the
// same t values — and prints elevateDegree() before/after, because that routine
// rewrites B and must rebuild C and D (bezier.d:97).
//
// Degree elevation is the site where a port can drift: D grows B_star by
// .length=, blends with `alpha = to!double(i)/(n+1)` and then copies B_star back
// into B in a separate loop. The order is preserved, so the elevated control
// points print bit-identically (see the "bezier.elev*" rows).
//
// See probe_path.d for the reference build recipe (explicit module file list
// against the read-only gdtk tree; no @nogc edits, no `-i`). In short:
//   G=/home/tang/packages/gdtk/src
//   LIST=$(ls $G/geom/elements/*.d $G/geom/gpath/*.d $G/geom/grid/*.d \
//             $G/geom/misc/*.d $G/geom/surface/*.d $G/geom/volume/*.d $G/nm/*.d \
//             $G/ntypes/*.d $G/geom/package.d | grep -v '_demo\.d')
//   ROOTS=""; for f in $LIST; do
//     grep -qE '^[[:space:]]*(void|int)[[:space:]]+main[[:space:]]*\(' "$f" || \
//       ROOTS="$ROOTS $f"; done
//   ROOTS="$ROOTS $G/geom/geometry_exception.d $G/extern/gzip/gzip.d"
//   /home/tang/packages/dlang/bin/ldc2 -w -g -dip1008 -I$G -I$G/extern/gzip \
//     -od/tmp/gp/obj -of=/tmp/gp/probe_bezier \
//     probes/geom/gpath/probe_bezier.d probes/geom/gpath/pathdump.d $ROOTS
//   /tmp/gp/probe_bezier > baselines/e587b235/probes/geom_gpath_bezier.txt
import std.conv;
import std.format;
import std.math;
import std.stdio;

import geom.elements;
import geom.gpath.path;
import geom.gpath.arc;
import geom.gpath.bezier;
import nm.number;
import pathdump;

void dump_points(const char[] tag, const Vector3[] B)
{
    foreach (i; 0 .. B.length) {
        writefln("%s B[%d] x=%.17g y=%.17g z=%.17g", tag, i, B[i].x, B[i].y, B[i].z);
    }
    writefln("%s npts=%d", tag, B.length);
}

// C and D (the derivative and second-derivative control polygons, bezier.d:101-102)
// are protected, so they cannot be printed directly. de Casteljau at t = 0 and
// t = 1 returns exactly the first and last point of the polygon it walks, so
// dpdt(0)/dpdt(1) and d2pdt2(0)/d2pdt2(1) pin both ends of C and D without opening
// the class — the four values a wrong set_deriv_control_points() gets wrong first.
void dump_deriv(const char[] tag, const Bezier bz)
{
    foreach (t; [0.0, 1.0]) {
        Vector3 d1 = bz.dpdt(t);
        Vector3 d2 = bz.d2pdt2(t);
        writefln("%s deriv t=%.17g d1 x=%.17g y=%.17g z=%.17g d2 x=%.17g y=%.17g z=%.17g",
                 tag, t, d1.x, d1.y, d1.z, d2.x, d2.y, d2.z);
    }
}

void main()
{
    // bezier.d:141-158 — the arc-derived cubic-ish curve of the unittest.
    auto a = Vector3([2.0, 2.0, 0.0]);
    auto b = Vector3([1.0, 2.0, 1.0]);
    auto c = Vector3([1.0, 2.0, 0.0]);
    auto abc = new Arc(a, b, c);
    auto d = abc(0.5);
    auto adb = new Bezier([a, d, b]);
    dump_path("bezier.quad", adb);
    dump_points("bezier.quad", adb.B);
    dump_deriv("bezier.quad", adb);

    // Linear Bezier: C has one point, D is the single Vector3(0) of bezier.d:115.
    auto ab = new Bezier([a, b]);
    dump_path("bezier.linear", ab);
    dump_points("bezier.linear", ab.B);
    dump_deriv("bezier.linear", ab);
    writefln("bezier.linear dpdt(0.5) x=%.17g y=%.17g z=%.17g",
             ab.dpdt(0.5).x, ab.dpdt(0.5).y, ab.dpdt(0.5).z);
    writefln("bezier.linear d2pdt2(0.5) x=%.17g y=%.17g z=%.17g",
             ab.d2pdt2(0.5).x, ab.d2pdt2(0.5).y, ab.d2pdt2(0.5).z);

    // The quadratic through c (bezier.d:152-154).
    auto acb = new Bezier([a, c, b]);
    dump_path("bezier.acb", acb);
    dump_deriv("bezier.acb", acb);
    writefln("bezier.acb dpdt(0.5) x=%.17g y=%.17g z=%.17g",
             acb.dpdt(0.5).x, acb.dpdt(0.5).y, acb.dpdt(0.5).z);
    writefln("bezier.acb d2pdt2(0.5) x=%.17g y=%.17g z=%.17g",
             acb.d2pdt2(0.5).x, acb.d2pdt2(0.5).y, acb.d2pdt2(0.5).z);

    // Degree elevation, from the unittest (bezier.d:156-158), and further.
    auto adbCopy = cast(Bezier) adb.dup();
    adbCopy.elevateDegree(4);
    dump_points("bezier.elev4", adbCopy.B);
    dump_deriv("bezier.elev4", adbCopy);
    dump_path("bezier.elev4", adbCopy);
    writefln("bezier.elev4 mid cmp adb=%.17g %.17g %.17g elev=%.17g %.17g %.17g",
             adb(0.5).x, adb(0.5).y, adb(0.5).z, adbCopy(0.5).x, adbCopy(0.5).y, adbCopy(0.5).z);
    auto big = new Bezier([a, c, b]);
    big.elevateDegree(7);
    dump_points("bezier.elev7", big.B);
    dump_deriv("bezier.elev7", big);
    dump_path("bezier.elev7", big);
    // Elevating to the current degree returns immediately (bezier.d:76-77).
    auto same = new Bezier([a, c, b]);
    same.elevateDegree(2);
    dump_points("bezier.elevsame", same.B);
    dump_deriv("bezier.elevsame", same);

    // A cubic, the shape the surface patches use.
    auto P0 = Vector3([2.0, 2.0, 0.0]);
    auto P1 = Vector3([1.0, 2.0, 1.0]);
    auto P2 = Vector3([1.0, 2.0, 0.0]);
    auto P3 = Vector3([2.0, 2.0, 1.0]);
    auto cub = new Bezier([P0, P1, P2, P3]);
    dump_path("bezier.cubic", cub);
    dump_points("bezier.cubic", cub.B);
    dump_deriv("bezier.cubic", cub);

    // Error paths (bezier.d:22-25, :73 and :108).
    dump_error("bezier.err.nopoints", new Bezier(cast(Vector3[])[]));
    dump_error("bezier.err.onepoint", new Bezier([a]));
    auto low = new Bezier([a, c, b]);
    dump_error("bezier.err.lowerdegree", low.elevateDegree(1));
    writefln("bezier.str=%s", cub.toString());
}
