// tests/test_geom_surface_channelpatch.cpp — C++ mirror of
// probes/geom/surface/probe_channelpatch.d, replayed line for line against
// baselines/e587b235/probes/geom_surface_channelpatch.txt (820 D rows).
//
// channelpatch.d has NO `unittest` block (measured: 0 matches of '^unittest' in its 105
// lines), so this replay is the whole gate for the module, exactly as the
// geom/surface/parametricsurface exemplar does for a block-less module.
//
// Row groups and what each one can catch:
//   chp.demo.*           the two-argument construction of surface_demo.d:40, i.e. both
//                        flags false → the Bezier bridge, not a Line. Includes the two
//                        exact demo stdout lines (chanp= …, chanp(0.5, 0.5)= …).
//   chp.f00 / f01 / f10 / f11
//                        all four (ruled, pure2D) combinations over z-bearing Line
//                        boundaries, so pure2D's three `z = 0.0` sites (D:54, :82, :101)
//                        are separately visible, each with its own bridge at r=0.25 and
//                        the r=1.0 boundary.
//   chp.arc.*            Arc boundaries. Arc does NOT override dpdt, so the tangents come
//                        from Path.dpdt's three finite-difference branches; the r sweep is
//                        0.0, 0.0005, 0.001, 0.5, 0.999, 0.9995, 1.0 (dt = 0.001 and
//                        0.999 + 0.001 == 1.0 exactly, so the switch is `>` not `>=`).
//                        chp.arc bridge … B[1]/B[2] are the two normals' output: D:96-97
//                        build nA as cross(oopvA, tangentA) and nB as cross(tangentB,
//                        oopvB), and a port that symmetrises them changes these eight
//                        numbers.
//   chp.arc.ruled.*      the same boundaries with ruled set, where the finite differences
//                        must not appear at all.
//   chp.arc.pure2d.*     z flattening over Arc boundaries.
//   chp.outr.*           r = -0.5 and 1.5, i.e. the bridging path asked outside the unit
//                        interval, where dpdt takes the one-sided branches.
//   chp.parallel / chp.coincident / chp.antiparallel
//                        cross(tangent, pB-pA) is the zero vector in all three, so
//                        nA/nB come from normalize() of zero. Measured, they come out as
//                        0 (geom/elements/vector3.d:362-370 zeroes the vector instead of
//                        dividing by zero), and the cubic degenerates to [pA, pA, pB, pB].
//   chp.copy / chp.dup / chp.aftermut / chp.callerstr
//                        D:33-43 vs D:45-48, and the proof that D:23-24 dups the caller's
//                        boundaries.
//   chp.default / chp.ruledonly / chp.pure2donly
//                        the two D default arguments and the bool spelling inside
//                        toString().
//   chp.badarc.radii / chp.badarc.collinear / chp.badboundary / chp.bridging.northonly
//                        channelpatch.d throws nowhere itself (0 matches for
//                        throw|enforce|assert), but its constructor evaluates both
//                        boundaries four times (D:27-30), so a bad boundary takes the
//                        construction down. Each row names WHICH type escaped — Error, the
//                        sibling of Exception in this port — and the message text.
//
// Byte-for-byte comparison, no tolerance band.
//
// Regenerate (block comment so the shell line-splices do not trip -Wcomment):
/*
  ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface \
    probes/geom/surface/probe_channelpatch.d -of=/tmp/probe_chp -od/tmp/probe_obj_w2_surfA
  /tmp/probe_chp > baselines/e587b235/probes/geom_surface_channelpatch.txt
*/
//
// Run: ./test_geom_surface_channelpatch <baseline.txt>
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
#include "geom/surface/channelpatch.hpp"
#include "geom/surface/parametricsurface.hpp"

namespace {

using eil::geom::ChannelPatch;
using eil::geom::ParametricSurface;
using eil::geom::Path;
using eil::geom::PathPtr;
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
}

void dump_patch(const std::string& tag, const ChannelPatch& cp) {
    emitf("%s str=%s", tag, cp.toString());
    emitf("%s classname=%s", tag, cp.classString());
    emitf("%s ruled=%s pure2D=%s", tag, cp.ruled ? "true" : "false", cp.pure2D ? "true" : "false");
    dump_corner(tag, "p00", cp.p00);
    dump_corner(tag, "p10", cp.p10);
    dump_corner(tag, "p11", cp.p11);
    dump_corner(tag, "p01", cp.p01);
    dump_one_bound(tag, "cSouth", cp.cSouth);
    dump_one_bound(tag, "cNorth", cp.cNorth);
}

// Mirror of dump_bridge(): class, text, and whichever control data the bridge type has.
void dump_bridge(const std::string& tag, const ChannelPatch& cp, double r) {
    PathPtr br = cp.make_bridging_path(r);
    emitf("%s bridge r=%.17g class=%s str=%s", tag, r, br->classString(), br->toString());
    if (auto l = std::dynamic_pointer_cast<eil::geom::Line>(br)) {
        emitf("%s bridge r=%.17g p0 x=%.17g y=%.17g z=%.17g", tag, r, eil::re(l->p0.x),
              eil::re(l->p0.y), eil::re(l->p0.z));
        emitf("%s bridge r=%.17g p1 x=%.17g y=%.17g z=%.17g", tag, r, eil::re(l->p1.x),
              eil::re(l->p1.y), eil::re(l->p1.z));
    } else if (auto b = std::dynamic_pointer_cast<eil::geom::Bezier>(br)) {
        for (size_t i = 0; i < b->B.length(); ++i) {
            emitf("%s bridge r=%.17g B[%d] x=%.17g y=%.17g z=%.17g", tag, r, static_cast<int>(i),
                  eil::re(b->B[i].x), eil::re(b->B[i].y), eil::re(b->B[i].z));
        }
    }
    for (double s : {0.0, 0.5, 1.0}) {
        auto p = (*br)(s);
        emitf("%s bridge r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g", tag, r, s, eil::re(p.x),
              eil::re(p.y), eil::re(p.z));
    }
    // dpdt of the two boundaries at this r: the operands the normals are built from.
    Vector3 tA = cp.cSouth->dpdt(r);
    Vector3 tB = cp.cNorth->dpdt(r);
    emitf("%s tangentA r=%.17g x=%.17g y=%.17g z=%.17g", tag, r, eil::re(tA.x), eil::re(tA.y),
          eil::re(tA.z));
    emitf("%s tangentB r=%.17g x=%.17g y=%.17g z=%.17g", tag, r, eil::re(tB.x), eil::re(tB.y),
          eil::re(tB.z));
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

PathPtr mk_line(double x0, double y0, double z0, double x1, double y1, double z1) {
    return std::make_shared<eil::geom::Line>(Vector3(x0, y0, z0), Vector3(x1, y1, z1));
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]);
        return 2;
    }

    const std::vector<double> qs{0.0, 0.2, 0.5, 0.75, 1.0};
    const std::vector<double> fd_rs{0.0, 0.0005, 0.001, 0.5, 0.999, 0.9995, 1.0};
    const std::vector<double> ss3{0.0, 0.5, 1.0};

    auto sweep = [&](const std::string& tag, const ParametricSurface& cp,
                     const std::vector<double>& rs, const std::vector<double>& ss) {
        for (double r : rs) {
            for (double s : ss) {
                auto p = cp(r, s);
                emitf("%s opCall r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g", tag, r, s, eil::re(p.x),
                      eil::re(p.y), eil::re(p.z));
            }
        }
    };

    //--- group A: the surface_demo.d channel ------------------------------------
    PathPtr aA = mk_line(0.0, 0.0, 0.0, 1.0, 0.0, 0.0);
    PathPtr aB = mk_line(0.0, 0.25, 0.0, 1.0, 1.0, 0.0);
    auto demo = std::make_shared<ChannelPatch>(aA, aB);
    dump_patch("chp.demo", *demo);
    sweep("chp.demo", *demo, qs, qs);
    dump_bridge("chp.demo", *demo, 0.5);
    dump_area("chp.demo", *demo, 10, 10);
    dump_area("chp.demo", *demo, 3, 7);
    emitf("chp.demo strline=%s", "chanp= " + demo->toString());
    auto dc = (*demo)(0.5, 0.5);
    emitf("chp.demo cline=%s", "chanp(0.5, 0.5)= " + dc.toString());

    //--- group B: the four (ruled, pure2D) combinations ------------------------
    PathPtr sB = mk_line(0.0, 0.0, 0.7, 1.0, 0.0, -0.3);
    PathPtr nB = mk_line(0.0, 1.0, 0.2, 1.0, 1.0, 0.9);
    for (bool ruled : {false, true}) {
        for (bool pure2D : {false, true}) {
            std::string t = eil::format("chp.f%d%d", ruled ? 1 : 0, pure2D ? 1 : 0);
            auto cp = std::make_shared<ChannelPatch>(sB, nB, ruled, pure2D);
            dump_patch(t, *cp);
            sweep(t, *cp, qs, qs);
            dump_bridge(t, *cp, 0.25);
            dump_bridge(t, *cp, 1.0);
            dump_area(t, *cp, 10, 10);
            dump_area(t, *cp, 3, 7);
        }
    }

    //--- group C: Arc boundaries (Path.dpdt finite differences) ----------------
    PathPtr sA = std::make_shared<eil::geom::Arc>(Vector3(0.0, 0.0, 0.1), Vector3(1.0, 0.0, 0.1),
                                                  Vector3(0.5, 0.5, 0.1));
    PathPtr nA = std::make_shared<eil::geom::Arc>(Vector3(0.0, 1.0, 0.4), Vector3(1.0, 1.0, 0.4),
                                                  Vector3(0.5, 0.5, 0.4));
    auto arcp = std::make_shared<ChannelPatch>(sA, nA);
    dump_patch("chp.arc", *arcp);
    for (double r : fd_rs) dump_bridge("chp.arc", *arcp, r);
    sweep("chp.arc", *arcp, fd_rs, ss3);
    sweep("chp.arc", *arcp, qs, qs);
    dump_area("chp.arc", *arcp, 10, 10);
    dump_area("chp.arc", *arcp, 3, 7);

    auto arcr = std::make_shared<ChannelPatch>(sA, nA, true);
    dump_patch("chp.arc.ruled", *arcr);
    for (double r : fd_rs) dump_bridge("chp.arc.ruled", *arcr, r);
    sweep("chp.arc.ruled", *arcr, fd_rs, ss3);
    dump_area("chp.arc.ruled", *arcr, 10, 10);

    auto arcp2 = std::make_shared<ChannelPatch>(sA, nA, false, true);
    dump_patch("chp.arc.pure2d", *arcp2);
    for (double r : fd_rs) dump_bridge("chp.arc.pure2d", *arcp2, r);
    sweep("chp.arc.pure2d", *arcp2, fd_rs, ss3);
    dump_area("chp.arc.pure2d", *arcp2, 10, 10);

    //--- group D: the zero-cross-product / zero-L branches ---------------------
    PathPtr sP = mk_line(0.0, 0.0, 0.0, 1.0, 0.0, 0.0);
    PathPtr nP = mk_line(2.0, 0.0, 0.0, 3.0, 0.0, 0.0);
    auto par = std::make_shared<ChannelPatch>(sP, nP);
    dump_patch("chp.parallel", *par);
    dump_bridge("chp.parallel", *par, 0.5);
    sweep("chp.parallel", *par, {0.5}, ss3);
    dump_area("chp.parallel", *par, 10, 10);

    auto parr = std::make_shared<ChannelPatch>(sP, nP, true);
    dump_bridge("chp.parallel.ruled", *parr, 0.5);
    sweep("chp.parallel.ruled", *parr, {0.5}, ss3);
    dump_area("chp.parallel.ruled", *parr, 10, 10);

    PathPtr nC = mk_line(0.0, 0.0, 0.0, 1.0, 0.0, 0.0);
    auto cw = std::make_shared<ChannelPatch>(sP, nC);
    dump_patch("chp.coincident", *cw);
    dump_bridge("chp.coincident", *cw, 0.5);
    sweep("chp.coincident", *cw, {0.5}, ss3);
    dump_area("chp.coincident", *cw, 10, 10);

    PathPtr nR = mk_line(2.0, 0.0, 0.0, 1.0, 0.0, 0.0);
    auto ap = std::make_shared<ChannelPatch>(sP, nR);
    dump_bridge("chp.antiparallel", *ap, 0.5);
    sweep("chp.antiparallel", *ap, {0.5}, ss3);

    //--- group E: r outside the unit interval ----------------------------------
    for (double r : {-0.5, 1.5}) dump_bridge("chp.outr", *arcp, r);
    sweep("chp.outr", *arcp, {-0.5, 1.5}, ss3);

    //--- group F: copy constructor, dup(), aliasing ----------------------------
    auto cpd = std::make_shared<ChannelPatch>(sB, nB, false, true);
    auto cpy = std::make_shared<ChannelPatch>(*cpd);
    dump_patch("chp.copy", *cpy);
    sweep("chp.copy", *cpy, qs, qs);
    dump_area("chp.copy", *cpy, 10, 10);

    auto dp = std::dynamic_pointer_cast<ChannelPatch>(cpd->dup());
    emitf("chp.dup str=%s", dp->toString());
    emitf("chp.dup classname=%s", dp->classString());
    sweep("chp.dup", *dp, {0.0, 0.5, 1.0}, qs);
    dump_area("chp.dup", *dp, 10, 10);
    dump_area("chp.dup", *dp, 3, 7);

    std::dynamic_pointer_cast<eil::geom::Line>(sB)->p1 = Vector3(1.0, 0.0, 9.0);
    dump_patch("chp.aftermut", *cpd);
    emitf("chp.callerstr str=%s", sB->toString());

    //--- group G: the flag defaults and the bool spellings ---------------------
    auto dflt = std::make_shared<ChannelPatch>(sP, nP);
    emitf("chp.default flags ruled=%s pure2D=%s", dflt->ruled ? "true" : "false",
          dflt->pure2D ? "true" : "false");
    auto ronly = std::make_shared<ChannelPatch>(sP, nP, true);
    emitf("chp.ruledonly flags ruled=%s pure2D=%s", ronly->ruled ? "true" : "false",
          ronly->pure2D ? "true" : "false");
    auto ponly = std::make_shared<ChannelPatch>(sP, nP, false, true);
    emitf("chp.pure2donly flags ruled=%s pure2D=%s", ponly->ruled ? "true" : "false",
          ponly->pure2D ? "true" : "false");
    emitf("chp.pure2donly str=%s", ponly->toString());

    //--- group H: the boundary paths that take the constructor down ------------
    dump_throw("chp.badarc.radii", [&]() {
        PathPtr bad = std::make_shared<eil::geom::Arc>(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0),
                                                       Vector3(0.4, 0.2, 0.0));
        ChannelPatch(bad, nP, false, false);
    });
    dump_throw("chp.badarc.collinear", [&]() {
        PathPtr bad = std::make_shared<eil::geom::Arc>(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0),
                                                       Vector3(0.5, 0.0, 0.0));
        ChannelPatch(bad, nP, true, false);
    });
    dump_throw("chp.badboundary", [&]() {
        PathPtr bad = std::make_shared<eil::geom::Bezier>(
            eil::Array<Vector3>{Vector3(1.0, 0.0, 0.0)});
        ChannelPatch(bad, nP, false, false);
    });
    dump_throw("chp.bridging.northonly", [&]() {
        PathPtr good = mk_line(0.0, 0.0, 0.0, 1.0, 0.0, 0.0);
        PathPtr badN = std::make_shared<eil::geom::Arc>(Vector3(0.0, 1.0, 0.0), Vector3(1.0, 1.0, 0.0),
                                                        Vector3(0.5, 0.5, 0.0));
        auto cpn = std::make_shared<ChannelPatch>(good, badN, false, false);
        cpn->make_bridging_path(0.5);
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
        "%s: geom_surface_channelpatch, 0/0 unittest assertions (channelpatch.d has no "
        "unittest block — 0 matches of '^unittest' in its 105 lines); probe replay %zu D "
        "lines, %zu C++ lines, %d differ; %zu of %zu rows byte-identical, 0 rows accepted "
        "through a tolerance band\n",
        (diffs == 0) ? "PASS" : "FAIL", expected.size(), g_lines.size(), diffs,
        common - static_cast<size_t>(diffs), common);
    return diffs == 0 ? 0 : 1;
}
