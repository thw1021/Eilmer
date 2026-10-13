// tests/test_geom_surface_ruledsurface.cpp — C++ mirror of
// probes/geom/surface/probe_ruledsurface.d, replayed line for line against
// baselines/e587b235/probes/geom_surface_ruledsurface.txt (598 D rows).
//
// geom/surface/ruledsurface.d has NO `unittest` block (measured: 0 occurrences of the
// keyword in the 88-line file), so this replay is the whole gate for the module.
//
// Groups and what each one can still catch:
//   rs.dir{r,i,s,j}.*, rs2d.dir{r,i,s,j}.*
//                  10 rows of member dump + 36 opCall rows per object, for the four
//                  accepted direction strings at pure2D false and true (2 x 4 x 46 =
//                  368 rows). The corner dump order is p00, p10, p01, p11 and the
//                  VALUES are what separate the two families: "r"/"i" take edge0 as the
//                  west edge (p01 = edge0(1)) while "s"/"j" take it as the south edge
//                  (p10 = edge0(1)). A port that swapped those two lines would keep every
//                  opCall row identical and fail only here.
//   rs.bezS/rs.bezR/rs.sameEdge/rs.zeroLen/rs.bez2d
//                  a curved (Bezier) edge in each slot, two coincident edges, and a
//                  zero-length Line — the degenerate inputs a grid generator hits.
//   rs.area.*      the base class' area() at D's defaults and at (3,7), (7,3), (1,1),
//                  plus the two degenerate patches.
//   rs.dup.*, rs.copy.*
//                  the two copy routes D offers; dup() re-runs the validating
//                  constructor, the copy constructor does not. Mutating cb.p11 after
//                  making the copy proves the corners are values, not aliases.
//   rs.ctorGuard.* the constructor's `default:` throw for six invalid strings, and
//                  rs.tampered.* the same for opCall, area() and dup(), reached by
//                  writing the public field.
//                  D prints `type=object.Exception` for these (measured, rows 585-598 of
//                  the baseline). The port prints the same bytes by naming the CATCH
//                  BRANCH that fired: eil::ExceptionBase -> "object.Exception",
//                  eil::ErrorBase -> "object.Error". What is under test is which sibling
//                  threw, and a port that threw ErrorBase where D throws Exception would
//                  print a different row and fail the diff. The last group proves the
//                  two really are siblings by giving the throw both handlers, exactly as
//                  D's `catch (Error) / catch (Exception)` chain does; D reaches the
//                  Exception handler (row rs.errorHandler.fellThroughToException).
//
// Regenerate (one line, so this comment stays a single // comment):
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface probes/geom/surface/probe_ruledsurface.d -of=/tmp/probe_rs -od=/tmp/w2_surfB/obj_rs
//   then /tmp/probe_rs > baselines/e587b235/probes/geom_surface_ruledsurface.txt
//
// Run: ./test_geom_surface_ruledsurface <baseline.txt>
#include <algorithm>
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
#include "geom/gpath/bezier.hpp"
#include "geom/gpath/line.hpp"
#include "geom/gpath/path.hpp"
#include "geom/surface/parametricsurface.hpp"
#include "geom/surface/ruledsurface.hpp"

namespace {

using eil::geom::ParametricSurface;
using eil::geom::RuledSurface;
using eil::geom::Vector3;
std::vector<std::string> g_lines;

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

// The D-side spelling of t.classinfo.name for the two throwable siblings the port has.
std::string d_type_name(const eil::ExceptionBase&) { return "object.Exception"; }
std::string d_type_name(const eil::ErrorBase&) { return "object.Error"; }

void dump_v3(const std::string& key, const Vector3& v) {
    emit(eil::format("%s x=%.17g y=%.17g z=%.17g", key, eil::re(v.x), eil::re(v.y),
                     eil::re(v.z)));
}

void dump_all(const std::string& key, const RuledSurface& rs) {
    emit(eil::format("%s str=%s", key, rs.toString()));
    emit(eil::format("%s dir=%s pure2D=%s", key, rs.ruled_direction, eil::to_string(rs.pure2D)));
    emit(eil::format("%s e0=%s e1=%s", key, eil::geom::d_tostr(rs.edge0),
                     eil::geom::d_tostr(rs.edge1)));
    dump_v3(key + ".p00", rs.p00);
    dump_v3(key + ".p10", rs.p10);
    dump_v3(key + ".p01", rs.p01);
    dump_v3(key + ".p11", rs.p11);
}

void sweep(const std::string& key, const RuledSurface& rs) {
    for (double r : {0.0, 0.125, 0.3, 0.7, 0.875, 1.0}) {
        for (double s : {0.0, 0.125, 0.3, 0.7, 0.875, 1.0}) {
            dump_v3(eil::format("%s.opCall r=%.17g s=%.17g", key, r, s), rs(r, s));
        }
    }
}

void dump_area(const std::string& key, ParametricSurface& ps, int nr, int ns) {
    auto a = ps.area(nr, ns);
    emit(eil::format("%s nr=%d ns=%d area x=%.17g y=%.17g z=%.17g", key, nr, ns, eil::re(a.x),
                     eil::re(a.y), eil::re(a.z)));
}

// One catch chain, mirroring D's `catch (Exception e)`: the arm that fires is printed
// under D's spelling of classinfo.name, so a port that threw the wrong sibling (or
// nothing at all) produces a different row. D's `catch (Exception)` could not catch an
// Error and would let it terminate; the extra arm here exists only to name what fired
// rather than to swallow it.
template <typename F> void guard_row(F&& body, const std::string& key) {
    try {
        body();
    } catch (const eil::ExceptionBase& e) {
        emit(key + " type=" + d_type_name(e) + " msg=" + esc(e.msg()));
    } catch (const eil::ErrorBase& e) {
        emit(key + " type=" + d_type_name(e) + " msg=" + esc(e.msg()));
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]);
        return 2;
    }

    auto west = std::make_shared<eil::geom::Line>(Vector3(0.0, 0.0, 0.0), Vector3(0.0, 1.0, 0.25));
    auto east = std::make_shared<eil::geom::Line>(Vector3(2.0, 0.5, -0.5),
                                                  Vector3(2.0, 1.5, 0.75));
    auto bez = std::make_shared<eil::geom::Bezier>(eil::Array<Vector3>{
        Vector3(0.5, -1.0, 0.125), Vector3(1.0, 0.5, -0.25), Vector3(1.5, 1.0, 0.5),
        Vector3(2.5, 2.0, -0.125)});

    // --- group 1 -----------------------------------------------------------------
    for (const std::string& dir : {std::string("r"), std::string("i"), std::string("s"),
                                   std::string("j")}) {
        RuledSurface rs(west, east, dir, false);
        dump_all("rs.dir" + dir, rs);
        sweep("rs.dir" + dir, rs);
        RuledSurface rs2d(west, east, dir, true);
        dump_all("rs2d.dir" + dir, rs2d);
        sweep("rs2d.dir" + dir, rs2d);
    }

    // --- group 2 -----------------------------------------------------------------
    auto cb = std::make_shared<RuledSurface>(west, bez, "s", false);
    dump_all("rs.bezS", *cb);
    sweep("rs.bezS", *cb);
    auto cr = std::make_shared<RuledSurface>(bez, east, "r", false);
    dump_all("rs.bezR", *cr);
    sweep("rs.bezR", *cr);
    auto same = std::make_shared<RuledSurface>(west, west, "s", false);
    dump_all("rs.sameEdge", *same);
    sweep("rs.sameEdge", *same);
    auto zeroLine = std::make_shared<eil::geom::Line>(Vector3(1.0, 2.0, 3.0),
                                                      Vector3(1.0, 2.0, 3.0));
    auto zero = std::make_shared<RuledSurface>(zeroLine, east, "r", false);
    dump_all("rs.zeroLen", *zero);
    sweep("rs.zeroLen", *zero);
    auto bez2d = std::make_shared<RuledSurface>(bez, bez, "j", true);
    dump_all("rs.bez2d", *bez2d);
    sweep("rs.bez2d", *bez2d);

    // --- group 3 -----------------------------------------------------------------
    dump_area("rs.area.default", *cb, 10, 10);
    dump_area("rs.area.r3s7", *cb, 3, 7);
    dump_area("rs.area.r7s3", *cb, 7, 3);
    dump_area("rs.area.r1s1", *cb, 1, 1);
    dump_area("rs.area.same.default", *same, 10, 10);
    dump_area("rs.area.zero.default", *zero, 10, 10);
    dump_area("rs.area.bez2d.r5s2", *bez2d, 5, 2);

    // --- group 4 -----------------------------------------------------------------
    auto d = std::dynamic_pointer_cast<RuledSurface>(cb->dup());
    dump_all("rs.dup", *d);
    dump_v3("rs.dup.opCall", (*d)(0.3, 0.7));
    auto cp = std::make_shared<RuledSurface>(*cb);
    dump_all("rs.copy", *cp);
    dump_v3("rs.copy.opCall", (*cp)(0.3, 0.7));
    cb->p11 = Vector3(9.0, 9.0, 9.0);
    dump_v3("rs.copy.p11.afterMutate", cp->p11);
    dump_v3("rs.orig.p11.afterMutate", cb->p11);

    // --- group 5: guards ---------------------------------------------------------
    for (const std::string& bad : {std::string("q"), std::string(""), std::string("R"),
                                   std::string("S"), std::string(" i"), std::string("ii")}) {
        const std::string k = "rs.ctorGuard." + bad;
        guard_row(
            [&]() {
                RuledSurface rs(west, east, bad, false);
                emit(k + ".noThrow str=" + rs.toString());
            },
            k + ".thrown");
    }
    auto tam = std::make_shared<RuledSurface>(west, east, "s", false);
    tam->ruled_direction = "q";
    emit("rs.tampered.str=" + tam->toString());
    guard_row([&]() { dump_v3("rs.tampered.opCall", (*tam)(0.5, 0.5)); }, "rs.tampered.thrown");
    guard_row([&]() { dump_area("rs.tampered.area", *tam, 10, 10); },
              "rs.tampered.area.thrown");
    guard_row(
        [&]() {
            auto dd = std::dynamic_pointer_cast<RuledSurface>(tam->dup());
            emit("rs.tampered.dup.noThrow str=" + dd->toString());
        },
        "rs.tampered.dup.thrown");
    auto tc = std::make_shared<RuledSurface>(*tam);
    emit("rs.tampered.copy.str=" + tc->toString());
    dump_v3("rs.tampered.copy.p00", tc->p00);
    guard_row([&]() { dump_v3("rs.tampered.copy.opCall", (*tc)(0.25, 0.75)); },
              "rs.tampered.copy.thrown");
    // D gives the throw both handlers, `catch (Error)` first, and reaches the Exception
    // arm — which proves Exception and Error are siblings here.
    try {
        RuledSurface bad2(west, east, "q", false);
        emit("rs.errorHandler.noThrow");
    } catch (const eil::ErrorBase& e) {
        emit("rs.errorHandler.caught type=" + d_type_name(e));
    } catch (const eil::ExceptionBase& e) {
        emit("rs.errorHandler.fellThroughToException type=" + d_type_name(e) + " msg=" +
             esc(e.msg()));
    }

    // Replay.
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
            std::printf("line %zu\n  D:   |%s|\n  C++: |%s|\n", i + 1, e.c_str(), g.c_str());
            ++shown;
        }
        ++diffs;
    }
    // Every row is a byte comparison: no tolerance band exists in this test, so the
    // only residual reported is the count of differing rows.
    std::printf("%s: geom_surface_ruledsurface, probe replay %zu D rows, %zu C++ rows, %d differ; "
                "%zu of %zu rows byte-identical\n",
                diffs == 0 ? "PASS" : "FAIL", expected.size(), g_lines.size(), diffs,
                std::min(expected.size(), g_lines.size()) - static_cast<size_t>(diffs),
                std::min(expected.size(), g_lines.size()));
    return diffs == 0 ? 0 : 1;
}
