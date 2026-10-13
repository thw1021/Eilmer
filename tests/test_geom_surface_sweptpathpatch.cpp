// tests/test_geom_surface_sweptpathpatch.cpp — C++ mirror of
// probes/geom/surface/probe_sweptpathpatch.d, replayed line for line against
// baselines/e587b235/probes/geom_surface_sweptpathpatch.txt (724 D rows).
//
// sweptpathpatch.d has NO `unittest` block (measured: 0 matches of '^unittest' in its 54
// lines), so the replay is the whole gate for the module.
//
// Row groups and what each one can catch:
//   spp.demo.*           the surface_demo.d patch (Line swept along Line): corners, both
//                        boundaries with their own dpdt/d2pdt2 at the Path::dpdt branch
//                        points, a 5x5 quadrant sweep, the fd sweep, area() at (10,10) and
//                        (3,7), plus the two exact demo stdout lines.
//   spp.demo.dup/.copy   D:37-40 re-derives the corners; D:27-35 does not.
//   spp.demo.aftermut /
//   spp.demo.callerstr   D:19-20 dup the boundaries, so the patch is unchanged when the
//                        caller's Line end point moves to y=7 while the caller's own text
//                        does change.
//   spp.arc.*            Arc boundaries. Arc does not override dpdt/d2pdt2, so those twelve
//                        rows per boundary come from Path's finite differences (dt = 0.001,
//                        forward one-sided below 0.001, backward one-sided above 0.999,
//                        central in between).
//   spp.poly.*           Polyline boundaries — piecewise-linear, also base finite
//                        differences, and the kinks make the three stencils differ.
//   spp.mixed.*          a Bezier swept along an Arc.
//   spp.zerospan /
//   spp.zerosweep        degenerate edges: a zero-length swept path collapses the patch
//                        onto cB, a zero-length sweep path leaves only cA(s) - cA(0).
//   spp.corrupt.copy /
//   spp.corrupt.dup      corners corrupted after construction: the copy keeps
//                        (11,12,13)…(20,21,22), dup() recomputes (1,0.25,0)…(1,7.25,0).
//   spp.nan.*            a boundary built on Vector3() corners (NaN).
//   spp.demo.outrange.*  r = -0.5 and 1.5, outside the unit interval.
//   spp.badarc.radii / spp.badarc.collinear / spp.badbezier / spp.badpolyline /
//   spp.opcall.boundarythrows
//                        the boundary-Path failures reached through the constructor's four
//                        evaluations and through opCall. Each row names WHICH type escaped
//                        (Error for all five; the sibling of Exception in this port) and
//                        the message text.
//
// Not replayed, and why: `new SweptPathPatch(null, cB)` SEGFAULTS in D — measured as exit
// status 139 with a core dump and no catchable Error or Exception — so it has no stdout
// row. The port adds no null check either, because D has none (see the module header).
//
// Byte-for-byte comparison, no tolerance band.
//
// Regenerate (block comment so the shell line-splices do not trip -Wcomment):
/*
  ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface \
    probes/geom/surface/probe_sweptpathpatch.d -of=/tmp/probe_spp -od/tmp/probe_obj_w2_surfA
  /tmp/probe_spp > baselines/e587b235/probes/geom_surface_sweptpathpatch.txt
*/
//
// Run: ./test_geom_surface_sweptpathpatch <baseline.txt>
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "eil/array.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/arc.hpp"
#include "geom/gpath/bezier.hpp"
#include "geom/gpath/line.hpp"
#include "geom/gpath/path.hpp"
#include "geom/gpath/polyline.hpp"
#include "geom/surface/parametricsurface.hpp"
#include "geom/surface/sweptpathpatch.hpp"

namespace {

using eil::geom::ParametricSurface;
using eil::geom::Path;
using eil::geom::PathPtr;
using eil::geom::SweptPathPatch;
using eil::geom::Vector3;

std::vector<std::string> g_lines;

void emit(const std::string& s) { g_lines.push_back(s); }
template <typename... Args> void emitf(const std::string& fmt, const Args&... args) {
    emit(eil::format(fmt, args...));
}

void dump_corner(const std::string& tag, const std::string& name, const Vector3& v) {
    emitf("%s %s x=%.17g y=%.17g z=%.17g", tag, name, eil::re(v.x), eil::re(v.y), eil::re(v.z));
}

void dump_one_bound(const std::string& tag, const std::string& name, const PathPtr& b) {
    emitf("%s %s class=%s str=%s", tag, name, b->classString(), b->toString());
    for (double t : {0.0, 0.5, 1.0}) {
        auto p = (*b)(t);
        emitf("%s %s t=%.17g x=%.17g y=%.17g z=%.17g", tag, name, t, eil::re(p.x), eil::re(p.y),
              eil::re(p.z));
    }
    for (double t : {0.0, 0.0005, 0.001, 0.999, 0.9995, 1.0}) {
        auto d1 = b->dpdt(t);
        auto d2 = b->d2pdt2(t);
        emitf("%s %s dpdt t=%.17g x=%.17g y=%.17g z=%.17g", tag, name, t, eil::re(d1.x),
              eil::re(d1.y), eil::re(d1.z));
        emitf("%s %s d2pdt2 t=%.17g x=%.17g y=%.17g z=%.17g", tag, name, t, eil::re(d2.x),
              eil::re(d2.y), eil::re(d2.z));
    }
}

void dump_patch(const std::string& tag, const SweptPathPatch& sp) {
    emitf("%s str=%s", tag, sp.toString());
    emitf("%s classname=%s", tag, sp.classString());
    dump_corner(tag, "p00", sp.p00);
    dump_corner(tag, "p10", sp.p10);
    dump_corner(tag, "p11", sp.p11);
    dump_corner(tag, "p01", sp.p01);
    dump_one_bound(tag, "cA", sp.cA);
    dump_one_bound(tag, "cB", sp.cB);
}

void sweep(const std::string& tag, const ParametricSurface& ps, const std::vector<double>& rs,
           const std::vector<double>& ss) {
    for (double r : rs) {
        for (double s : ss) {
            auto p = ps(r, s);
            emitf("%s opCall r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g", tag, r, s, eil::re(p.x),
                  eil::re(p.y), eil::re(p.z));
        }
    }
}

void dump_area(const std::string& tag, ParametricSurface& ps, int nr, int ns) {
    auto a = ps.area(nr, ns);
    emitf("%s area nr=%d ns=%d x=%.17g y=%.17g z=%.17g", tag, nr, ns, eil::re(a.x), eil::re(a.y),
          eil::re(a.z));
}

void dump_throw(const std::string& tag, const std::function<void()>& fn) {
    try {
        fn();
        emitf("%s threw=none", tag);
    } catch (const eil::ErrorBase& e) {
        emitf("%s threw=Error msg=%s", tag, e.msg());
    } catch (const eil::ExceptionBase& e) {
        emitf("%s threw=Exception msg=%s", tag, e.msg());
    }
}

eil::Array<Vector3> pts(std::initializer_list<Vector3> v) {
    eil::Array<Vector3> a;
    for (const Vector3& p : v) a.append(p);
    return a;
}

PathPtr mk_line(double x0, double y0, double z0, double x1, double y1, double z1) {
    return std::make_shared<eil::geom::Line>(Vector3(x0, y0, z0), Vector3(x1, y1, z1));
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]);
        return 2;
    }

    // sweptpathpatch.d has no unittest block: 0 of 0 assertions, measured.
    const std::vector<double> fd{0.0, 0.0005, 0.001, 0.999, 0.9995, 1.0};
    const std::vector<double> quad{0.0, 0.25, 0.5, 0.75, 1.0};
    const std::vector<double> s3{0.0, 0.5, 1.0};

    //--- group A: the surface_demo.d patch --------------------------------------
    PathPtr aA = mk_line(0.0, 0.0, 0.0, 0.0, 1.0, 0.0);
    PathPtr aB = mk_line(1.0, 0.25, 0.0, 2.0, 0.25, 0.0);
    auto demo = std::make_shared<SweptPathPatch>(aA, aB);
    dump_patch("spp.demo", *demo);
    sweep("spp.demo", *demo, quad, quad);
    sweep("spp.demo", *demo, fd, s3);
    dump_area("spp.demo", *demo, 10, 10);
    dump_area("spp.demo", *demo, 3, 7);
    emitf("spp.demo strline=%s", "spp= " + demo->toString());
    auto dc = (*demo)(0.5, 0.5);
    emitf("spp.demo cline=%s", "spp(0.5, 0.5)= " + dc.toString());

    auto dd = std::dynamic_pointer_cast<SweptPathPatch>(demo->dup());
    dump_patch("spp.demo.dup", *dd);
    sweep("spp.demo.dup", *dd, quad, s3);
    dump_area("spp.demo.dup", *dd, 10, 10);
    auto cc = std::make_shared<SweptPathPatch>(*demo);
    dump_patch("spp.demo.copy", *cc);
    sweep("spp.demo.copy", *cc, quad, s3);
    dump_area("spp.demo.copy", *cc, 3, 7);

    std::dynamic_pointer_cast<eil::geom::Line>(aA)->p1 = Vector3(0.0, 7.0, 0.0);
    dump_patch("spp.demo.aftermut", *demo);
    emitf("spp.demo.callerstr str=%s", aA->toString());

    //--- group B: Arc boundaries ----------------------------------------------
    PathPtr bA = std::make_shared<eil::geom::Arc>(Vector3(0.0, 0.0, 0.1), Vector3(0.0, 1.0, 0.1),
                                                 Vector3(0.5, 0.5, 0.1));
    PathPtr bB = std::make_shared<eil::geom::Arc>(Vector3(1.0, 0.0, 0.0), Vector3(2.0, 0.0, 0.0),
                                                 Vector3(1.5, 0.5, 0.0));
    auto arc = std::make_shared<SweptPathPatch>(bA, bB);
    dump_patch("spp.arc", *arc);
    sweep("spp.arc", *arc, quad, quad);
    sweep("spp.arc", *arc, fd, s3);
    dump_area("spp.arc", *arc, 10, 10);
    dump_area("spp.arc", *arc, 3, 7);
    emitf("spp.arc str.as.base=%s", (static_cast<ParametricSurface&>(*arc)).toString());

    //--- group C: Polyline boundaries -----------------------------------------
    eil::Array<Vector3> pa = pts({Vector3(0.0, 0.0, 0.0), Vector3(0.2, 0.5, 0.1),
                                  Vector3(-0.1, 1.0, 0.3)});
    eil::Array<Vector3> pb = pts({Vector3(1.0, 0.0, 0.0), Vector3(1.7, 0.1, -0.2),
                                  Vector3(2.0, 0.3, 0.1), Vector3(2.4, 0.0, 0.4)});
    auto poly = std::make_shared<SweptPathPatch>(std::make_shared<eil::geom::Polyline>(pa),
                                                 std::make_shared<eil::geom::Polyline>(pb));
    dump_patch("spp.poly", *poly);
    sweep("spp.poly", *poly, quad, quad);
    sweep("spp.poly", *poly, fd, s3);
    dump_area("spp.poly", *poly, 10, 10);
    dump_area("spp.poly", *poly, 3, 7);

    //--- group D: a Bezier swept along an Arc ---------------------------------
    PathPtr cA = std::make_shared<eil::geom::Bezier>(
        pts({Vector3(0.0, 0.0, 0.0), Vector3(0.4, 0.1, 0.2), Vector3(0.1, 0.9, 0.1),
             Vector3(0.0, 1.0, 0.3)}));
    auto mixed = std::make_shared<SweptPathPatch>(cA, bB);
    dump_patch("spp.mixed", *mixed);
    sweep("spp.mixed", *mixed, quad, quad);
    sweep("spp.mixed", *mixed, fd, s3);
    dump_area("spp.mixed", *mixed, 10, 10);

    //--- group E: degenerate edges --------------------------------------------
    PathPtr zA = mk_line(0.5, 0.5, 0.5, 0.5, 0.5, 0.5);
    auto z = std::make_shared<SweptPathPatch>(zA, aB);
    dump_patch("spp.zerospan", *z);
    sweep("spp.zerospan", *z, quad, s3);
    dump_area("spp.zerospan", *z, 10, 10);

    PathPtr zB = mk_line(1.0, 1.0, 1.0, 1.0, 1.0, 1.0);
    auto zs = std::make_shared<SweptPathPatch>(aA, zB);
    dump_patch("spp.zerosweep", *zs);
    sweep("spp.zerosweep", *zs, quad, s3);
    dump_area("spp.zerosweep", *zs, 10, 10);

    auto corrupt = std::make_shared<SweptPathPatch>(aA, aB);
    corrupt->p00 = Vector3(11.0, 12.0, 13.0);
    corrupt->p10 = Vector3(14.0, 15.0, 16.0);
    corrupt->p11 = Vector3(17.0, 18.0, 19.0);
    corrupt->p01 = Vector3(20.0, 21.0, 22.0);
    auto copied = std::make_shared<SweptPathPatch>(*corrupt);
    dump_patch("spp.corrupt.copy", *copied);
    auto reduped = std::dynamic_pointer_cast<SweptPathPatch>(corrupt->dup());
    dump_patch("spp.corrupt.dup", *reduped);

    //--- group F: NaN corners, r/s outside the unit interval -------------------
    auto nanc = std::make_shared<SweptPathPatch>(
        std::make_shared<eil::geom::Line>(Vector3(), Vector3(1.0, 0.0, 0.0)), aB);
    dump_patch("spp.nan", *nanc);
    sweep("spp.nan", *nanc, {0.0, 0.5, 1.0}, s3);
    dump_area("spp.nan", *nanc, 10, 10);
    sweep("spp.demo.outrange", *demo, {-0.5, 1.5}, {0.0, 0.5, 1.0});

    //--- group G: throwing boundary paths -------------------------------------
    dump_throw("spp.badarc.radii", [&]() {
        PathPtr bad = std::make_shared<eil::geom::Arc>(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0),
                                                      Vector3(0.4, 0.2, 0.0));
        SweptPathPatch(bad, aB);
    });
    dump_throw("spp.badarc.collinear", [&]() {
        PathPtr bad = std::make_shared<eil::geom::Arc>(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0),
                                                      Vector3(0.5, 0.0, 0.0));
        SweptPathPatch(aB, bad);
    });
    dump_throw("spp.badbezier", [&]() {
        PathPtr bad = std::make_shared<eil::geom::Bezier>(eil::Array<Vector3>{Vector3(1.0, 0.0, 0.0)});
        SweptPathPatch(bad, aB);
    });
    dump_throw("spp.badpolyline", [&]() {
        eil::Array<Vector3> none;
        SweptPathPatch(std::make_shared<eil::geom::Polyline>(none), aB);
    });
    dump_throw("spp.opcall.boundarythrows", [&]() {
        PathPtr bad = std::make_shared<eil::geom::Arc>(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0),
                                                      Vector3(0.4, 0.2, 0.0));
        auto gp = std::make_shared<SweptPathPatch>(bB, aB);
        gp->cA = bad;
        (*gp)(0.5, 0.5);
    });

    //--- replay ---------------------------------------------------------------
    std::ifstream in(argv[1]);
    if (!in) {
        std::fprintf(stderr, "cannot open baseline %s\n", argv[1]);
        return 2;
    }
    std::vector<std::string> expected;
    for (std::string l; std::getline(in, l);) expected.push_back(l);

    int diffs = 0, shown = 0;
    for (size_t i = 0; i < expected.size() || i < g_lines.size(); ++i) {
        const std::string e = i < expected.size() ? expected[i] : "<none>";
        const std::string g = i < g_lines.size() ? g_lines[i] : "<none>";
        if (e == g) continue;
        if (shown < 12) {
            std::printf("line %zu\n  D:   |%.400s|\n  C++: |%.400s|\n", i + 1, e.c_str(),
                        g.c_str());
            ++shown;
        }
        ++diffs;
    }
    const size_t common = std::min(expected.size(), g_lines.size());
    std::printf(
        "%s: geom_surface_sweptpathpatch, 0/0 unittest assertions (sweptpathpatch.d has no "
        "unittest block — 0 matches of '^unittest' in its 54 lines); probe replay %zu D "
        "lines, %zu C++ lines, %d differ; %zu of %zu rows byte-identical, 0 rows accepted "
        "through a tolerance band\n",
        (diffs == 0) ? "PASS" : "FAIL", expected.size(), g_lines.size(), diffs,
        common - static_cast<size_t>(diffs), common);
    return diffs == 0 ? 0 : 1;
}
