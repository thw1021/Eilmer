// tests/test_geom_surface_coonspatch.cpp — C++ mirror of
// probes/geom/surface/probe_coonspatch.d, replayed line for line against
// baselines/e587b235/probes/geom_surface_coonspatch.txt (333 D rows), plus the
// `unittest` block of geom/surface/coonspatch.d (D:115-125) ported assertion for
// assertion, in source order.
//
// Row groups and what each one can catch:
//   cnp.corners.*        the four-corner constructor: toString(), the four corners, and
//                        each bounding path as classString()/toString() plus its values
//                        at t = 0.0/0.5/1.0 — the pairing north=Line(p01,p11),
//                        east=Line(p10,p11), south=Line(p00,p10), west=Line(p00,p01)
//                        (D:33-36) is what those twelve rows distinguish.
//   cnp.dup.* / cnp.copy.*  D:88-91 re-runs the corner checks, D:76-86 does not.
//   cnp.bezier.*         the four-Path constructor with curved boundaries, including
//                        aftermut/callerstr: D:45-48 dups each path, so a caller's later
//                        mutation must not move the patch's boundary.
//   cnp.open.p00/p10/p11/p01/all
//                        the four throw guards in check order (D:58-73). The rows name
//                        WHICH type escaped (all five are Error, the sibling of
//                        Exception in this port) and the message text, which is
//                        text(...) over Vector3's %.6g toString().
//   cnp.tol.* / cnp.reltol.*
//                        approxEqualVectors' two tolerances separately: 1.0e-6 passes
//                        (threw=none), 1.0e-3 fails on maxAbsDiff, 5.0e-3 at a
//                        magnitude-1 corner passes only through maxRelDiff, 1.5e-2 fails
//                        both. The stored corner comes from south/north (D:50-53), not
//                        from the alternate evaluation — rows cnp.reltol.5e-3.p10 and
//                        cnp.reltol.5e-3.east t=0.
//   cnp.nan.corners      Vector3() is Vector3.init, i.e. NaN, so the guard fires with
//                        "Vector3(nan, nan, nan)" in the message.
//   cnp.point.*          four coincident corners: point paths, zero-area quads.
//   cnp.skew.*           a non-planar corner set so all three components of area() are
//                        non-zero, dumped at (10,10) and (3,7).
//   cnp.demo.*           the CoonsPatch fragment of geom/surface/surface_demo.d (D:14-23)
//                        as exact stdout text. surface_demo.d is directory-level and also
//                        constructs AOPatch and SubRangedSurface, which are outside this
//                        module, so the demo is carried inside the replay instead of as a
//                        separate demo_ test.
//
// Byte-for-byte comparison, no tolerance band.
//
// Regenerate (block comment so the shell line-splices do not trip -Wcomment):
/*
  ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface \
    probes/geom/surface/probe_coonspatch.d -of=/tmp/probe_cnp -od/tmp/probe_obj_w2_surfA
  /tmp/probe_cnp > baselines/e587b235/probes/geom_surface_coonspatch.txt
*/
//
// Run: ./test_geom_surface_coonspatch <baseline.txt>
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
#include "geom/gpath/bezier.hpp"
#include "geom/gpath/line.hpp"
#include "geom/gpath/path.hpp"
#include "geom/surface/coonspatch.hpp"
#include "geom/surface/parametricsurface.hpp"

namespace {

using eil::geom::CoonsPatch;
using eil::geom::ParametricSurface;
using eil::geom::Path;
using eil::geom::PathPtr;
using eil::geom::Vector3;
using eil::number;

std::vector<std::string> g_lines;
int g_checks = 0;
int g_failures = 0;

void emit(const std::string& s) { g_lines.push_back(s); }
template <typename... Args> void emitf(const std::string& fmt, const Args&... args) {
    emit(eil::format(fmt, args...));
}
void check(bool cond, const std::string& what) {
    ++g_checks;
    if (!cond) {
        ++g_failures;
        std::cout << "ASSERT FAILED: " << what << "\n";
    }
}

void dump_one_bound(const std::string& tag, const std::string& name, const PathPtr& b) {
    emitf("%s %s class=%s str=%s", tag, name, b->classString(), b->toString());
    for (double t : {0.0, 0.5, 1.0}) {
        auto p = (*b)(t);
        emitf("%s %s t=%.17g x=%.17g y=%.17g z=%.17g", tag, name, t, eil::re(p.x), eil::re(p.y),
              eil::re(p.z));
    }
}

void dump_patch(const std::string& tag, const CoonsPatch& cp) {
    emitf("%s str=%s", tag, cp.toString());
    emitf("%s classname=%s", tag, cp.classString());
    for (const std::pair<const char*, Vector3>& c :
         {std::make_pair("p00", cp.p00), std::make_pair("p10", cp.p10),
          std::make_pair("p11", cp.p11), std::make_pair("p01", cp.p01)}) {
        emitf("%s %s x=%.17g y=%.17g z=%.17g", tag, c.first, eil::re(c.second.x),
              eil::re(c.second.y), eil::re(c.second.z));
    }
    dump_one_bound(tag, "south", cp.south);
    dump_one_bound(tag, "north", cp.north);
    dump_one_bound(tag, "west", cp.west);
    dump_one_bound(tag, "east", cp.east);
}

void dump_sweep(const std::string& tag, const ParametricSurface& ps) {
    for (double r : {0.0, 0.2, 0.5, 0.75, 1.0}) {
        for (double s : {0.0, 0.25, 0.5, 0.75, 1.0}) {
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

// ============================================================================
// A. coonspatch.d's unittest (D:115-125), assertion for assertion.
// ============================================================================
void port_d_unittest() {
    auto p00 = Vector3(eil::Array<number>{0.0, 0.1, 3.0}.all()); // D:116 Vector3([0.0,0.1,3.0])
    auto p10 = Vector3(1.0, 0.1, 3.0);
    auto p11 = Vector3(1.0, 1.1, 3.0);
    auto p01 = Vector3(0.0, 1.1, 3.0);
    auto my_patch = std::make_shared<CoonsPatch>(p00, p10, p11, p01);
    auto c = (*my_patch)(0.5, 0.5);
    // D:122 assert(approxEqualVectors(c, Vector3(0.5, 0.6, 3.0)));
    check(eil::geom::approxEqualVectors(c, Vector3(0.5, 0.6, 3.0)),
          "coonspatch.d:122 approxEqualVectors(my_patch(0.5, 0.5), Vector3(0.5, 0.6, 3.0))");
    c = (*my_patch)(0.1, 0.1);
    // D:124 assert(approxEqualVectors(c, Vector3(0.1, 0.2, 3.0)));
    check(eil::geom::approxEqualVectors(c, Vector3(0.1, 0.2, 3.0)),
          "coonspatch.d:124 approxEqualVectors(my_patch(0.1, 0.1), Vector3(0.1, 0.2, 3.0))");
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]);
        return 2;
    }

    port_d_unittest();

    //--- group A: the corner constructor ----------------------------------------
    auto p00 = Vector3(eil::Array<number>{0.0, 0.1, 3.0}.all());
    auto p10 = Vector3(1.0, 0.1, 3.0);
    auto p11 = Vector3(1.0, 1.1, 3.0);
    auto p01 = Vector3(0.0, 1.1, 3.0);
    auto cp = std::make_shared<CoonsPatch>(p00, p10, p11, p01);
    dump_patch("cnp.corners", *cp);
    dump_sweep("cnp.corners", *cp);
    dump_area("cnp.corners", *cp, 10, 10);
    dump_area("cnp.corners", *cp, 3, 7);
    dump_area("cnp.corners", *cp, 1, 1);

    auto dp = cp->dup();
    dump_patch("cnp.dup", *std::dynamic_pointer_cast<CoonsPatch>(dp));
    dump_sweep("cnp.dup", *dp);
    dump_area("cnp.dup", *dp, 10, 10);

    auto cpc = std::make_shared<CoonsPatch>(*cp); // D:76-86
    dump_patch("cnp.copy", *cpc);
    dump_sweep("cnp.copy", *cpc);
    dump_area("cnp.copy", *cpc, 10, 10);

    //--- group B: Bezier-bounded patch -----------------------------------------
    PathPtr sB = std::make_shared<eil::geom::Bezier>(
        pts({Vector3(0.0, 0.0, 0.0), Vector3(0.33, -0.2, 0.1), Vector3(0.67, -0.2, 0.1),
             Vector3(1.0, 0.0, 0.0)}));
    PathPtr nB = std::make_shared<eil::geom::Bezier>(
        pts({Vector3(0.0, 1.0, 0.0), Vector3(0.33, 1.2, 0.1), Vector3(0.67, 1.2, 0.1),
             Vector3(1.0, 1.0, 0.0)}));
    PathPtr wB = std::make_shared<eil::geom::Bezier>(
        pts({Vector3(0.0, 0.0, 0.0), Vector3(-0.15, 0.33, 0.1), Vector3(-0.15, 0.67, 0.1),
             Vector3(0.0, 1.0, 0.0)}));
    PathPtr eB = std::make_shared<eil::geom::Bezier>(
        pts({Vector3(1.0, 0.0, 0.0), Vector3(1.15, 0.33, 0.1), Vector3(1.15, 0.67, 0.1),
             Vector3(1.0, 1.0, 0.0)}));
    auto bp = std::make_shared<CoonsPatch>(sB, nB, wB, eB);
    dump_patch("cnp.bezier", *bp);
    dump_sweep("cnp.bezier", *bp);
    dump_area("cnp.bezier", *bp, 10, 10);
    dump_area("cnp.bezier", *bp, 3, 7);

    std::dynamic_pointer_cast<eil::geom::Bezier>(sB)->B[1] = Vector3(0.33, -0.2, 5.0);
    dump_patch("cnp.bezier.aftermut", *bp);
    emitf("cnp.bezier.callerstr str=%s", sB->toString());
    emitf("cnp.bezier str.as.base=%s", (static_cast<ParametricSurface&>(*bp)).toString());

    //--- group C: the four open-corner throw branches --------------------------
    {
        PathPtr s2 = std::make_shared<eil::geom::Line>(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
        PathPtr n2 = std::make_shared<eil::geom::Line>(Vector3(0.1, 1.0, 0.0), Vector3(1.0, 1.0, 0.0));
        PathPtr w2 = std::make_shared<eil::geom::Line>(Vector3(0.1, 0.0, 0.0), Vector3(0.1, 1.0, 0.0));
        PathPtr e2 = std::make_shared<eil::geom::Line>(Vector3(1.0, 0.0, 0.0), Vector3(1.0, 1.0, 0.0));
        dump_throw("cnp.open.p00", [&]() { CoonsPatch(s2, n2, w2, e2); });
    }
    {
        PathPtr s2 = std::make_shared<eil::geom::Line>(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
        PathPtr n2 = std::make_shared<eil::geom::Line>(Vector3(0.0, 1.0, 0.0), Vector3(1.0, 1.0, 0.0));
        PathPtr w2 = std::make_shared<eil::geom::Line>(Vector3(0.0, 0.0, 0.0), Vector3(0.0, 1.0, 0.0));
        PathPtr e2 = std::make_shared<eil::geom::Line>(Vector3(1.0, 0.1, 0.0), Vector3(1.0, 1.0, 0.0));
        dump_throw("cnp.open.p10", [&]() { CoonsPatch(s2, n2, w2, e2); });
    }
    {
        PathPtr s2 = std::make_shared<eil::geom::Line>(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
        PathPtr n2 = std::make_shared<eil::geom::Line>(Vector3(0.0, 1.0, 0.0), Vector3(1.0, 0.9, 0.0));
        PathPtr w2 = std::make_shared<eil::geom::Line>(Vector3(0.0, 0.0, 0.0), Vector3(0.0, 1.0, 0.0));
        PathPtr e2 = std::make_shared<eil::geom::Line>(Vector3(1.0, 0.0, 0.0), Vector3(1.0, 1.0, 0.0));
        dump_throw("cnp.open.p11", [&]() { CoonsPatch(s2, n2, w2, e2); });
    }
    {
        PathPtr s2 = std::make_shared<eil::geom::Line>(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
        PathPtr n2 = std::make_shared<eil::geom::Line>(Vector3(0.1, 1.0, 0.0), Vector3(1.0, 1.0, 0.0));
        PathPtr w2 = std::make_shared<eil::geom::Line>(Vector3(0.0, 0.0, 0.0), Vector3(0.0, 1.0, 0.0));
        PathPtr e2 = std::make_shared<eil::geom::Line>(Vector3(1.0, 0.0, 0.0), Vector3(1.0, 1.0, 0.0));
        dump_throw("cnp.open.p01", [&]() { CoonsPatch(s2, n2, w2, e2); });
    }
    {
        PathPtr s2 = std::make_shared<eil::geom::Line>(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
        PathPtr n2 = std::make_shared<eil::geom::Line>(Vector3(0.4, 1.0, 0.0), Vector3(1.0, 0.4, 0.0));
        PathPtr w2 = std::make_shared<eil::geom::Line>(Vector3(0.3, 0.1, 0.0), Vector3(0.2, 1.0, 0.0));
        PathPtr e2 = std::make_shared<eil::geom::Line>(Vector3(1.1, 0.2, 0.0), Vector3(1.0, 1.1, 0.0));
        dump_throw("cnp.open.all", [&]() { CoonsPatch(s2, n2, w2, e2); });
    }

    //--- group D: approxEqualVectors' tolerances -------------------------------
    {
        PathPtr n2 = std::make_shared<eil::geom::Line>(Vector3(0.0, 1.0, 0.0), Vector3(1.0, 1.0, 0.0));
        PathPtr w2 = std::make_shared<eil::geom::Line>(Vector3(0.0, 0.0, 0.0), Vector3(0.0, 1.0, 0.0));
        PathPtr e2 = std::make_shared<eil::geom::Line>(Vector3(1.0, 0.0, 0.0), Vector3(1.0, 1.0, 0.0));
        {
            PathPtr s2 =
                std::make_shared<eil::geom::Line>(Vector3(1.0e-3, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
            dump_throw("cnp.tol.1e-3", [&]() { CoonsPatch(s2, n2, w2, e2); });
        }
        {
            PathPtr s2 =
                std::make_shared<eil::geom::Line>(Vector3(1.0e-6, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
            dump_throw("cnp.tol.1e-6", [&]() { CoonsPatch(s2, n2, w2, e2); });
        }
        {
            PathPtr s2 =
                std::make_shared<eil::geom::Line>(Vector3(1.0, 0.0, 0.0), Vector3(1.015, 0.0, 0.0));
            dump_throw("cnp.tol.1.5e-2", [&]() { CoonsPatch(s2, n2, w2, e2); });
        }
        {
            PathPtr s2 =
                std::make_shared<eil::geom::Line>(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
            PathPtr e2b =
                std::make_shared<eil::geom::Line>(Vector3(1.005, 0.0, 0.0), Vector3(1.0, 1.0, 0.0));
            dump_throw("cnp.reltol.5e-3", [&]() { CoonsPatch(s2, n2, w2, e2b); });
            auto okp = std::make_shared<CoonsPatch>(s2, n2, w2, e2b);
            emitf("cnp.reltol.5e-3.p10 x=%.17g y=%.17g z=%.17g", eil::re(okp->p10.x),
                  eil::re(okp->p10.y), eil::re(okp->p10.z));
            auto ea = (*okp->east)(0.0);
            emitf("cnp.reltol.5e-3.east t=%.17g x=%.17g y=%.17g z=%.17g", 0.0, eil::re(ea.x),
                  eil::re(ea.y), eil::re(ea.z));
        }
        {
            PathPtr s2 =
                std::make_shared<eil::geom::Line>(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
            PathPtr e2c =
                std::make_shared<eil::geom::Line>(Vector3(1.015, 0.0, 0.0), Vector3(1.0, 1.0, 0.0));
            dump_throw("cnp.reltol.1.5e-2", [&]() { CoonsPatch(s2, n2, w2, e2c); });
        }
    }

    //--- group E: NaN corners --------------------------------------------------
    dump_throw("cnp.nan.corners",
               [&]() { CoonsPatch(Vector3(), Vector3(), Vector3(), Vector3()); });

    //--- group F: degenerate and skewed patches -------------------------------
    auto pt = std::make_shared<CoonsPatch>(Vector3(1.0, 2.0, 3.0), Vector3(1.0, 2.0, 3.0),
                                           Vector3(1.0, 2.0, 3.0), Vector3(1.0, 2.0, 3.0));
    dump_patch("cnp.point", *pt);
    dump_sweep("cnp.point", *pt);
    dump_area("cnp.point", *pt, 10, 10);

    auto sk = std::make_shared<CoonsPatch>(Vector3(0.5, -0.25, 0.1), Vector3(2.0, 0.75, -0.3),
                                           Vector3(1.25, 2.5, 0.05), Vector3(-0.5, 1.5, 0.9));
    dump_patch("cnp.skew", *sk);
    dump_sweep("cnp.skew", *sk);
    dump_area("cnp.skew", *sk, 10, 10);
    dump_area("cnp.skew", *sk, 3, 7);

    //--- group G: the surface_demo.d CoonsPatch fragment ----------------------
    {
        auto d00 = Vector3(0.0, 0.1, 3.0);
        auto d10 = Vector3(1.0, 0.1, 3.0);
        auto d11 = Vector3(1.0, 1.1, 3.0);
        auto d01 = Vector3(0.0, 1.1, 3.0);
        emitf("cnp.demo head=%s", std::string("CoonsPatch demo"));
        auto my_patch = std::make_shared<CoonsPatch>(d00, d10, d11, d01);
        emitf("cnp.demo patch=%s", "my_patch= " + my_patch->toString());
        auto c = (*my_patch)(0.1, 0.1);
        emitf("cnp.demo c=%s", "my_patch(0.1, 0.1)= " + c.toString());
        d10 = Vector3(1.0, 0.4, 3.0);
        auto my_patch2 = std::make_shared<CoonsPatch>(d00, d10, d11, d01);
        emitf("cnp.demo patch2=%s", "my_patch= " + my_patch2->toString());
    }

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
        "%s: geom_surface_coonspatch, %d/%d unittest assertions hold (coonspatch.d:115-125); "
        "probe replay %zu D lines, %zu C++ lines, %d differ; %zu of %zu rows byte-identical, "
        "0 rows accepted through a tolerance band\n",
        (diffs == 0 && g_failures == 0) ? "PASS" : "FAIL", g_checks - g_failures, g_checks,
        expected.size(), g_lines.size(), diffs, common - static_cast<size_t>(diffs), common);
    return (diffs == 0 && g_failures == 0) ? 0 : 1;
}
