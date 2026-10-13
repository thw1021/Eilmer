// tests/test_geom_surface_nurbssurface.cpp — the D `unittest` block of
// geom/surface/nurbssurface.d ported assertion by assertion, plus a line-for-line
// replay of probes/geom/surface/probe_nurbssurface.d against
// baselines/e587b235/probes/geom_surface_nurbssurface.txt (200 D rows).
//
// Part A is the D unittest (D:138-156) in source order; it has ONE assertion, at D:155.
// Part B replays the corpus, which is where the module's real risks live: five Error
// guards, four ArrayIndexError paths from the shared static buffers, and the m_b quirk.
//
// Throwable-name mapping used in part B (what the rows compare is WHICH branch fired,
// and the escaped message text):
//   eil::ExceptionBase -> "object.Exception"   (findSpan, nurbs_utils.d:33)
//   eil::RangeError    -> "core.exception.ArrayIndexError"  (out-of-bounds indexing)
//   eil::ErrorBase     -> "object.Error"       (the five constructor guards, D:28..:58)
// D prints these names itself (t.classinfo.name, rows 96-105 and 111-135 of the
// baseline), and the message bodies are the port's own text — an
// eil::RangeError("index [4] is out of bounds for array of length 4") only matches if
// the port really did index U[4] of a length-4 knot vector.
//
// Regenerate (one line, so this comment stays a single // comment):
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface probes/geom/surface/probe_nurbssurface.d -of=/tmp/probe_ns -od=/tmp/w2_surfB/obj_ns
//   then /tmp/probe_ns > baselines/e587b235/probes/geom_surface_nurbssurface.txt
//
// Run: ./test_geom_surface_nurbssurface <baseline.txt>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/surface/nurbssurface.hpp"
#include "geom/surface/parametricsurface.hpp"

namespace {

using eil::geom::NURBSSurface;
using eil::geom::ParametricSurface;
using eil::geom::PwArray;
using eil::geom::Vector3;
std::vector<std::string> g_lines;
int g_unittest_failures = 0;

void emit(const std::string& s) { g_lines.push_back(s); }

std::string esc(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '\n') out += "<newline>";
        else if (c == '\r') out += "<cr>";
        else out += c;
    }
    return out;
}

// D's t.classinfo.name for the three throwable shapes this module can produce.
std::string d_throw_name(const eil::ExceptionBase&) { return "object.Exception"; }
std::string d_throw_name(const eil::RangeError&) { return "core.exception.ArrayIndexError"; }
std::string d_throw_name(const eil::ErrorBase&) { return "object.Error"; }

// One row for a call that may throw, with D's class name and the escaped message.
template <typename F> void attempt(const std::string& key, F&& f) {
    try {
        f();
    } catch (const eil::RangeError& e) {
        emit(key + " thrown type=" + d_throw_name(e) + " msg=" + esc(e.msg()));
    } catch (const eil::ErrorBase& e) {
        emit(key + " thrown type=" + d_throw_name(e) + " msg=" + esc(e.msg()));
    } catch (const eil::ExceptionBase& e) {
        emit(key + " thrown type=" + d_throw_name(e) + " msg=" + esc(e.msg()));
    }
}

void dump_v3(const std::string& key, const Vector3& v) {
    emit(eil::format("%s x=%.17g y=%.17g z=%.17g", key, eil::re(v.x), eil::re(v.y),
                     eil::re(v.z)));
}

void eval_at(const std::string& key, const NURBSSurface& s, double u, double v) {
    attempt(key, [&] { dump_v3(key, s(u, v)); });
}

NURBSSurface::Pw2 a4(double x, double y, double z, double w) {
    NURBSSurface::Pw2 p(1);
    p[0] = eil::Array<std::array<double, 4>>(1);
    p[0][0] = std::array<double, 4>{x, y, z, w};
    return p;
}

// grid(nu, nv, w) from the probe: x = i, y = j, z = 0.25*(i+j), weight = w.
NURBSSurface::Pw2 grid(int nu, int nv, double w) {
    NURBSSurface::Pw2 p;
    p.set_length(static_cast<size_t>(nu));
    for (int i = 0; i < nu; ++i) {
        p[static_cast<size_t>(i)].set_length(static_cast<size_t>(nv));
        for (int j = 0; j < nv; ++j) {
            p[static_cast<size_t>(i)][static_cast<size_t>(j)] =
                std::array<double, 4>{static_cast<double>(i), static_cast<double>(j),
                                      0.25 * (i + j), w};
        }
    }
    return p;
}

// net44(w0) from the probe: the non-planar 4x4 net.
NURBSSurface::Pw2 net44(double w0) {
    NURBSSurface::Pw2 p;
    p.set_length(4);
    for (int i = 0; i < 4; ++i) {
        p[static_cast<size_t>(i)].set_length(4);
        for (int j = 0; j < 4; ++j) {
            p[static_cast<size_t>(i)][static_cast<size_t>(j)] =
                std::array<double, 4>{0.5 * i + 0.1 * i * j, 0.25 * j * j - 0.1 * i,
                                      0.2 * (i + 1) * (j + 1), w0 + 0.2 * i * j};
        }
    }
    return p;
}

eil::Array<double> knots(std::initializer_list<double> v) { return eil::Array<double>(v); }

void try_build(const std::string& key, const NURBSSurface::Pw2& Pw, eil::Slice<double> U, int p,
               eil::Slice<double> V, int q) {
    attempt(key, [&] {
        NURBSSurface s(Pw, U, p, V, q);
        emit(key + " built str=" + s.toString());
    });
}

void dump_area(const std::string& key, ParametricSurface& ps, int nr, int ns) {
    attempt(key, [&] {
        auto a = ps.area(nr, ns);
        emit(eil::format("%s x=%.17g y=%.17g z=%.17g", key, eil::re(a.x), eil::re(a.y),
                         eil::re(a.z)));
    });
}

// A check from a D unittest: prints nothing on success, names the D line on failure.
void ut_assert(bool cond, int d_line, const std::string& what) {
    if (!cond) {
        ++g_unittest_failures;
        std::printf("D unittest assertion at nurbssurface.d:%d FAILED: %s\n", d_line,
                    what.c_str());
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]);
        return 2;
    }

    // ============================================================================
    // Part A — the D unittest block (nurbssurface.d:138-156), in source order.
    // ============================================================================
    {
        // D:139-148. Ex4.3 from Piegl and Tiller (1997); only the 'activated' control
        // points carry values, the rest are zero-weight and contribute nothing.
        NURBSSurface::Pw2 Pw;
        Pw.set_length(8);
        const double vals[8][5][3] = {
            {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
            {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
            {{0, 0, 0}, {0, 2, 4}, {0, 6, 4}, {0, 2, 0}, {0, 0, 0}},
            {{0, 0, 0}, {4, 6, 8}, {12, 24, 12}, {4, 6, 0}, {0, 0, 0}},
            {{0, 0, 0}, {4, 2, 4}, {8, 6, 4}, {4, 2, 0}, {0, 0, 0}},
            {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
            {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
            {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}};
        const double ws[8][5] = {{0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 1, 2, 1, 0},
                                 {2, 6, 6, 2, 0}, {1, 2, 2, 1, 0}, {0, 0, 0, 0, 0},
                                 {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}};
        for (int i = 0; i < 8; ++i) {
            Pw[static_cast<size_t>(i)].set_length(5);
            for (int j = 0; j < 5; ++j) {
                Pw[static_cast<size_t>(i)][static_cast<size_t>(j)] =
                    std::array<double, 4>{vals[i][j][0], vals[i][j][1], vals[i][j][2],
                                          ws[i][j]};
            }
        }
        int p = 2; // D:149
        int q = 2; // D:150
        auto U = knots({0.0, 0.0, 0.0, 1.0, 2.0, 3.0, 4.0, 4.0, 5.0, 5.0, 5.0}); // D:151
        auto V = knots({0.0, 0.0, 0.0, 1.0, 2.0, 3.0, 3.0, 3.0});                 // D:152
        NURBSSurface nsurf(Pw, U.all(), p, V.all(), q);                           // D:154
        // D:155 assert(approxEqualVectors(Vector3(2.0, 98.0/27.0, 68.0/27.0),
        //                  nsurf.opCall(5.0/2.0, 1.0)));
        Vector3 got = nsurf(5.0 / 2.0, 1.0);
        Vector3 want(2.0, 98.0 / 27.0, 68.0 / 27.0);
        ut_assert(eil::geom::approxEqualVectors(want, got), 155,
                  "Ex4.3 point at (u,v)=(2.5,1)");
        // Same check, spelled with the numbers, so a failure prints what it saw.
        if (!eil::geom::approxEqualVectors(want, got)) {
            std::printf("  got x=%.17g y=%.17g z=%.17g want x=%.17g y=%.17g z=%.17g\n",
                        eil::re(got.x), eil::re(got.y), eil::re(got.z), eil::re(want.x),
                        eil::re(want.y), eil::re(want.z));
        }
    }

    // ============================================================================
    // Part B — corpus replay, in the probe's exact statement order (the static
    // buffers make the order observable).
    // ============================================================================
    auto Pw43 = [&] {
        NURBSSurface::Pw2 Pw;
        Pw.set_length(8);
        for (int i = 0; i < 8; ++i) {
            Pw[static_cast<size_t>(i)].set_length(5);
            for (int j = 0; j < 5; ++j) {
                double x = 0, y = 0, z = 0, w = 0;
                if (i == 2 && j == 1) { y = 2; z = 4; w = 1; }
                if (i == 2 && j == 2) { y = 6; z = 4; w = 2; }
                if (i == 2 && j == 3) { y = 2; w = 1; }
                if (i == 3 && j == 1) { x = 4; y = 6; z = 8; w = 2; }
                if (i == 3 && j == 2) { x = 12; y = 24; z = 12; w = 6; }
                if (i == 3 && j == 3) { x = 4; y = 6; w = 2; }
                if (i == 4 && j == 1) { x = 4; y = 2; z = 4; w = 1; }
                if (i == 4 && j == 2) { x = 8; y = 6; z = 4; w = 2; }
                if (i == 4 && j == 3) { x = 4; y = 2; w = 1; }
                Pw[static_cast<size_t>(i)][static_cast<size_t>(j)] =
                    std::array<double, 4>{x, y, z, w};
            }
        }
        return Pw;
    }();
    auto U43 = knots({0.0, 0.0, 0.0, 1.0, 2.0, 3.0, 4.0, 4.0, 5.0, 5.0, 5.0});
    auto V43 = knots({0.0, 0.0, 0.0, 1.0, 2.0, 3.0, 3.0, 3.0});
    NURBSSurface e43(Pw43, U43.all(), 2, V43.all(), 2);

    // group 1
    emit("ns.e43 str=" + e43.toString());
    eval_at("ns.e43.unittest u=2.5 v=1", e43, 5.0 / 2.0, 1.0);
    for (double u : {0.0, 1.0, 2.0, 2.5, 3.5, 4.0, 5.0}) {
        for (double v : {0.0, 1.0, 2.0, 2.5, 3.0}) {
            eval_at(eil::format("ns.e43.sweep u=%.17g v=%.17g", u, v), e43, u, v);
        }
    }

    // group 2
    auto Uc = knots({0.0, 0.0, 0.0, 0.5, 1.0, 1.0, 1.0});
    auto g = std::make_shared<NURBSSurface>(grid(4, 4, 1.0), Uc.all(), 2, Uc.all(), 2);
    emit("ns.grid str=" + g->toString());
    for (double u : {0.0, 0.25, 0.5, 0.75, 1.0}) {
        for (double v : {0.0, 0.25, 0.5, 0.75, 1.0}) {
            eval_at(eil::format("ns.grid.sweep u=%.17g v=%.17g", u, v), *g, u, v);
        }
    }
    eval_at("ns.grid.corner00", *g, 0.0, 0.0);
    eval_at("ns.grid.corner10", *g, 1.0, 0.0);
    eval_at("ns.grid.corner01", *g, 0.0, 1.0);
    eval_at("ns.grid.corner11", *g, 1.0, 1.0);

    auto gw = std::make_shared<NURBSSurface>(grid(4, 4, 2.5), Uc.all(), 2, Uc.all(), 2);
    emit("ns.gridw str=" + gw->toString());
    for (double u : {0.0, 0.5, 1.0}) {
        for (double v : {0.0, 0.5, 1.0}) {
            eval_at(eil::format("ns.gridw.sweep u=%.17g v=%.17g", u, v), *gw, u, v);
        }
    }
    auto gz = std::make_shared<NURBSSurface>(grid(4, 4, 0.0), Uc.all(), 2, Uc.all(), 2);
    emit("ns.gridz str=" + gz->toString());
    for (double u : {0.0, 0.5, 1.0}) {
        for (double v : {0.0, 0.5, 1.0}) {
            eval_at(eil::format("ns.gridz.sweep u=%.17g v=%.17g", u, v), *gz, u, v);
        }
    }

    // group 3: the five constructor guards
    auto U1 = knots({0.0, 0.0, 0.0, 1.0, 1.0, 1.0});
    try_build("ns.guards.oneRow", grid(1, 2, 1.0), U1.all(), 0, Uc.all(), 1);
    try_build("ns.guards.oneCol", grid(2, 1, 1.0), Uc.all(), 1, U1.all(), 0);
    try_build("ns.guards.pTooBig", grid(4, 4, 1.0), Uc.all(), 4, Uc.all(), 2);
    try_build("ns.guards.qTooBig", grid(4, 4, 1.0), Uc.all(), 2, Uc.all(), 5);
    try_build("ns.guards.shortU", grid(4, 4, 1.0), Uc.slice(0, 5), 2, Uc.all(), 2);
    try_build("ns.guards.shortV", grid(4, 4, 1.0), Uc.all(), 2, Uc.slice(0, 5), 2);
    try_build("ns.guards.negP", grid(4, 4, 1.0), Uc.all(), -1, Uc.all(), 2);
    try_build("ns.guards.negQ", grid(4, 4, 1.0), Uc.all(), 2, Uc.all(), -1);
    try_build("ns.guards.zeroRows", grid(0, 2, 1.0), Uc.all(), 2, Uc.all(), 2);
    try_build("ns.guards.deg0", grid(2, 2, 1.0), U1.all(), 0, U1.all(), 0);

    // group 4: findSpan's Exception and the tol clamping
    eval_at("ns.range.u.below", *g, -0.1, 0.5);
    eval_at("ns.range.u.above", *g, 1.1, 0.5);
    eval_at("ns.range.v.below", *g, 0.5, -0.25);
    eval_at("ns.range.v.above", *g, 0.5, 2.0);
    eval_at("ns.clamp.u.below", *g, -1e-13, 0.5);
    eval_at("ns.clamp.u.above", *g, 1.0 + 1e-13, 0.5);
    eval_at("ns.clamp.v.below", *g, 0.5, -1e-13);
    eval_at("ns.clamp.v.above", *g, 0.5, 1.0 + 1e-13);
    eval_at("ns.e43.outside", e43, 5.5, 1.0);

    // group 5: construction order against the shared static buffers
    eval_at("ns.ord.before", *g, 0.25, 0.75);
    auto Ucubic = knots({0.0, 0.0, 0.0, 0.0, 0.5, 1.0, 1.0, 1.0, 1.0});
    auto Vlin = knots({0.0, 0.0, 1.0, 1.0});
    auto cub = std::make_shared<NURBSSurface>(grid(5, 2, 1.0), Ucubic.all(), 3, Vlin.all(), 1);
    emit("ns.ord.cub str=" + cub->toString());
    eval_at("ns.ord.cub.a", *cub, 0.0, 0.0);
    eval_at("ns.ord.cub.b", *cub, 0.5, 1.0);
    eval_at("ns.ord.afterBig", *g, 0.25, 0.75);
    auto Ulin = knots({0.0, 0.0, 1.0, 1.0});
    auto lin = std::make_shared<NURBSSurface>(grid(2, 2, 1.0), Ulin.all(), 1, Ulin.all(), 1);
    emit("ns.ord.lin str=" + lin->toString());
    eval_at("ns.ord.lin.a", *lin, 0.0, 0.5);
    eval_at("ns.ord.afterSmall", *g, 0.25, 0.75);
    eval_at("ns.ord.afterSmall2", *g, 0.0, 1.0);
    auto g2 = std::make_shared<NURBSSurface>(grid(4, 4, 1.0), Uc.all(), 2, Uc.all(), 2);
    eval_at("ns.ord.rebuilt", *g2, 0.25, 0.75);
    eval_at("ns.ord.old.afterRebuild", *g, 0.25, 0.75);

    // group 6: dup() vs the copy constructor (the m_b quirk at nurbssurface.d:80)
    auto cubD = std::dynamic_pointer_cast<NURBSSurface>(cub->dup());
    auto cubC = std::make_shared<NURBSSurface>(*cub);
    for (double u : {0.0, 0.5, 1.0}) {
        for (double v : {0.0, 1.0}) {
            eval_at(eil::format("ns.cub.orig u=%.17g v=%.17g", u, v), *cub, u, v);
            eval_at(eil::format("ns.cub.dup u=%.17g v=%.17g", u, v), *cubD, u, v);
            eval_at(eil::format("ns.cub.copy u=%.17g v=%.17g", u, v), *cubC, u, v);
        }
    }
    auto gp = std::make_shared<NURBSSurface>(net44(1.0), Uc.all(), 2, Uc.all(), 2);
    emit("ns.gp str=" + gp->toString());
    auto gpD = std::dynamic_pointer_cast<NURBSSurface>(gp->dup());
    auto gpC = std::make_shared<NURBSSurface>(*gp);
    emit("ns.gp.dup str=" + gpD->toString());
    emit("ns.gp.copy str=" + gpC->toString());
    for (double u : {0.0, 0.3, 1.0}) {
        for (double v : {0.0, 0.25, 0.5}) {
            eval_at(eil::format("ns.gp.orig u=%.17g v=%.17g", u, v), *gp, u, v);
            eval_at(eil::format("ns.gp.dup u=%.17g v=%.17g", u, v), *gpD, u, v);
            eval_at(eil::format("ns.gp.copy u=%.17g v=%.17g", u, v), *gpC, u, v);
        }
    }
    auto dp = std::dynamic_pointer_cast<NURBSSurface>(g->dup());
    auto cp = std::make_shared<NURBSSurface>(*g);
    emit("ns.dup str=" + dp->toString());
    emit("ns.copy str=" + cp->toString());
    // v = 1.0 is left out for the copy: D's findSpan does not return for it (see the
    // probe header and nurbssurface.hpp) — an unbounded loop produces no corpus row.
    for (double u : {0.0, 0.25, 0.5, 1.0}) {
        for (double v : {0.0, 0.5}) {
            eval_at(eil::format("ns.orig u=%.17g v=%.17g", u, v), *g, u, v);
            eval_at(eil::format("ns.dup u=%.17g v=%.17g", u, v), *dp, u, v);
            eval_at(eil::format("ns.copy u=%.17g v=%.17g", u, v), *cp, u, v);
        }
    }
    for (double u : {0.0, 0.5, 1.0}) {
        eval_at(eil::format("ns.orig.v1 u=%.17g", u), *g, u, 1.0);
        eval_at(eil::format("ns.dup.v1 u=%.17g", u), *dp, u, 1.0);
    }

    // group 7: area() through the base class
    dump_area("ns.area.default", *g, 10, 10);
    dump_area("ns.area.r3s7", *g, 3, 7);
    dump_area("ns.area.e43r4s2", e43, 4, 2);

    // ---- replay ---------------------------------------------------------------
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
        const std::string c = i < g_lines.size() ? g_lines[i] : "<none>";
        if (e == c) continue;
        if (shown < 12) {
            std::printf("line %zu\n  D:   |%s|\n  C++: |%s|\n", i + 1, e.c_str(), c.c_str());
            ++shown;
        }
        ++diffs;
    }
    std::printf("%s: geom_surface_nurbssurface, D unittest 1 assertion (nurbssurface.d:155), "
                "%d failed; probe replay %zu D rows, %zu C++ rows, %d differ; %zu of %zu rows "
                "byte-identical\n",
                (diffs == 0 && g_unittest_failures == 0) ? "PASS" : "FAIL", g_unittest_failures,
                expected.size(), g_lines.size(), diffs,
                std::min(expected.size(), g_lines.size()) - static_cast<size_t>(diffs),
                std::min(expected.size(), g_lines.size()));
    return (diffs == 0 && g_unittest_failures == 0) ? 0 : 1;
}
