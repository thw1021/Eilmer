// probes/geom/gpath/probe_arcsteps.d — two things, in this order:
//
//  1. THE atan2 / atan CORPUS. D's std.math.atan2(double,double) is NOT libm's:
//     std/math/trigonometry.d:945 sends it to atan2Impl!double, which does its own
//     quadrant reduction and calls atanImpl!double (the CEPHES/fdlibm rational
//     approximation D keeps in pure source). glibc uses the IBM Accurate
//     Mathematical Engine and the two differ by 1 ULP near the y-axis — measured on
//     this machine: atan2(1.0, -1.6653345369377348e-16) is 1.5707963267948966 in D
//     and 1.5707963267948968 in glibc. arc.d:92 calls atan2 once per point and its
//     result feeds theta, L, cos/sin and both frame transforms, so that ULP moves
//     every number the Arc module prints. These rows pin D's values over the special
//     cases and all three range-reduction branches.
//
//  2. THE STEP-BY-STEP ARCS dump. The intermediates of
//     Arc.evaluate_position_and_length (arc.d:58-101), recomputed from outside the
//     class with the same public Vector3 operations. This is what localised the
//     divergence above: arc3.thru (centre (1, 2, -5.55e-17)) diverged from D in the
//     last bit while arc.qtr (centre exactly on the axis) agreed everywhere, and the
//     first row that differs is the theta one — every value before it agrees.
//
// Build (same explicit-list recipe as probe_path.d; this probe needs no pathdump.d):
//   G=/home/tang/packages/gdtk/src
//   LIST=$(ls $G/geom/elements/*.d $G/geom/gpath/*.d $G/geom/grid/*.d \
//             $G/geom/misc/*.d $G/geom/surface/*.d $G/geom/volume/*.d $G/nm/*.d \
//             $G/ntypes/*.d $G/geom/package.d | grep -v '_demo\.d')
//   ROOTS=""; for f in $LIST; do
//     grep -qE '^[[:space:]]*(void|int)[[:space:]]+main[[:space:]]*\(' "$f" || \
//       ROOTS="$ROOTS $f"; done
//   ROOTS="$ROOTS $G/geom/geometry_exception.d $G/extern/gzip/gzip.d"
//   /home/tang/packages/dlang/bin/ldc2 -w -g -dip1008 -I$G -I$G/extern/gzip \
//     -od/tmp/gp/obj -of=/tmp/gp/probe_arcsteps \
//     probes/geom/gpath/probe_arcsteps.d $ROOTS
//   /tmp/gp/probe_arcsteps > baselines/e587b235/probes/geom_gpath_arcsteps.txt
import std.conv;
import std.format;
import std.math;
import std.stdio;

import geom.elements;
import geom.gpath.arc;
import nm.number;

void v(string tag, Vector3 p)
{
    writefln("%s x=%.17g y=%.17g z=%.17g", tag, p.x, p.y, p.z);
}

void atan_table()
{
    // y, x. The set covers: both zeros with each sign, the axes, all four
    // quadrants, both infinities, NaN, the tiny-negative-x case arc3.thru actually
    // evaluates, and arguments on each side of the 0.66 and tan(3*PI/8) cuts of
    // atanImpl's double branch.
    double[2][] pairs = [
        [1.0, -1.6653345369377348e-16],
        [1.0, 0.0],
        [-1.0, 0.0],
        [0.0, 1.0],
        [-0.0, 1.0],
        [0.0, -1.0],
        [-0.0, -1.0],
        [1.0, 1.0],
        [1.0, -1.0],
        [-1.0, 1.0],
        [-1.0, -1.0],
        [1.0, sqrt(3.0)],
        [0.5, 0.25],
        [0.66, 1.0],
        [0.67, 1.0],
        [0.4142135623730951, 1.0],
        [0.4142135623730950, 1.0],
        [2.414213562373095, 1.0],
        [2.4142135623730951, 1.0],
        [3.0, 1.0],
        [1.0, 3.0],
        [1.0e-300, 1.0],
        [1.0, 1.0e-300],
        [1.0e-300, 1.0e-300],
        [1.0e300, 1.0e-300],
        [1.0e-300, 1.0e300],
        [2.0, 1.7],
        [-0.3, 1.2],
        [1.2, -0.3],
        [-1.2, -0.3],
        [0.7071067811865476, 0.7071067811865475],
        [1.0, double.infinity],
        [1.0, -double.infinity],
        [double.infinity, 1.0],
        [-double.infinity, 1.0],
        [double.infinity, double.infinity],
        [-double.infinity, -double.infinity],
        [double.nan, 1.0],
        [1.0, double.nan],
        [0.99999815799335934, 1.8420066406622953e-06],
        [-5.551115123125783e-17, 1.0]
    ];
    foreach (p; pairs) {
        writefln("atan2 y=%.17g x=%.17g v=%.17g ay=%.17g ax=%.17g",
                 p[0], p[1], atan2(p[0], p[1]), atan(p[0]), atan(p[1]));
    }
}

void steps(string tag, Vector3 a, Vector3 b, Vector3 c)
{
    Vector3 ca = a - c;
    Vector3 cb = b - c;
    number ca_mag = geom.abs(ca);
    number cb_mag = geom.abs(cb);
    writefln("%s ca_mag=%.17g cb_mag=%.17g diff=%.17g", tag, ca_mag, cb_mag,
             fabs(ca_mag - cb_mag));
    v(tag ~ ".ca", ca);
    v(tag ~ ".cb", cb);

    Vector3 tangent1 = Vector3(ca);
    tangent1.normalize();
    v(tag ~ ".t1", tangent1);

    Vector3 n = cross(ca, cb);
    v(tag ~ ".cr", n);
    writefln("%s nabs=%.17g", tag, geom.abs(n));
    n.normalize();
    v(tag ~ ".n", n);

    Vector3 tangent2 = cross(n, tangent1);
    v(tag ~ ".t2", tangent2);

    Vector3 cb_local = cb;
    Vector3 zerov = Vector3(0.0, 0.0, 0.0);
    cb_local.transform_to_local_frame(tangent1, tangent2, n, zerov);
    v(tag ~ ".cbl", cb_local);

    number theta = atan2(cb_local.y, cb_local.x);
    number L = theta * cb_mag;
    writefln("%s theta=%.17g L=%.17g", tag, theta, L);

    foreach (tt; [0.0, 0.0005, 0.001, 0.5]) {
        number th = theta * tt;
        number cx = cos(th) * cb_mag;
        number sy = sin(th) * cb_mag;
        writefln("%s loc t=%.17g cx=%.17g sy=%.17g", tag, tt, cx, sy);
        Vector3 loc = Vector3(cx, sy, to!number(0.0));
        loc.transform_to_global_frame(tangent1, tangent2, n, c);
        v(tag ~ ".loc", loc);
    }
}

void main()
{
    atan_table();

    Vector3 a = Vector3([2.0, 2.0, 0.0]);
    Vector3 b = Vector3([1.0, 2.0, 1.0]);
    Vector3 cex = Vector3([1.0, 2.0, 0.0]);
    steps("qtr", a, b, cex);

    Arc abc = new Arc(a, b, cex);
    Vector3 d = abc(0.5);
    Arc3 adb = new Arc3(a, d, b);
    v("thru.a", adb.a);
    v("thru.b", adb.b);
    v("thru.c", adb.c);
    steps("thru", adb.a, adb.b, adb.c);

    // The same centre with the 5.55e-17 z component forced to zero: separates "the
    // centre is off-axis" from "the arithmetic order differs".
    steps("thru.c0", a, b, Vector3(adb.c.x, adb.c.y, 0.0));

    // The shallow Arc3 of arc.d:176-183, whose centre is exact in z.
    Arc3 sh = new Arc3(Vector3(0.0, 1.0), Vector3(0.5, 1.2), Vector3(1.0, 1.0));
    steps("shallow", sh.a, sh.b, sh.c);
}
