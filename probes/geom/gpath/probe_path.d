// probes/geom/gpath/probe_path.d — what geom/gpath/path.d's own code produces.
//
// path.d is the abstract base, so its six concrete methods (dpdt, d2pdt2,
// partial_length, length, point_from_length, intersect2D, closestDistance) can
// only be measured through a subclass. The Stub below overrides *only* the four
// abstract ones (dup, opCall, toString, classString), so every other call in the
// dump goes through path.d's code — the finite-difference stencils with their
// three branches, the 100-chord and 1000-chord sums, the tb<ta swap, the
// `cast(Line)` special case and the golden-section search over
// distanceToPoint (path.d:154-158).
//
// The Stub's opCall is curved in all three components (x = t, y = t^^2,
// z = sin(PI*t)) so the one-sided and central differences differ, and so the
// `t^^2` (double ^^ int, measured to return double) and the 80-bit PI path are
// both exercised the way path.d's consumers use them.
//
// REFERENCE BUILD NOTE (applies to every probe in this directory): the reference
// is built from an EXPLICIT MODULE FILE LIST against the read-only gdtk tree, not
// with `-i`. `ldc2 -i -I<gdtk>/src` fails with
//   nm/bbla.d(709): Error: cannot use `new` in `@nogc` function
//       gaussJordanElimination!double
// because path.d:27 imports nm.bbla (and geom.surface drags in nm/spline.d, which
// instantiates the same @nogc template). Making those modules *imports* instead of
// *root modules* keeps them in the semantic pass only, where the offending
// instantiation is never emitted, and the unmodified tree then compiles and links.
// Verified: every module below is listed as a root TU, which is also what satisfies
// the ModuleInfo references path.d's `import geom.surface` creates. Nothing under
// /home/tang/packages/gdtk is written; all output goes to /tmp.
//
//   G=/home/tang/packages/gdtk/src
//   LIST=$(ls $G/geom/elements/*.d $G/geom/gpath/*.d $G/geom/grid/*.d \
//             $G/geom/misc/*.d $G/geom/surface/*.d $G/geom/volume/*.d $G/nm/*.d \
//             $G/ntypes/*.d $G/geom/package.d | grep -v '_demo\.d')
//   ROOTS=""; for f in $LIST; do                       # drop modules with a main()
//     grep -qE '^[[:space:]]*(void|int)[[:space:]]+main[[:space:]]*\(' "$f" || \
//       ROOTS="$ROOTS $f"; done
//   ROOTS="$ROOTS $G/geom/geometry_exception.d $G/extern/gzip/gzip.d"
//   /home/tang/packages/dlang/bin/ldc2 -w -g -dip1008 -I$G -I$G/extern/gzip \
//     -od/tmp/gp/obj -of=/tmp/gp/probe_path \
//     probes/geom/gpath/probe_path.d probes/geom/gpath/pathdump.d $ROOTS
//   /tmp/gp/probe_path > baselines/e587b235/probes/geom_gpath_path.txt
//
// (The same command with probe_<module>.d builds the other four captures; the
// comment in each of those files repeats it.)
import std.conv;
import std.math;
import std.format;
import std.stdio;

import geom.elements;
import geom.gpath.path;
import geom.gpath.line;
import nm.number;
import pathdump;

class Stub : Path {
    override Path dup() const { return new Stub(); }
    override Vector3 opCall(double t) const {
        double y = t ^^ 2;
        real zr = sin(PI * t); // 80-bit, rounded only when stored
        return Vector3(t, y, to!number(zr));
    }
    override string toString() const { return "Stub()"; }
    override string classString() const { return "Stub"; }
} // end class Stub

void main()
{
    dump_path("path.stub", new Stub());

    // A Line through the same dump: Line overrides dpdt, d2pdt2, partial_length
    // and point_from_length, so these rows are the *other* side of the virtual
    // surface — Path's versions must NOT run for a Line. The intersect2D rows also
    // take the `cast(Line)` branch (nseg forced to 1, path.d:136).
    dump_path("path.line", new Line(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0)));

    // dup() through the base handle: the copy must reproduce the original exactly.
    auto s = new Stub();
    Path sd = s.dup();
    writefln("path.dup.class=%s", sd.classString());
    dump_path("path.dup", sd);

    // closestDistance at the two special cases path.d:166-172 branches on: a point
    // in front of t=0 and one past t=1 of a straight segment. Each call is a
    // separate statement: the argument order inside writefln would be unspecified
    // because closestDistance writes t while the row also prints it.
    auto ab = new Line(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
    double t0, t1, tm;
    double d0 = ab.closestDistance(Vector3(-1.0, 0.5, 0.0), t0);
    writefln("path.cd.before t=%.17g dist=%.17g", t0, d0);
    double d1 = ab.closestDistance(Vector3(2.0, 0.5, 0.0), t1);
    writefln("path.cd.after  t=%.17g dist=%.17g", t1, d1);
    double dm = ab.closestDistance(Vector3(0.5, 0.5, 0.0), tm);
    writefln("path.cd.middle t=%.17g dist=%.17g", tm, dm);

    // point_from_length past the end (the loop never exceeds the target, so t ends
    // up at dt*n = 1.0 and p1 is the last sample).
    double tp;
    auto pe = ab.point_from_length(1.0e3, tp);
    writefln("path.pfl.over t=%.17g x=%.17g y=%.17g z=%.17g", tp, pe.x, pe.y, pe.z);
    double tz;
    auto pz = ab.point_from_length(0.0, tz);
    writefln("path.pfl.zero t=%.17g x=%.17g y=%.17g z=%.17g", tz, pz.x, pz.y, pz.z);

    // D's `out double t` (path.d:124, :102) resets the *caller's* variable to
    // double.init (NaN) on entry, before the body runs — measured: an empty
    // `void set(out double x) {}` leaves a caller holding 3.25 printing nan. The
    // difference is visible exactly where intersect2D returns false without ever
    // writing t (path.d:149), so this row reuses a variable that still holds a
    // valid t from the successful call above and fires a ray parallel to the
    // segment, which makes every geom.intersect2D call at path.d:142 return false.
    // A port that treats the parameter as a plain reference leaves 0.500000697...
    // in place and prints that instead of nan.
    double tre = tm;
    bool fpar = ab.intersect2D(Vector3(0.5, 0.5, 0.0), Vector3(1.0, 0.0, 0.0), tre);
    writefln("path.i2d.parallel found=%s t=%.17g", fpar, tre);
    // ...and the same variable through a successful call, to show it is written back.
    bool fnom = ab.intersect2D(Vector3(0.5, -1.0, 0.0), Vector3(0.0, 1.0, 0.0), tre);
    writefln("path.i2d.afterpar found=%s t=%.17g", fnom, tre);
}
