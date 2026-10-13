// tests/test_geom_surface_cubepatch.cpp — C++ mirror of
// probes/geom/surface/probe_cubepatch.d, replayed line for line against
// baselines/e587b235/probes/geom_surface_cubepatch.txt (1604 D rows), plus the
// `unittest` block of geom/surface/cubepatch.d (D:118-152) ported assertion for
// assertion, in source order (9 asserts: D:129, 136, 137, 138, 139, 144, 145, 150, 151).
//
// Row groups and what each one can catch:
//   cubp.f.<face>.*        all six canonical faces (in D's valid_names order: north, east,
//                          south, west, bottom, top) swept over
//                          r,s ∈ {0.0, 0.25, 0.5, 0.75, 1.0} — both parameter boundaries
//                          and a point in each quadrant — plus area() at (10,10) and (3,7).
//                          This is the D:63-71 face mapping; a swapped axis changes 25 rows.
//   cubp.big*, cubp.frac   a = 2.5 with centre (0.5, -1.25, 3) and a = 1/3, pinning
//                          D:104's per-component scale-then-add and to!string(a)'s %.6g
//                          form ("a=0.333333").
//   cubp.big.dup / .copy   D:54-57 vs D:46-52.
//   cubp.part.<face>.<part>
//                          13 which_part values × 3 faces, including the two the D comment
//                          at D:75-77 calls inconsistent ("south-north", "bottom-top"),
//                          the order-sensitive ones ("south-top" vs "top-south"), the
//                          cross-group ones ("west-east", "east" on an east face) and the
//                          no-op "xyz". The empty part is tagged "empty".
//   cubp.part2.<face>.<part>
//                          the other three faces at the corners a part selects.
//   cubp.badname.*         the ten rejected face names: the type is Exception (all ten
//                          rows), and the text keeps D's lack of separator, so the empty
//                          name yields "Invalid face name: " with a trailing space.
//                          "east-west" and "East" are rejected because canFind over the
//                          ARRAY of six names is exact equality.
//   cubp.goodname.*        all six valid names construct with which_part set, which is the
//                          measurement that the part switch's default (D:100-101) cannot
//                          be reached from a validated patch.
//   cubp.mutated*, cubp.copy.invalid
//                          face_name is public, so opCall's default (D:70) is reachable:
//                          "Invalid face: sideways". dup() re-validates (its message is the
//                          constructor's "Invalid face name: …"), the copy constructor does
//                          not.
//   cubp.unittest.*        the values and the two writeln lines of D's own unittest block.
//   cubp.nan.*, cubp.zero.a
//                          Vector3() corners (NaN, wave-02-brief section 5), a = number.init
//                          (prints "a=nan", positive NaN) and a = 0.
//
// Byte-for-byte comparison, no tolerance band.
//
// Regenerate (block comment so the shell line-splices do not trip -Wcomment):
/*
  ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface \
    probes/geom/surface/probe_cubepatch.d -of=/tmp/probe_cbp -od/tmp/probe_obj_w2_surfA
  /tmp/probe_cbp > baselines/e587b235/probes/geom_surface_cubepatch.txt
*/
//
// Run: ./test_geom_surface_cubepatch <baseline.txt>
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
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/math.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/surface/cubepatch.hpp"
#include "geom/surface/parametricsurface.hpp"

namespace {

using eil::geom::CubePatch;
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

void dump_patch(const std::string& tag, const CubePatch& pp) {
    emitf("%s str=%s", tag, pp.toString());
    emitf("%s classname=%s", tag, pp.classString());
    emitf("%s a=%.17g centre x=%.17g y=%.17g z=%.17g face=%s which_part=%s which_part.len=%d", tag,
          eil::re(pp.a), eil::re(pp.centre.x), eil::re(pp.centre.y), eil::re(pp.centre.z),
          pp.face_name, pp.which_part, static_cast<int>(pp.which_part.length()));
}

void dump_sweep(const std::string& tag, const ParametricSurface& ps) {
    for (double r : {0.0, 0.25, 0.5, 0.75, 1.0}) {
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

void dump_point(const std::string& tag, const std::string& name, const Vector3& v) {
    emitf("%s %s x=%.17g y=%.17g z=%.17g", tag, name, eil::re(v.x), eil::re(v.y), eil::re(v.z));
}

const std::vector<std::string> kFaces{"north", "east", "south", "west", "bottom", "top"};

// ============================================================================
// A. cubepatch.d's unittest (D:118-152), assertion for assertion.
// ============================================================================
void port_d_unittest() {
    number a = 1.0;
    number zero = 0.0;
    auto east_patch = std::make_shared<CubePatch>(a, Vector3(0.0, 0.0, 0.0), "east");
    auto mid_e = (*east_patch)(0.5, 0.5);
    auto p5_east = (*east_patch)(0.0, 1.0);
    auto p6_east = (*east_patch)(1.0, 1.0);
    // D:129 assert(approxEqualVectors(mid_e, Vector3(a, zero, zero)));
    check(eil::geom::approxEqualVectors(mid_e, Vector3(a, zero, zero)),
          "cubepatch.d:129 approxEqualVectors(mid_e, Vector3(a, zero, zero))");

    auto top_patch = std::make_shared<CubePatch>(a, Vector3(0.0, 0.0, 0.0), "top");
    auto mid_t = (*top_patch)(0.5, 0.5);
    auto p5_top = (*top_patch)(1.0, 0.0);
    auto p6_top = (*top_patch)(1.0, 1.0);
    // D:136 assert(approxEqualVectors(mid_t, Vector3(zero, zero, a)));
    check(eil::geom::approxEqualVectors(mid_t, Vector3(zero, zero, a)),
          "cubepatch.d:136 approxEqualVectors(mid_t, Vector3(zero, zero, a))");
    // D:137 assert(approxEqualVectors(p5_top, p5_east));
    check(eil::geom::approxEqualVectors(p5_top, p5_east),
          "cubepatch.d:137 approxEqualVectors(p5_top, p5_east)");
    // D:138 assert(approxEqualVectors(p6_top, p6_east));
    check(eil::geom::approxEqualVectors(p6_top, p6_east),
          "cubepatch.d:138 approxEqualVectors(p6_top, p6_east)");
    // D:139 assert(isClose(abs(p6_top), sqrt(3 * a)));
    check(eil::isClose(eil::geom::abs(p6_top), eil::sqrt(3 * a)),
          "cubepatch.d:139 isClose(abs(p6_top), sqrt(3 * a))");

    auto south_patch = std::make_shared<CubePatch>(a, Vector3(0.0, 0.0, 0.0), "south", "top");
    auto mid_s = (*south_patch)(0.5, 0.0);
    auto p5_south = (*south_patch)(1.0, 1.0);
    // D:144 assert(approxEqualVectors(mid_s, Vector3(zero, -a, zero)));
    check(eil::geom::approxEqualVectors(mid_s, Vector3(zero, -a, zero)),
          "cubepatch.d:144 approxEqualVectors(mid_s, Vector3(zero, -a, zero))");
    // D:145 assert(approxEqualVectors(p5_south, p5_east));
    check(eil::geom::approxEqualVectors(p5_south, p5_east),
          "cubepatch.d:145 approxEqualVectors(p5_south, p5_east)");

    auto north_patch =
        std::make_shared<CubePatch>(a, Vector3(0.0, 0.0, 0.0), "north", "top-east");
    auto mid_n = (*north_patch)(0.0, 0.0);
    auto p6_north = (*north_patch)(1.0, 1.0);
    // D:150 assert(approxEqualVectors(mid_n, Vector3(zero, a, zero)));
    check(eil::geom::approxEqualVectors(mid_n, Vector3(zero, a, zero)),
          "cubepatch.d:150 approxEqualVectors(mid_n, Vector3(zero, a, zero))");
    // D:151 assert(approxEqualVectors(p6_north, p6_top));
    check(eil::geom::approxEqualVectors(p6_north, p6_top),
          "cubepatch.d:151 approxEqualVectors(p6_north, p6_top)");
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]);
        return 2;
    }

    port_d_unittest();

    //--- group A: the six canonical faces, a=1 at the origin -------------------
    for (const std::string& face : kFaces) {
        auto pp = std::make_shared<CubePatch>(1.0, Vector3(0.0, 0.0, 0.0), face);
        std::string tag = eil::format("cubp.f.%s", face);
        dump_patch(tag, *pp);
        dump_sweep(tag, *pp);
        dump_area(tag, *pp, 10, 10);
        dump_area(tag, *pp, 3, 7);
    }

    //--- group B: a non-unit cube, off-origin centre ---------------------------
    auto big = std::make_shared<CubePatch>(2.5, Vector3(0.5, -1.25, 3.0), "east");
    dump_patch("cubp.big", *big);
    dump_sweep("cubp.big", *big);
    dump_area("cubp.big", *big, 10, 10);
    dump_area("cubp.big", *big, 3, 7);
    auto westbig = std::make_shared<CubePatch>(2.5, Vector3(0.5, -1.25, 3.0), "west");
    dump_patch("cubp.big.west", *westbig);
    dump_sweep("cubp.big.west", *westbig);
    dump_area("cubp.big.west", *big, 10, 10);

    auto frac = std::make_shared<CubePatch>(1.0 / 3.0, Vector3(0.0, 0.0, 0.0), "top");
    dump_patch("cubp.frac", *frac);
    dump_sweep("cubp.frac", *frac);
    dump_area("cubp.frac", *frac, 10, 10);

    //--- group C: dup() and the copy constructor -------------------------------
    auto d = std::dynamic_pointer_cast<CubePatch>(big->dup());
    dump_patch("cubp.big.dup", *d);
    dump_sweep("cubp.big.dup", *d);
    dump_area("cubp.big.dup", *d, 10, 10);
    auto cpy = std::make_shared<CubePatch>(*big);
    dump_patch("cubp.big.copy", *cpy);
    dump_sweep("cubp.big.copy", *cpy);
    dump_area("cubp.big.copy", *d, 3, 7);

    //--- group D: which_part halves, quarters and nonsense --------------------
    const std::vector<std::string> parts{"",        "south",  "north",  "bottom", "top",
                                         "south-top", "north-bottom", "west-east",
                                         "south-north", "bottom-top", "xyz",    "east",
                                         "top-south"};
    const std::vector<std::string> part_faces_a{"east", "south", "bottom"};
    for (const std::string& part : parts) {
        for (const std::string& face : part_faces_a) {
            std::string tag =
                eil::format("cubp.part.%s.%s", face, part.empty() ? std::string("empty") : part);
            auto pp = std::make_shared<CubePatch>(1.0, Vector3(0.0, 0.0, 0.0), face, part);
            dump_patch(tag, *pp);
            dump_sweep(tag, *pp);
        }
    }
    const std::vector<std::string> part_faces_b{"west", "north", "top"};
    const std::vector<std::string> parts_b{"south-top", "north-bottom", "west", "east"};
    for (const std::string& face : part_faces_b) {
        for (const std::string& part : parts_b) {
            std::string tag = eil::format("cubp.part2.%s.%s", face, part);
            auto pp = std::make_shared<CubePatch>(1.0, Vector3(0.0, 0.0, 0.0), face, part);
            dump_patch(tag, *pp);
            auto c00 = (*pp)(0.0, 0.0);
            emitf("%s corner r=0 s=0 x=%.17g y=%.17g z=%.17g", tag, eil::re(c00.x), eil::re(c00.y),
                  eil::re(c00.z));
            auto c11 = (*pp)(1.0, 1.0);
            emitf("%s corner r=1 s=1 x=%.17g y=%.17g z=%.17g", tag, eil::re(c11.x), eil::re(c11.y),
                  eil::re(c11.z));
        }
    }

    //--- group E: the constructor's face-name guard ----------------------------
    const std::vector<std::string> bad{"",        " ",     "East",   "EAST",       "east-west",
                                       "east ",   " east", "n",      "north-south", "side"};
    for (const std::string& b : bad) {
        std::string tag =
            eil::format("cubp.badname.%s", b.empty() ? std::string("empty") : b);
        dump_throw(tag, [&b]() { CubePatch(1.0, Vector3(0.0, 0.0, 0.0), b); });
    }
    for (const std::string& g : kFaces) {
        dump_throw(eil::format("cubp.goodname.%s", g),
                   [&g]() { CubePatch(1.0, Vector3(0.0, 0.0, 0.0), g, "top"); });
    }

    //--- group F: opCall's default branch via the public field -----------------
    auto mut = std::make_shared<CubePatch>(1.0, Vector3(0.0, 0.0, 0.0), "east");
    mut->face_name = "sideways";
    dump_patch("cubp.mutated", *mut);
    dump_throw("cubp.mutated.opCall", [&mut]() { (*mut)(0.5, 0.5); });
    dump_throw("cubp.mutated.area", [&mut]() { mut->area(); });
    dump_throw("cubp.mutated.dup", [&mut]() { mut->dup(); });
    auto mut2 = std::make_shared<CubePatch>(1.0, Vector3(0.0, 0.0, 0.0), "top", "south");
    mut2->face_name = "bogus";
    dump_throw("cubp.mutated2.opCall", [&mut2]() { (*mut2)(0.5, 0.5); });
    auto mut3 = std::make_shared<CubePatch>(*mut2);
    dump_patch("cubp.copy.invalid", *mut3);
    dump_throw("cubp.copy.invalid.opCall", [&mut3]() { (*mut3)(0.5, 0.5); });

    //--- group G: the D unittest's own observable values ----------------------
    {
        number a = 1.0;
        auto east_patch = std::make_shared<CubePatch>(a, Vector3(0.0, 0.0, 0.0), "east");
        auto mid_e = (*east_patch)(0.5, 0.5);
        auto p5_east = (*east_patch)(0.0, 1.0);
        auto p6_east = (*east_patch)(1.0, 1.0);
        emitf("cubp.unittest mid_e_line=%s", "mid_e=" + mid_e.toString());
        dump_point("cubp.unittest", "mid_e", mid_e);
        dump_point("cubp.unittest", "p5_east", p5_east);
        dump_point("cubp.unittest", "p6_east", p6_east);

        auto top_patch = std::make_shared<CubePatch>(a, Vector3(0.0, 0.0, 0.0), "top");
        auto mid_t = (*top_patch)(0.5, 0.5);
        auto p5_top = (*top_patch)(1.0, 0.0);
        auto p6_top = (*top_patch)(1.0, 1.0);
        emitf("cubp.unittest mid_t_line=%s", "mid_t=" + mid_t.toString());
        dump_point("cubp.unittest", "mid_t", mid_t);
        dump_point("cubp.unittest", "p5_top", p5_top);
        dump_point("cubp.unittest", "p6_top", p6_top);

        number m = eil::geom::abs(p6_top);
        number s = eil::sqrt(3 * a);
        emitf("cubp.unittest abs_p6_top=%.17g sqrt3a=%.17g isClose=%s", eil::re(m), eil::re(s),
              eil::isClose(m, s) ? "true" : "false");

        auto south_patch = std::make_shared<CubePatch>(a, Vector3(0.0, 0.0, 0.0), "south", "top");
        auto mid_s = (*south_patch)(0.5, 0.0);
        auto p5_south = (*south_patch)(1.0, 1.0);
        dump_point("cubp.unittest", "mid_s", mid_s);
        dump_point("cubp.unittest", "p5_south", p5_south);

        auto north_patch = std::make_shared<CubePatch>(a, Vector3(0.0, 0.0, 0.0), "north",
                                                       "top-east");
        auto mid_n = (*north_patch)(0.0, 0.0);
        auto p6_north = (*north_patch)(1.0, 1.0);
        dump_point("cubp.unittest", "mid_n", mid_n);
        dump_point("cubp.unittest", "p6_north", p6_north);
    }

    //--- group H: NaN and extreme parameters ----------------------------------
    auto nanp = std::make_shared<CubePatch>(1.0, Vector3(), "east");
    dump_patch("cubp.nan.centre", *nanp);
    dump_sweep("cubp.nan.centre", *nanp);
    dump_area("cubp.nan.centre", *nanp, 10, 10);
    auto nanA = std::make_shared<CubePatch>(eil::d_init<number>(), Vector3(0.0, 0.0, 0.0), "top");
    dump_patch("cubp.nan.a", *nanA);
    dump_sweep("cubp.nan.a", *nanA);
    auto zeroA = std::make_shared<CubePatch>(0.0, Vector3(0.0, 0.0, 0.0), "bottom");
    dump_patch("cubp.zero.a", *zeroA);
    dump_sweep("cubp.zero.a", *zeroA);
    dump_area("cubp.zero.a", *zeroA, 10, 10);

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
        "%s: geom_surface_cubepatch, %d/%d unittest assertions hold (cubepatch.d:118-152); "
        "probe replay %zu D lines, %zu C++ lines, %d differ; %zu of %zu rows byte-identical, "
        "0 rows accepted through a tolerance band\n",
        (diffs == 0 && g_failures == 0) ? "PASS" : "FAIL", g_checks - g_failures, g_checks,
        expected.size(), g_lines.size(), diffs, common - static_cast<size_t>(diffs), common);
    return (diffs == 0 && g_failures == 0) ? 0 : 1;
}
