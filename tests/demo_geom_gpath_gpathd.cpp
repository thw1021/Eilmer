// tests/demo_geom_gpath_gpathd.cpp — port of geom/gpath/gpath_demo.d (99 lines).
//
// stdout is byte-compared against the D binary's captured text,
// baselines/e587b235/probes/geom_gpath_gpath_demo.txt, through
// harness/compare_text.sh (CTest name demo_equiv_geom_gpath_gpathd).
//
// The _gpathd name (rather than the bare module name) keeps the target unique: the
// wave-02 brief also lists "the two gpath demos" under
// cmake/wave2_geom_gpath2.cmake, and two add_executable() calls for the same target
// would break configuration for every stage. This demo is the one that exercises
// the stage-1 modules (Path, Line, Arc, Arc3, Bezier, Polynomial).
//
// D source order is preserved line for line. Two spelling differences, both
// behaviour-preserving:
//   * D's `import geom;` becomes the module headers the demo actually resolves
//     (path/line/arc/bezier/helix/polyline/modifiedpath and elements.vector3); the
//     aggregate package header has no C++ counterpart (wave-02-brief section 4
//     item 5).
//   * `writeln(x)` finds a class's toString() through D's opPrint; each print site
//     here calls .toString() explicitly, which emits the same bytes (the convention
//     of tests/demo_geom_elements_vector3.cpp:12-14).
//
// Helix, Polyline, ArcLengthParameterizedPath, SubRangedPath and ReversedPath are
// stage-2/3 modules of this directory; their translation units are attached to this
// executable by cmake/wave2_geom_gpath.cmake, so the demo also proves the virtual
// Path contract holds across the whole hierarchy (note the "rp=" line: D's
// ReversedPath does not override toString(), so it prints as a SubRangedPath).

#include <cstdio>
#include <memory>

#include "eil/eil.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/arc.hpp"
#include "geom/gpath/bezier.hpp"
#include "geom/gpath/helix.hpp"
#include "geom/gpath/line.hpp"
#include "geom/gpath/modifiedpath.hpp"
#include "geom/gpath/path.hpp"
#include "geom/gpath/polyline.hpp"
#include "nm/number.hpp"

using eil::geom::Arc;
using eil::geom::Arc3;
using eil::geom::ArcLengthParameterizedPath;
using eil::geom::Bezier;
using eil::geom::Helix;
using eil::geom::Line;
using eil::geom::Path;
using eil::geom::PathList;
using eil::geom::PathPtr;
using eil::geom::Polyline;
using eil::geom::ReversedPath;
using eil::geom::SubRangedPath;
using eil::geom::Vector3;
using eil::number;

int main() {
    eil::writeln("Begin demonstration of the geometric Path primitives.");
    Vector3 a = Vector3(eil::Array<number>{1.0, 2.2, 3.0}.all());
    Vector3 b = Vector3(1.0);
    eil::writeln("a = ", a.toString(), ", b = ", b.toString());
    Line ab = Line(a, b);
    eil::writeln("ab= ", ab.toString());
    Vector3 c = ab(0.5);
    eil::writeln("ab(0.5)= ", c.toString());

    eil::writeln("Arc");
    a = Vector3(eil::Array<number>{2.0, 2.0, 0.0}.all());
    b = Vector3(eil::Array<number>{1.0, 2.0, 1.0}.all());
    c = Vector3(eil::Array<number>{1.0, 2.0, 0.0}.all());
    Arc abc = Arc(a, b, c);
    eil::writeln("abc= ", abc.toString());
    Vector3 d = abc(0.5);
    eil::writeln("abc(0.5)= ", d.toString());

    eil::writeln("Arc3");
    Arc3 adb = Arc3(a, d, b);
    Vector3 adbmid = adb(0.5);
    eil::writeln("adb(0.5)= ", adbmid.toString());

    eil::writeln("Helix");
    Vector3 axis0 = Vector3(eil::Array<number>{0.0, 0.0, 0.0}.all());
    Vector3 axis1 = Vector3(eil::Array<number>{1.0, 0.0, 0.0}.all());
    Vector3 pstart = Vector3(eil::Array<number>{0.0, 1.0, 0.0}.all());
    Vector3 pend = Vector3(eil::Array<number>{1.0, 0.0, 1.0}.all());
    Helix h1 = Helix(pstart, pend, axis0, axis1);
    eil::writeln("helix= ", h1.toString());
    Vector3 hmid = h1(0.5);
    eil::writeln("helix(0.5)=", hmid.toString());

    eil::writeln("Bezier");
    Bezier bez_adb = Bezier(eil::Array<Vector3>{a, d, b});
    eil::writeln("Bezier adb= ", bez_adb.toString());
    Vector3 e = bez_adb(0.5);
    eil::writeln("bez_adb(0.5)=", e.toString());

    eil::writeln("Polyline");
    // D holds class references, so the Polyline handle below is shared with the
    // SubRangedPath/ReversedPath that wrap it (modifiedpath.d dups it on the way in,
    // exactly as it does in D).
    PathList segs = {std::make_shared<Arc>(a, b, c), std::make_shared<Line>(b, c)};
    PathPtr polyline = std::make_shared<Polyline>(segs);
    eil::writeln("polyline= ", polyline->toString());
    Vector3 q25 = (*polyline)(0.25);
    Vector3 q50 = (*polyline)(0.50);
    Vector3 q75 = (*polyline)(0.75);
    eil::writeln("polyline(0.25)= ", q25.toString());
    eil::writeln("polyline(0.50)= ", q50.toString());
    eil::writeln("polyline(0.75)= ", q75.toString());

    eil::writeln("ArcLengthParameterizedPath");
    Vector3 p0 = Vector3(eil::Array<number>{0.0, 0.0, 0.0}.all());
    Vector3 p1 = Vector3(eil::Array<number>{1.0, 1.0, 1.0}.all());
    Vector3 p2 = Vector3(eil::Array<number>{4.0, 4.0, 4.0}.all());
    ArcLengthParameterizedPath alpp =
        ArcLengthParameterizedPath(std::make_shared<Bezier>(eil::Array<Vector3>{p0, p1, p2}));
    eil::writeln("alpp= ", alpp.toString());
    Vector3 alppmid = alpp(0.5);
    eil::writeln("alpp(0.5)= ", alppmid.toString());

    eil::writeln("SubRangedPath");
    SubRangedPath srp = SubRangedPath(polyline, 1.0, 0.0); // effectively reversed
    eil::writeln("srp= ", srp.toString());
    Vector3 s25 = srp(0.25);
    Vector3 s50 = srp(0.50);
    Vector3 s75 = srp(0.75);
    eil::writeln("srp(0.25)= ", s25.toString());
    eil::writeln("srp(0.50)= ", s50.toString());
    eil::writeln("srp(0.75)= ", s75.toString());

    eil::writeln("ReversedPath");
    ReversedPath rp = ReversedPath(polyline);
    eil::writeln("rp= ", rp.toString());
    Vector3 r25 = rp(0.25);
    Vector3 r50 = rp(0.50);
    Vector3 r75 = rp(0.75);
    eil::writeln("rp(0.25)= ", r25.toString());
    eil::writeln("rp(0.50)= ", r50.toString());
    eil::writeln("rp(0.75)= ", r75.toString());

    eil::writeln("Spline (Polyline)");
    // A rough circle.
    eil::Array<Vector3> pnts = {
        Vector3(eil::Array<number>{0.0, -1.0, 0.0}.all()),
        Vector3(eil::Array<number>{-1.0, 0.0, 0.0}.all()),
        Vector3(eil::Array<number>{0.0, 1.0, 0.0}.all()),
        Vector3(eil::Array<number>{1.0, 0.0, 0.0}.all()),
        Vector3(eil::Array<number>{0.0, -1.0, 0.0}.all())};
    Polyline circle = Polyline(pnts);
    eil::writeln("circle= ", circle.toString());
    Vector3 cmid = circle(5.0 / 8); // approx 45 degrees
    eil::writeln("circle(5.0/8)= ", cmid.toString());

    // Test closestDistance method.
    ab.p0 = Vector3(0.0, 0.0, 0.0);
    ab.p1 = Vector3(1.0, 0.0, 0.0);
    c = Vector3(0.5, 0.5, 0.0);
    double t = eil::d_init<double>();
    double dist = ab.closestDistance(c, t);
    (void)t; // D does not print the parameter here either (gpath_demo.d:95-96)
    eil::writeln("closest distance of point (0.5,0.5) to line (0.0,0.0) -> (1.0,0.0) is: ", dist);

    eil::writeln("Done gpath_demo.");
    return 0;
}
