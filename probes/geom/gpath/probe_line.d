// probes/geom/gpath/probe_line.d — what geom/gpath/line.d produces.
//
// Line overrides dpdt, d2pdt2, partial_length and point_from_length, so the
// dump_path rows compare the exact expressions against Path's finite differences
// (both are printed: Path::partial_length is reachable as pth.Path.partial_length
// in D and through the non-overriding siblings in the same dump). A port that
// dropped an override would still print plausible numbers, which is why the rows
// below include the base-version comparison explicitly.
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
//     -od/tmp/gp/obj -of=/tmp/gp/probe_line \
//     probes/geom/gpath/probe_line.d probes/geom/gpath/pathdump.d $ROOTS
//   /tmp/gp/probe_line > baselines/e587b235/probes/geom_gpath_line.txt
import std.conv;
import std.format;
import std.math;
import std.stdio;

import geom.elements;
import geom.gpath.path;
import geom.gpath.line;
import nm.number;
import pathdump;

void main()
{
    // The line from line.d's unittest (line.d:66-73).
    auto a = Vector3([1.0, 2.2, 3.0]);
    auto b = Vector3(1.0);
    auto ab = new Line(a, b);
    dump_path("line.ab", ab);
    dump_path("line.dup", ab.dup());

    // The horizontal line of the intersection test (line.d:76-83).
    auto pth = new Line(Vector3(0.0, 1.0), Vector3(1.0, 1.0));
    dump_path("line.horiz", pth);

    // Exact vs finite-difference. Path.dpdt at an interior t is a central
    // difference of opCall, which for a straight line equals p1 - p0 exactly only
    // to rounding; printing both tells the replay which one the port used.
    foreach (t; [0.0, 0.5, 1.0]) {
        Vector3 e = ab.dpdt(t);
        Vector3 f = ab.Path.dpdt(t); // the base implementation, via qualification
        writefln("line.cmp t=%.17g exact x=%.17g y=%.17g z=%.17g fd x=%.17g y=%.17g z=%.17g",
                 t, e.x, e.y, e.z, f.x, f.y, f.z);
        Vector3 e2 = ab.d2pdt2(t);
        Vector3 f2 = ab.Path.d2pdt2(t);
        writefln("line.cmp2 t=%.17g exact x=%.17g y=%.17g z=%.17g fd x=%.17g y=%.17g z=%.17g",
                 t, e2.x, e2.y, e2.z, f2.x, f2.y, f2.z);
    }

    // partial_length: exact |p1-p0|*|tb-ta| vs the 100-chord sum.
    writefln("line.pl exact=%.17g base=%.17g ta=%.17g tb=%.17g",
             ab.partial_length(0.2, 0.7), ab.Path.partial_length(0.2, 0.7), 0.2, 0.7);
    writefln("line.pl.swap exact=%.17g base=%.17g",
             ab.partial_length(0.7, 0.2), ab.Path.partial_length(0.7, 0.2));
    writefln("line.length exact=%.17g base=%.17g",
             ab.length(), ab.Path.length());

    // point_from_length, including a target beyond the end (t > 1 in D's formula,
    // which Line happily extrapolates because it does not clamp).
    Vector3 dab = b - a; // geom.abs takes ref const Vector3, so it needs a name
    double Lab = geom.elements.abs(dab);
    foreach (L; [0.0, 0.5 * Lab, Lab, 2.0 * Lab]) {
        double t;
        auto p = ab.point_from_length(L, t);
        writefln("line.pfl target=%.17g t=%.17g x=%.17g y=%.17g z=%.17g", L, t, p.x, p.y, p.z);
    }

    // A degenerate line (p0 == p1): partial_length divides by |dp| in
    // point_from_length, which is the 0/0 case.
    auto dd = new Line(Vector3(1.0, 2.0, 3.0), Vector3(1.0, 2.0, 3.0));
    double tz;
    auto pz = dd.point_from_length(0.0, tz);
    writefln("line.degen pfl t=%.17g x=%.17g y=%.17g z=%.17g", tz, pz.x, pz.y, pz.z);
    writefln("line.degen length=%.17g pl=%.17g", dd.length(), dd.partial_length(0.0, 1.0));
    dump_path("line.degen", dd);

    // The copy constructor (line.d:23-26) through a base handle.
    auto cp = new Line(ab);
    dump_path("line.copy", cp);
    writefln("line.copy str=%s", cp.toString());
}
