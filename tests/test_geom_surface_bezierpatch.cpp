// tests/test_geom_surface_bezierpatch.cpp — C++ mirror of
// probes/geom/surface/probe_bezierpatch.d, replayed line for line against
// baselines/e587b235/probes/geom_surface_bezierpatch.txt (310 D rows), plus the
// `unittest` block of geom/surface/bezierpatch.d (D:107-127) ported assertion for
// assertion, in source order.
//
// The row groups and why each is load-bearing:
//   bzp.unit.*    the D unittest control net (n=3, m=4) — members, a 5x5 (r,s) sweep
//                 that hits r=0.0/1.0 and s=0.0/1.0 exactly and has one point in each
//                 parametric quadrant, and area() at the D defaults (10,10) and an
//                 asymmetric (3,7).
//   bzp.copy.*    D:37-46's copy constructor, whose `m = other.n` gives m=3 where the
//                 original has m=4 (rows 57 vs 3). opCall cannot see the difference,
//                 so only the member dump separates a faithful port from a "fixed"
//                 one.
//   bzp.dup.*     D:48-51, dup() through the three-argument constructor, where m=4
//                 survives.
//   bzp.aliaschk  D:32 dups each net row, so mutating the caller's Q afterwards must
//                 not move the patch's own copy.
//   bzp.twist.*   a non-planar net (n=2, m=3) so all three components of area() are
//                 non-zero, dumped at (10,10), (3,7) and (1,1); also toString()
//                 through a ParametricSurface base reference.
//   bzp.jagged.*  inner arrays of unequal length (4/2/3): legal to build and evaluate.
//   bzp.n0/nm1/nm2/inner1/inner0/short/mismatch/overlong
//                 the constructor and evaluation guards. Each row names WHICH type
//                 escaped (Error vs Exception — siblings in this port, so
//                 catch (ExceptionBase) would not catch an ErrorBase) and the message
//                 text: Bezier's two constructor Errors (geom/gpath/bezier.d:19-24),
//                 the RangeError from `_Q[i]` (D:32) and `Q[0]` (D:85), and D's
//                 `Error("Memory allocation failed")` for the negative length
//                 assignment at D:30.
//   bzp.vtk.*     writeCtrlPtsAsVtkXml: whole file bodies with newlines marked
//                 <newline>, the jagged-net out-of-bounds read, and the Phobos text of
//                 an unusable output path.
//
// The comparison is byte-for-byte with no tolerance band of any kind: every numeric
// row is produced with the same %.17g / %20.16e conversions the D probe uses, so a
// single differing last digit is a finding, not something to paper over.
//
// Regenerate (block comment so the shell line-splices do not trip -Wcomment):
/*
  ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface \
    probes/geom/surface/probe_bezierpatch.d -of=/tmp/probe_bzp -od/tmp/probe_obj_w2_surfA
  /tmp/probe_bzp > baselines/e587b235/probes/geom_surface_bezierpatch.txt
*/
//
// Run: ./test_geom_surface_bezierpatch <baseline.txt>
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/bezier.hpp"
#include "geom/surface/bezierpatch.hpp"
#include "geom/surface/parametricsurface.hpp"

namespace {

using eil::geom::BezierPatch;
using eil::geom::ParametricSurface;
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

// The control net the D unittest builds (bezierpatch.d:108-121).
eil::Array<eil::Array<Vector3>> make_unit_net() {
    double L = 2.0;
    double H = 1.0;
    int n = 3;
    int m = 4;
    double dx = L / n;
    double dy = H / m;
    eil::Array<eil::Array<Vector3>> Q;
    Q.set_length(static_cast<size_t>(n) + 1);
    for (int i = 0; i < n + 1; ++i) {
        Q[static_cast<size_t>(i)].set_length(static_cast<size_t>(m) + 1);
        for (int j = 0; j < m + 1; ++j) {
            Q[static_cast<size_t>(i)][static_cast<size_t>(j)] =
                Vector3(i * dx, j * dy, 0.0);
        }
    }
    return Q;
}

// A genuinely non-planar net (n=2, m=3), as in the probe.
eil::Array<eil::Array<Vector3>> make_twist_net() {
    eil::Array<eil::Array<Vector3>> Q;
    Q.set_length(3);
    for (size_t i = 0; i < 3; ++i) {
        Q[i].set_length(4);
        for (size_t j = 0; j < 4; ++j) {
            Q[i][j] = Vector3(0.3 * i - 0.11 * j, 0.25 * j + 0.07 * i,
                              0.13 * i * j - 0.05 * (i + j) + 0.02);
        }
    }
    return Q;
}

// Mirror of dump_members() in the probe.
void dump_members(const std::string& tag, const BezierPatch& bp) {
    emitf("%s str=%s", tag, bp.toString());
    emitf("%s classname=%s", tag, bp.classString());
    emitf("%s n=%d m=%d Q.length=%d", tag, bp.n, bp.m, static_cast<int>(bp.Q.length()));
    for (size_t i = 0; i < bp.Q.length(); ++i) {
        emitf("%s Q[%d].length=%d", tag, static_cast<int>(i), static_cast<int>(bp.Q[i].length()));
        for (size_t j = 0; j < bp.Q[i].length(); ++j) {
            const Vector3& v = bp.Q[i][j];
            emitf("%s Q[%d][%d] x=%.17g y=%.17g z=%.17g", tag, static_cast<int>(i),
                  static_cast<int>(j), eil::re(v.x), eil::re(v.y), eil::re(v.z));
        }
    }
}

void dump_sweep(const std::string& tag, const ParametricSurface& ps) {
    for (double r : {0.0, 0.2, 0.5, 0.9, 1.0}) {
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

// Mirror of dump_throw(): the escaped TYPE is part of the observable, and ErrorBase and
// ExceptionBase are siblings in this port, so the two catch clauses below are the same
// two-branch test D's `catch (Error)` / `catch (Exception)` pair is.
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

// D's join(split(text,"\n"), "<newline>").
std::string one_line(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::string out;
    size_t pos = 0;
    while (true) {
        size_t nl = body.find('\n', pos);
        if (nl == std::string::npos) {
            out += body.substr(pos);
            break;
        }
        out += body.substr(pos, nl - pos) + "<newline>";
        pos = nl + 1;
    }
    return out;
}

size_t file_len(const std::string& path) {
    std::ifstream in(path, std::ios::ate | std::ios::binary);
    return static_cast<size_t>(in.tellg());
}

using Net = eil::Array<eil::Array<Vector3>>;

Net inner_nets(std::initializer_list<size_t> inner_lengths) {
    Net q;
    q.set_length(inner_lengths.size());
    size_t i = 0;
    for (size_t len : inner_lengths) q[i++].set_length(len);
    return q;
}

// ============================================================================
// A. bezierpatch.d's unittest (D:107-127), assertion for assertion.
// ============================================================================
void port_d_unittest() {
    double L = 2.0;
    double H = 1.0;
    int n = 3;
    int m = 4;
    double dx = L / n;
    double dy = H / m;
    Net Q;
    Q.set_length(static_cast<size_t>(n) + 1);
    for (int i = 0; i < n + 1; ++i) {
        Q[static_cast<size_t>(i)].set_length(static_cast<size_t>(m) + 1);
        for (int j = 0; j < m + 1; ++j) {
            Q[static_cast<size_t>(i)][static_cast<size_t>(j)] = Vector3(i * dx, j * dy, 0.0);
        }
    }
    auto bezPatch = std::make_shared<BezierPatch>(Q, n, m);
    auto p = bezPatch->operator()(0.2, 0.25);
    // D:124 assert(approxEqualVectors(p, Vector3(0.4, 0.25, 0.0)));
    check(eil::geom::approxEqualVectors(p, Vector3(0.4, 0.25, 0.0)),
          "bezierpatch.d:124 approxEqualVectors(bezPatch(0.2, 0.25), Vector3(0.4, 0.25, 0.0))");
    p = bezPatch->operator()(0.9, 0.1);
    // D:126 assert(approxEqualVectors(p, Vector3(1.8, 0.1, 0.0)));
    check(eil::geom::approxEqualVectors(p, Vector3(1.8, 0.1, 0.0)),
          "bezierpatch.d:126 approxEqualVectors(bezPatch(0.9, 0.1), Vector3(1.8, 0.1, 0.0))");
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]);
        return 2;
    }

    port_d_unittest();

    //--- group B: the D unittest net, its copy ctor, and dup() -----------------
    Net Q = make_unit_net();
    auto bp = std::make_shared<BezierPatch>(Q, 3, 4);
    dump_members("bzp.unit", *bp);
    dump_sweep("bzp.unit", *bp);
    dump_area("bzp.unit", *bp, 10, 10);
    dump_area("bzp.unit", *bp, 3, 7);

    auto cp = std::make_shared<BezierPatch>(*bp); // D:37-46
    dump_members("bzp.copy", *cp);
    dump_sweep("bzp.copy", *cp);
    dump_area("bzp.copy", *cp, 10, 10);

    auto dp = bp->dup(); // D:48-51
    dump_members("bzp.dup", *std::dynamic_pointer_cast<BezierPatch>(dp));
    dump_sweep("bzp.dup", *dp);
    dump_area("bzp.dup", *dp, 10, 10);

    Q[0][0] = Vector3(99.0, 98.0, 97.0);
    emitf("bzp.aliaschk Q[0][0] x=%.17g y=%.17g z=%.17g", eil::re(bp->Q[0][0].x),
          eil::re(bp->Q[0][0].y), eil::re(bp->Q[0][0].z));

    //--- group C: a non-planar net ---------------------------------------------
    Net Qt = make_twist_net();
    auto tw = std::make_shared<BezierPatch>(Qt, 2, 3);
    dump_members("bzp.twist", *tw);
    dump_sweep("bzp.twist", *tw);
    dump_area("bzp.twist", *tw, 10, 10);
    dump_area("bzp.twist", *tw, 3, 7);
    dump_area("bzp.twist", *tw, 1, 1);
    emitf("bzp.twist str.as.base=%s", (static_cast<ParametricSurface&>(*tw)).toString());

    //--- group D: jagged nets --------------------------------------------------
    Net jg = inner_nets({4, 2, 3});
    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < jg[i].length(); ++j) {
            jg[i][j] = Vector3(0.5 * i + 0.25 * j, 1.0 - 0.3 * i, 0.1 * j - 0.05 * i);
        }
    }
    auto jp = std::make_shared<BezierPatch>(jg, 2, 3);
    dump_members("bzp.jagged", *jp);
    dump_sweep("bzp.jagged", *jp);
    dump_area("bzp.jagged", *jp, 10, 10);

    //--- group E: constructor and evaluation guards ---------------------------
    Net q0 = inner_nets({3});
    for (size_t j = 0; j < 3; ++j) q0[0][j] = Vector3(0.5 * j, 0.25 * j, 0.0);
    auto p0 = std::make_shared<BezierPatch>(q0, 0, 2);
    dump_members("bzp.n0", *p0);
    dump_throw("bzp.n0.opCall", [&p0]() { (*p0)(0.5, 0.5); });
    dump_throw("bzp.n0.area", [&p0]() { p0->area(); });

    Net qm1;
    dump_throw("bzp.nm1.ctor", [&qm1]() { BezierPatch(qm1, -1, 0); });
    auto pm1 = std::make_shared<BezierPatch>(qm1, -1, 0);
    dump_members("bzp.nm1", *pm1);
    dump_throw("bzp.nm1.opCall", [&pm1]() { (*pm1)(0.3, 0.7); });
    dump_throw("bzp.nm1.area", [&pm1]() { pm1->area(); });

    dump_throw("bzp.nm2.ctor", [&qm1]() { BezierPatch(qm1, -2, 0); });

    Net one = inner_nets({1, 1});
    one[0][0] = Vector3(1.0, 2.0, 3.0);
    one[1][0] = Vector3(4.0, 5.0, 6.0);
    dump_throw("bzp.inner1.ctor", [&one]() { BezierPatch(one, 1, 0); });

    Net empty_inner = inner_nets({0, 0});
    dump_throw("bzp.inner0.ctor", [&empty_inner]() { BezierPatch(empty_inner, 1, 1); });

    Net shortQ = inner_nets({2, 2});
    // D: foreach (i; 0 .. 2) foreach (j; 0 .. 2) shortQ[i][j] = Vector3(i + j, i - j, 0.0);
    // — i and j are ints there, so `i - j` is signed and goes negative. Kept signed.
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            shortQ[static_cast<size_t>(i)][static_cast<size_t>(j)] = Vector3(i + j, i - j, 0.0);
        }
    }
    dump_throw("bzp.short.ctor", [&shortQ]() { BezierPatch(shortQ, 4, 1); });

    auto sp = std::make_shared<BezierPatch>(shortQ, 1, 9);
    dump_members("bzp.mismatch", *sp);
    dump_sweep("bzp.mismatch", *sp);
    dump_throw("bzp.overlong.ctor", [&shortQ]() { BezierPatch(shortQ, 2, 9); });

    //--- group F: write_ctrl_pts_as_vtk_xml -----------------------------------
    const std::string f1 = "/tmp/probe_bzp_unit.vtk";
    const std::string f2 = "/tmp/probe_bzp_twist.vtk";
    const std::string f3 = "/tmp/probe_bzp_jag.vtk";
    const std::string f4 = "/tmp/probe_bzp_empty.vtk";
    eil::geom::write_ctrl_pts_as_vtk_xml(*bp, f1);
    emitf("bzp.vtk.unit len=%d", static_cast<int>(file_len(f1)));
    emitf("bzp.vtk.unit.body=%s", one_line(f1));

    eil::geom::write_ctrl_pts_as_vtk_xml(*tw, f2);
    emitf("bzp.vtk.twist len=%d", static_cast<int>(file_len(f2)));
    emitf("bzp.vtk.twist.body=%s", one_line(f2));

    dump_throw("bzp.vtk.jagged", [&jp, &f3]() { eil::geom::write_ctrl_pts_as_vtk_xml(*jp, f3); });
    dump_throw("bzp.vtk.empty",
               [&pm1, &f4]() { eil::geom::write_ctrl_pts_as_vtk_xml(*pm1, f4); });
    dump_throw("bzp.vtk.badpath", [&bp]() {
        eil::geom::write_ctrl_pts_as_vtk_xml(*bp, "/nonexistent-dir-xyz/a.vtk");
    });

    for (const std::string& f : {f1, f2, f3, f4}) std::remove(f.c_str());

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
        "%s: geom_surface_bezierpatch, %d/%d unittest assertions hold (bezierpatch.d:107-127); "
        "probe replay %zu D lines, %zu C++ lines, %d differ; %zu of %zu rows byte-identical, "
        "0 rows accepted through a tolerance band\n",
        (diffs == 0 && g_failures == 0) ? "PASS" : "FAIL", g_checks - g_failures, g_checks,
        expected.size(), g_lines.size(), diffs, common - static_cast<size_t>(diffs), common);
    return (diffs == 0 && g_failures == 0) ? 0 : 1;
}
