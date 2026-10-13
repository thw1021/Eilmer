// tests/test_geom_gpath_polyline.cpp
//
// Two layers, both required (the pattern of tests/test_geom_elements_properties.cpp):
//
//  A. the `unittest` block of geom/gpath/polyline.d (D:293-327) ported assertion
//     for assertion, in source order. The `version (complex_numbers)` block
//     (D:305-326) is carried under EIL_COMPLEX_NUMBERS and is not compiled in the
//     trunk build (backlog T-904).
//
//  B. a line-for-line replay of probes/geom/gpath/probe_polyline.d against
//     baselines/e587b235/probes/geom_gpath_polyline.txt: 693 lines covering the
//     segment bookkeeping (counts, per-segment classString() and length(), the
//     cumulative t_values table and its sum), the piecewise evaluation over the
//     shared t sweep of pathdump.d (which includes t = 0.0 and t = 1.0 and the two
//     values sitting on the finite-difference boundary), the closed-path variant
//     that appends a Line, the Gauss-Seidel spline-through-points constructor with
//     its resulting Bezier control points, the Gmsh writer verbatim for the Line /
//     Bezier / Arc / unhandled segment types (open and closed, two consecutive
//     calls to show the `ref int` tag chaining), the file constructor over
//     tests/test_data/gpath_points3d.dat, and the three error paths.
//
// Segment-boundary arithmetic is the reason the t_values rows are printed before
// each swept dump: Polyline::opCall (polyline.d:142-163) picks the segment with
// `t <= t_values[i]`, then maps to the local parameter, so a wrong table moves
// every opCall row and the two causes are only separable if the table is visible.
//
// Regenerate the baseline (full recipe in the probe header):
//   ldc2 -i -I/tmp/gdtk_shadow ... probes/geom/gpath/probe_polyline.d -of=/tmp/x
//   cd /home/tang/packages/Eilmer && /tmp/x > baselines/e587b235/probes/geom_gpath_polyline.txt
//
// Run: ./test_geom_gpath_polyline <baseline.txt> <tests/test_data dir>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/arc.hpp"
#include "geom/gpath/bezier.hpp"
#include "geom/gpath/line.hpp"
#include "geom/gpath/path.hpp"
#include "geom/gpath/polyline.hpp"
#include "nm/number.hpp"

#include "gpath3_dump.hpp"

using eil::geom::Arc;
using eil_test_gpath3::emitf;
using eil::geom::Bezier;
using eil::geom::Line;
using eil::geom::Path;
using eil::geom::PathPtr;
using eil::geom::Polyline;
using eil::geom::Vector3;

namespace {

std::string data_dir; // argv[2]; the probe reads its coordinate files from here

// A segment type Polyline::toGmshString does not handle, so that the default arm
// of polyline.d:260-261 ("// Segment type ... not handled.") is reached. The D
// probe declares the same class (probe_polyline.d:167-174).
class HelixPlaceholder : public Path {
  public:
    std::shared_ptr<Path> dup() const override { return std::make_shared<HelixPlaceholder>(); }
    Vector3 operator()(double t) const override { return Vector3(t, 0.0, 0.0); }
    std::string toString() const override { return "HelixPlaceholder()"; }
    std::string classString() const override { return "HelixPlaceholder"; }
};

// ============================================================================
// A. the D unittest block, assertion for assertion, in D's order.
// ============================================================================
void port_d_unittest() {
    // D:294-300
    Vector3 a(2.0, 2.0, 0.0);
    Vector3 b(1.0, 2.0, 1.0);
    Vector3 c(1.0, 2.0, 0.0);
    auto abc = std::make_shared<Arc>(a, b, c);
    eil::geom::PathList two = {abc, std::make_shared<Line>(b, c)};
    auto polyline1 = std::make_shared<Polyline>(two);
    Vector3 f = (*polyline1)(0.5);
    CHECK3(eil::geom::approxEqualVectors(f, Vector3(1.28154, 2, 0.95955)),
           "D:300 approxEqualVectors(f, Vector3(1.28154, 2, 0.95955))");
    // D:301-303
    auto polyline2 = std::make_shared<Polyline>(*polyline1, true);
    Vector3 g = (*polyline2)(0.999999);
    CHECK3(eil::geom::approxEqualVectors(g, a), "D:303 approxEqualVectors(g, a)");

#ifdef EIL_COMPLEX_NUMBERS
    // D:305-326 — the complex-step derivative of the midpoint of a two-line
    // polyline. Not compiled in the trunk build (backlog T-904); kept verbatim so
    // the wave-12 port can use it.
    {
        double h = 1.0e-20;
        eil::number ih = eil::Complex<double>(0.0, h);
        eil::number zero = 0.0;
        eil::number one = 1.0;
        double alpha = 1.0;
        Vector3 pa_dash = Vector3(alpha + ih, alpha + ih);
        Vector3 pb = Vector3(one, zero);
        Vector3 pc = Vector3(zero, one);
        auto line0 = std::make_shared<Line>(pc, pa_dash);
        auto line1 = std::make_shared<Line>(pa_dash, pb);
        eil::geom::PathList segs = {line0, line1};
        auto polyline3 = std::make_shared<Polyline>(segs);
        double dpmid_da_x = eil::im((*polyline3)(0.5).x) / h;
        double dpmid_da_y = eil::im((*polyline3)(0.5).y) / h;
        double dpmid_da_z = eil::im((*polyline3)(0.5).z) / h;
        CHECK3(eil::isClose(dpmid_da_x, 1.0), "D:323 dpmid_da_x");
        CHECK3(eil::isClose(dpmid_da_y, 1.0), "D:324 dpmid_da_y");
        CHECK3(eil::isClose(dpmid_da_z, 0.0), "D:325 dpmid_da_z");
    }
#endif
}

// ============================================================================
// B. the probe replay, in probe order.
// ============================================================================
void dump_breaks(const char* tag, const Polyline& pl) {
    // probe_polyline.d:44-57
    emitf("%s nseg=%d closed=%s", tag, static_cast<int>(pl.segments.length()),
          eil_test_gpath3::bools(pl.closed));
    for (size_t i = 0; i < pl.segments.length(); ++i) {
        emitf("%s seg[%d] class=%s len=%.17g", tag, static_cast<int>(i),
              pl.segments[i]->classString().c_str(), eil::re(pl.segments[i]->length()));
    }
    double tsum = 0.0;
    for (size_t i = 0; i < pl.t_values.length(); ++i) {
        emitf("%s t_values[%d]=%.17g", tag, static_cast<int>(i), pl.t_values[i]);
        tsum += pl.t_values[i];
    }
    emitf("%s t_sum=%.17g", tag, tsum);
}

void replay_probe() {
    using namespace eil_test_gpath3;

    // probe_polyline.d:71-77 — Arc + Line (the polyline.d:294-300 case).
    Vector3 a(2.0, 2.0, 0.0);
    Vector3 b(1.0, 2.0, 1.0);
    Vector3 c(1.0, 2.0, 0.0);
    auto arc = std::make_shared<Arc>(a, b, c);
    eil::geom::PathList two = {arc, std::make_shared<Line>(b, c)};
    auto polyline1 = std::make_shared<Polyline>(two);
    dump_breaks("poly.two", *polyline1);
    dump_path("poly.two", polyline1);

    // probe_polyline.d:79-83 — the same, closed (which appends a Line).
    auto polyline2 = std::make_shared<Polyline>(*polyline1, true);
    dump_breaks("poly.closed", *polyline2);
    dump_path("poly.closed", polyline2);
    Vector3 g = (*polyline2)(0.999999);
    emitf("poly.closed g(0.999999) x=%.17g y=%.17g z=%.17g", g.x, g.y, g.z);

    // probe_polyline.d:86-100 — the spline-through-points constructor, gpath_demo.d's
    // rough circle.
    eil::Array<Vector3> pnts = {Vector3(0.0, -1.0, 0.0), Vector3(-1.0, 0.0, 0.0),
                                Vector3(0.0, 1.0, 0.0), Vector3(1.0, 0.0, 0.0),
                                Vector3(0.0, -1.0, 0.0)};
    auto circle = std::make_shared<Polyline>(pnts);
    dump_breaks("poly.circle", *circle);
    dump_path("poly.circle", circle);
    for (size_t i = 0; i < circle->segments.length(); ++i) {
        const Bezier* bz = dynamic_cast<const Bezier*>(circle->segments[i].get());
        for (size_t j = 0; j < bz->B.length(); ++j) {
            emitf("poly.circle seg[%d] B[%d] x=%.17g y=%.17g z=%.17g", static_cast<int>(i),
                  static_cast<int>(j), bz->B[j].x, bz->B[j].y, bz->B[j].z);
        }
    }

    // probe_polyline.d:105-112 — the same constructor, closed and open.
    eil::Array<Vector3> openEnds = {Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.3, 0.0),
                                    Vector3(2.0, -0.2, 0.0), Vector3(3.0, 0.9, 0.0)};
    auto spline1 = std::make_shared<Polyline>(openEnds, true);
    dump_breaks("poly.spline.closed", *spline1);
    dump_path("poly.spline.closed", spline1);
    auto spline0 = std::make_shared<Polyline>(openEnds, false);
    dump_breaks("poly.spline.open", *spline0);
    dump_path("poly.spline.open", spline0);

    // probe_polyline.d:115-124 — Bezier + Line through the Gmsh writer, twice, so
    // the tag chaining across calls is visible.
    auto bez = std::make_shared<Bezier>(
        eil::Array<Vector3>{Vector3(0.0, 0.0, 0.0), Vector3(0.5, 1.0, 0.0), Vector3(1.0, 0.0, 0.0)});
    eil::geom::PathList mix = {bez, std::make_shared<Line>(Vector3(1.0, 0.0, 0.0),
                                                     Vector3(1.0, 1.0, 0.0))};
    auto mixed = std::make_shared<Polyline>(mix);
    int pt = 0, ct = 0, lt = 0;
    dump_string("poly.gmsh.mixed", mixed->toGmshString(pt, ct, lt, "mixed", 1.0e-2));
    emitf("poly.gmsh.mixed tags pointTag=%d curveTag=%d loopTag=%d", pt, ct, lt);
    dump_string("poly.gmsh.mixed2", mixed->toGmshString(pt, ct, lt, "again"));
    emitf("poly.gmsh.mixed2 tags pointTag=%d curveTag=%d loopTag=%d", pt, ct, lt);

    // probe_polyline.d:127-129 — the closed Arc/Line/Line polyline (Arc branch and
    // the closing Line branch of polyline.d:210-215).
    int pt2 = 0, ct2 = 0, lt2 = 0;
    dump_string("poly.gmsh.closed", polyline2->toGmshString(pt2, ct2, lt2, "closed", 0.05));
    emitf("poly.gmsh.closed tags pointTag=%d curveTag=%d loopTag=%d", pt2, ct2, lt2);

    // probe_polyline.d:132-137 — a segment type the writer does not handle.
    int pt3 = 0, ct3 = 0, lt3 = 0;
    eil::geom::PathList withodd = {std::make_shared<HelixPlaceholder>(),
                             std::make_shared<Line>(Vector3(1.0, 0.0, 0.0),
                                                    Vector3(2.0, 1.0, 0.0))};
    auto odd = std::make_shared<Polyline>(withodd);
    dump_string("poly.gmsh.unhandled", odd->toGmshString(pt3, ct3, lt3, "odd"));
    emitf("poly.gmsh.unhandled tags pointTag=%d curveTag=%d loopTag=%d", pt3, ct3, lt3);

    // probe_polyline.d:141-143 — the closed Bezier circle, whose last segment ends
    // on the first point (the tag1 / startPointTag branch of polyline.d:242-253).
    int pt4 = 0, ct4 = 0, lt4 = 0;
    dump_string("poly.gmsh.circle", circle->toGmshString(pt4, ct4, lt4, "circle"));
    emitf("poly.gmsh.circle tags pointTag=%d curveTag=%d loopTag=%d", pt4, ct4, lt4);

    // probe_polyline.d:146-149 — the file constructor (polyline.d:111-130).
    auto fromfile = std::make_shared<Polyline>(data_dir + "/gpath_points3d.dat");
    dump_breaks("poly.file", *fromfile);
    dump_path("poly.file", fromfile);
    auto fromfilec = std::make_shared<Polyline>(data_dir + "/gpath_points3d.dat", true);
    dump_breaks("poly.file.closed", *fromfilec);

    // probe_polyline.d:154-161 — the malformed file, the empty file, and no
    // segments at all.
    // Taxonomy only, not e.msg: eil::to_double's message for a token starting with
    // '#' diverges from Phobos' (substrate/include/eil/conv.hpp:87-92 sends every
    // non-digit/'.'/'+'/'-' first character down the "error converting input to
    // floating point" arm, while Phobos only does that for a leading n/N/i/I and
    // says "no digits seen for input X." otherwise). Reported as T-9xx; the probe
    // row is written the same way, see probe_xspline.d's SUBSTRATE NOTE.
    {
        auto t = classify([&]() {
            auto q = std::make_shared<Polyline>(data_dir + "/gpath_bad.dat");
        });
        emitf("poly.file.bad asError=%s asException=%s kind=%s", bools(t.asError),
              bools(t.asException), t.asError ? "Error" : (t.asException ? "Exception" : "none"));
    }
    dump_error("poly.file.empty", classify([&]() {
        auto q = std::make_shared<Polyline>(data_dir + "/gpath_empty.dat");
    }));
    eil::geom::PathList empty_segs;
    dump_error("poly.err.nosegments", classify([&]() {
        auto q = std::make_shared<Polyline>(empty_segs);
    }));

    // probe_polyline.d:162-164 — dup() (D:137-140) and the copy constructor
    // (D:132-135), which rebuild the segment list through the segment constructor
    // that dups every segment.
    dump_path("poly.dup", fromfile->dup());
    auto cp = std::make_shared<Polyline>(*mixed, false);
    dump_breaks("poly.copy", *cp);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: " << argv[0]
                  << " <probes/geom_gpath_polyline.txt> <tests/test_data dir>\n";
        return 2;
    }
    data_dir = argv[2];
    port_d_unittest();
    replay_probe();
    return eil_test_gpath3::report("polyline", argv[1]);
}
