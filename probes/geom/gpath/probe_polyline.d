// probes/geom/gpath/probe_polyline.d — what geom/gpath/polyline.d produces.
//
// Polyline is the composite: its constructor dups every segment, its breakpoint
// table divides by each segment's own virtual length() (so an Arc contributes the
// geometric length and a Line the exact one), and opCall maps t onto a segment and
// then onto that segment's local parameter. The rows below print t_values at
// %.17g before the swept dump, because a wrong breakpoint table moves *every*
// opCall row and the two causes are separable only if the table is visible.
//
// The spline constructor (polyline.d:53-108) is dumped for an open and a closed
// point set: the Gauss-Seidel loop, its 50-iteration cap and its
// `max_diff < tolerance` break are all in the resulting Bezier control points,
// which the probe prints explicitly (rows "poly.circle B[i]").
//
// toGmshString is dumped verbatim for Line, Bezier, Arc and unsupported segment
// types, open and closed, because its %g text (and the polyline.d:204 int.init
// case) is part of what a .geo file comparison sees.
//
// See probe_path.d for why the reference tree is copied before building.
//
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/gpath \
//     probes/geom/gpath/probe_polyline.d -of=/tmp/x && /tmp/x \
//     > baselines/e587b235/probes/geom_gpath_polyline.txt
//
// The file rows are run from the Eilmer repository root, so the relative file
// names below resolve; the data files live in tests/test_data/.
import std.conv;
import std.format;
import std.math;
import std.stdio;

import geom.elements;
import geom.gpath.path;
import geom.gpath.line;
import geom.gpath.arc;
import geom.gpath.bezier;
import geom.gpath.polyline;
import nm.number;
import pathdump;

void dump_breaks(const char[] tag, const Polyline pl)
{
    writefln("%s nseg=%d closed=%s", tag, pl.segments.length, pl.closed);
    foreach (i; 0 .. pl.segments.length) {
        writefln("%s seg[%d] class=%s len=%.17g", tag, i, pl.segments[i].classString(),
                 pl.segments[i].length());
    }
    double tsum = 0.0;
    foreach (i; 0 .. pl.t_values.length) {
        writefln("%s t_values[%d]=%.17g", tag, i, pl.t_values[i]);
        tsum += pl.t_values[i];
    }
    writefln("%s t_sum=%.17g", tag, tsum);
}

void dump_string(const char[] tag, string s)
{
    // The Gmsh text is written verbatim; the tag line lets the replay tell which
    // block of lines belongs to which call.
    writefln("%s begin", tag);
    write(s);
    writefln("%s end", tag);
}

void main()
{
    // polyline.d:294-303 — Arc + Line, and the same closed.
    auto a = Vector3([2.0, 2.0, 0.0]);
    auto b = Vector3([1.0, 2.0, 1.0]);
    auto c = Vector3([1.0, 2.0, 0.0]);
    auto abc = new Arc(a, b, c);
    auto polyline1 = new Polyline([cast(Path) abc, cast(Path) new Line(b, c)]);
    dump_breaks("poly.two", polyline1);
    dump_path("poly.two", polyline1);

    auto polyline2 = new Polyline(polyline1, true);
    dump_breaks("poly.closed", polyline2);
    dump_path("poly.closed", polyline2);
    writefln("poly.closed g(0.999999) x=%.17g y=%.17g z=%.17g",
             polyline2(0.999999).x, polyline2(0.999999).y, polyline2(0.999999).z);

    // The spline-through-points constructor: the rough circle of gpath_demo.d.
    auto pnts = [Vector3([0.0, -1.0, 0.0]),
                 Vector3([-1.0, 0.0, 0.0]),
                 Vector3([0.0, 1.0, 0.0]),
                 Vector3([1.0, 0.0, 0.0]),
                 Vector3([0.0, -1.0, 0.0])];
    auto circle = new Polyline(pnts);
    dump_breaks("poly.circle", circle);
    dump_path("poly.circle", circle);
    foreach (i; 0 .. circle.segments.length) {
        auto bz = cast(Bezier) circle.segments[i];
        foreach (j; 0 .. bz.B.length) {
            writefln("poly.circle seg[%d] B[%d] x=%.17g y=%.17g z=%.17g",
                     i, j, bz.B[j].x, bz.B[j].y, bz.B[j].z);
        }
    }

    // The same points, closed with the isClosed flag (which appends p[0] only when
    // the ends are further apart than tolerance — here they are equal, so no point
    // is added and no closing Line is created either).
    auto openEnds = [Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.3, 0.0),
                     Vector3(2.0, -0.2, 0.0), Vector3(3.0, 0.9, 0.0)];
    auto spline1 = new Polyline(openEnds, true);
    dump_breaks("poly.spline.closed", spline1);
    dump_path("poly.spline.closed", spline1);
    auto spline0 = new Polyline(openEnds, false);
    dump_breaks("poly.spline.open", spline0);
    dump_path("poly.spline.open", spline0);

    // A Bezier + Line + unsupported-type polyline for toGmshString.
    auto bez = new Bezier([Vector3(0.0, 0.0, 0.0), Vector3(0.5, 1.0, 0.0), Vector3(1.0, 0.0, 0.0)]);
    Path[] mix = [cast(Path) bez, cast(Path) new Line(Vector3(1.0, 0.0, 0.0), Vector3(1.0, 1.0, 0.0))];
    auto mixed = new Polyline(mix);
    int pt = 0, ct = 0, lt = 0;
    dump_string("poly.gmsh.mixed", mixed.toGmshString(pt, ct, lt, "mixed", 1.0e-2));
    writefln("poly.gmsh.mixed tags pointTag=%d curveTag=%d loopTag=%d", pt, ct, lt);
    // A second call continues the tag numbering from the first (that is the reason
    // the tags are `ref int` in D).
    dump_string("poly.gmsh.mixed2", mixed.toGmshString(pt, ct, lt, "again"));
    writefln("poly.gmsh.mixed2 tags pointTag=%d curveTag=%d loopTag=%d", pt, ct, lt);

    // Closed polyline with a Line that closes the loop, and the Arc branch.
    int pt2 = 0, ct2 = 0, lt2 = 0;
    dump_string("poly.gmsh.closed", polyline2.toGmshString(pt2, ct2, lt2, "closed", 0.05));
    writefln("poly.gmsh.closed tags pointTag=%d curveTag=%d loopTag=%d", pt2, ct2, lt2);
    // A segment type toGmshString does not handle, to reach the default branch
    // (polyline.d:260-261) and its "not handled" text.
    int pt3 = 0, ct3 = 0, lt3 = 0;
    Path[] withodd = [cast(Path) new HelixPlaceholder(),
                      cast(Path) new Line(Vector3(1.0, 0.0, 0.0), Vector3(2.0, 1.0, 0.0))];
    auto odd = new Polyline(withodd);
    dump_string("poly.gmsh.unhandled", odd.toGmshString(pt3, ct3, lt3, "odd"));
    writefln("poly.gmsh.unhandled tags pointTag=%d curveTag=%d loopTag=%d", pt3, ct3, lt3);

    // A polyline whose last segment ends exactly on the first point (the tag1
    // branch of polyline.d:204-215 with the closed path).
    int pt4 = 0, ct4 = 0, lt4 = 0;
    dump_string("poly.gmsh.circle", circle.toGmshString(pt4, ct4, lt4, "circle"));
    writefln("poly.gmsh.circle tags pointTag=%d curveTag=%d loopTag=%d", pt4, ct4, lt4);

    // The file constructor.
    auto fromfile = new Polyline("tests/test_data/gpath_points3d.dat");
    dump_breaks("poly.file", fromfile);
    dump_path("poly.file", fromfile);
    auto fromfilec = new Polyline("tests/test_data/gpath_points3d.dat", true);
    dump_breaks("poly.file.closed", fromfilec);

    // The malformed file: the "#c" line is not a comment by D's rule
    // (tokens[0] == "#"), so std.conv raises a ConversionException. Printed as a
    // taxonomy row only, without e.msg, because the port's message text for this
    // input class is wrong (measured: ldc2 gives `no digits seen for input "#c".`
    // for any first character that cannot start nan/infinity, while eil::to_double
    // gives `error converting input to floating point for input "#c".` -- a
    // substrate defect, see reports/backlog.md T-9xx and the header of
    // probes/geom/gpath/probe_xspline.d). The exception *kind* is the portable
    // part of this row; restoring the msg field is part of fixing the substrate.
    write("poly.file.bad asError=");
    try {
        auto bad = new Polyline("tests/test_data/gpath_bad.dat");
        writefln("false asException=false msg=none");
    } catch (Error e) {
        writefln("true asException=false kind=Error");
    } catch (Exception e) {
        writefln("false asException=true kind=Exception");
    }
    // An empty file yields no points, and the spline constructor then divides by
    // (p.length - 1) == 0 - 1 -- recorded, not fixed.
    dump_error("poly.file.empty", new Polyline("tests/test_data/gpath_empty.dat"));

    // Errors and copies.
    Path[] empty_segs;
    dump_error("poly.err.nosegments", new Polyline(empty_segs));
    dump_path("poly.dup", fromfile.dup());
    auto cp = new Polyline(mixed, false);
    dump_breaks("poly.copy", cp);
} // end main

class HelixPlaceholder : Path {
    // A segment type toGmshString does not handle, to reach the default branch
    // (polyline.d:260-261).
    override Path dup() const { return new HelixPlaceholder(); }
    override Vector3 opCall(double t) const { return Vector3(t, 0.0, 0.0); }
    override string toString() const { return "HelixPlaceholder()"; }
    override string classString() const { return "HelixPlaceholder"; }
}
