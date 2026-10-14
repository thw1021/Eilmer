// tests/test_geom_surface_gmopatch.cpp — C++ mirror of
// probes/geom/surface/probe_gmopatch.d, replayed line for line against
// baselines/e587b235/probes/geom_surface_gmopatch.txt (497 D rows), plus the `unittest`
// block of geom/surface/gmopatch.d (D:150-171, non-complex branch) ported assertion for
// assertion.
//
// The D file this corpus was captured against is geom/surface/gmopatch.d, 171 lines,
// sha256 ba95d67bf561b72e5efd63eeb3fe3ca56ddd7e4a67d93a472d8eb5e134e8758f — the same value
// the corpus states in its own first row (emitted below as data, so a replay against a
// corpus from another revision fails on line 1 rather than silently comparing against
// something else).
//
// gmopatch.d's unittest is NOT compiled with -unittest: the probe replays its statements
// in source order (rows gmo.ut.1 / gmo.ut.2), and so does this file. Its two `writeln("c=",
// c)` lines are carried as data with their exact bytes, and its two asserts are real checks
// here (the g_checks/g_failures pair in the summary line):
//   D:166 assert(approxEqualVectors(c, Vector3(0.477, 0.646, 3.0), 1.0e-1));
//   D:169 assert(approxEqualVectors(c, Vector3(0.117, 0.235, 3.0), 1.0e-1));
// The `version (complex_numbers)` branch (D:153-157, a skip() call) is not compiled, per
// backlog T-904, and D's own comment there says the GMO patch is incompatible with complex
// numbers — which is why the non-complex branch is the reference.
//
// Row groups and what each one can catch:
//   gmo.ut.*        the unittest patch: 11x11 test grid, the full member dump including the
//                   optimised r_grid/s_grid, a 5x5 (r,s) sweep and two area() splits. The
//                   redistributed parameters are the object's whole state, so 32 numbers at
//                   %.17g plus the %.6g toString() rendering of the same arrays are both
//                   carried (a wrong precision shows up in one and not the other).
//   gmo.def.*       the default arguments niv=11, njv=11 (D:36, D:46).
//   gmo.size55/33/22/2113
//                   the test-grid size is an INPUT to the optimiser, so all four leave
//                   different parameters. A port that optimises once, caches, or ignores
//                   niv/njv cannot reproduce these 4 groups.
//   gmo.skew.*      a non-planar sheared corner set (9x7), so area() has all three
//                   components non-zero.
//   gmo.bezier.*    the four-Path constructor with curved boundaries. The patch is dumped
//                   again after the caller's Bezier is mutated under foot (D:52-55 dups
//                   every boundary), and the caller's own string is printed to show which
//                   side moved.
//   gmo.lines.*     the same quad handed over explicitly: the three equality rows say the
//                   corner constructor (D:35-43) and the four-Path constructor agree, which
//                   is what pins the parameter order south, north, west, east (D:45-46) and
//                   the Line pairing of D:38-41.
//   gmo.open.*      the four throw guards in check order, one corner open at a time, plus
//                   all four open — which still names p00. Each is a bare `throw new
//                   Error(text(...))`, so the rows state file=[] line=0 and this port
//                   reports its OWN eil::ErrorBase site there: attaching __FILE__/__LINE__
//                   would fail the row (T-922). gmo.open.p00.corners is the corner
//                   constructor with the same geometry, and it does NOT throw — by
//                   construction its four Lines share every corner exactly.
//   gmo.tol.*       approxEqualVectors' default tolerances (maxRelDiff 1.0e-2, maxAbsDiff
//                   1.0e-5): 1.0e-6 passes, 1.0e-3 / 5.0e-3 / 1.5e-2 all fail, because at a
//                   corner of magnitude 0 the relative term has nothing to work on.
//   gmo.nan.corners NaN != NaN, so the guard fires and the message prints
//                   Vector3(nan, nan, nan) twice.
//   gmo.copy.* / gmo.indep.*
//                   D:91-103: no checks, no re-optimisation, both grids copied BY VALUE and
//                   the boundaries duped. The source's r_grid[1][2] and p11 are then
//                   overwritten and the copy re-read, so a shared-array copy would show.
//   gmo.dup.*       D:105-108 is NOT a copy: dup() re-enters the checking constructor with
//                   the DEFAULT 11x11 grid, so gmo.dup55 grids_eq_ut=true and
//                   grids_eq_src55=false.
//   gmo.oor.* / gmo.nf*
//                   opCall is a polynomial, so it extrapolates outside [0,1]; with NaN and
//                   +-Infinity arguments the SSE default QNaN shows through as -nan, which
//                   is why the corpus carries the sign and the replay prints it.
//   gmo.bad.*       the sizes the construction path cannot serve, reached through
//                   D:87-88's StructuredGrid and determine_rs_grids: njv<=0 or njv==1 die in
//                   measure_of_badness with a bare object.Error "Not implemented."
//                   (sgrid.d:1391), and niv<=1 with core.exception.AssertError "index i is
//                   invalid" at grid.d:73. The message is the assert's own text, not
//                   "Assertion failure" (T-918), and the class is the Error family.
//   gmo.point.*     four coincident corners: the optimiser still runs (on a NaN objective)
//                   and still leaves particular numbers. Captured rather than reasoned out.
//
// Byte-for-byte comparison, no tolerance band, no skipped row.
//
// Regenerate:
/*
  ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface \
    probes/geom/surface/probe_gmopatch.d -of=/tmp/probe_g6b_gmo -od/tmp/obj_g6b
  /tmp/probe_g6b_gmo > baselines/e587b235/probes/geom_surface_gmopatch.txt
*/
//
// Run: ./test_geom_surface_gmopatch <baseline.txt>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "eil/eil.hpp"
#include "eil/array.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/bezier.hpp"
#include "geom/gpath/line.hpp"
#include "geom/gpath/path.hpp"
#include "geom/grid/sgrid.hpp"
#include "geom/misc/univariatefunctions.hpp"
#include "geom/surface/coonspatch.hpp"
#include "geom/surface/gmopatch.hpp"
#include "geom/surface/parametricsurface.hpp"

namespace {

using eil::geom::CoonsPatch;
using eil::geom::GMOPatch;
using eil::geom::ParametricSurface;
using eil::geom::Path;
using eil::geom::PathPtr;
using eil::geom::RSGrid;
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

// Where D raised. An AssertError/ArrayIndexError inside the reference reports a path in
// the ldc2 shadow tree, which no C++ port can name, so it is transcribed as data — the
// convention tests/test_geom_grid_usgrid.cpp:33-46 documents for the grid corpora. What
// this gate grades is the class and the message. A bare object.Error is different: D
// reports NO site there, and the port's own (empty) site is printed for those rows.
const std::string GRID_PATH = "/tmp/gdtk_shadow/geom/grid/grid.d";

void dump_throw(const std::string& tag, const std::function<void()>& fn,
                const std::string& dfile, long dline) {
    try {
        fn();
        emitf("%s|class=none|msg=[]|file=[]|line=-1", tag);
    } catch (const eil::RangeError& e) {
        emitf("%s|class=core.exception.ArrayIndexError|msg=[%s]|file=[%s]|line=%d", tag, e.msg(),
              dfile, dline);
    } catch (const eil::ErrorBase& e) {
        // Which Error the port raised is decided by the site, exactly as
        // substrate/include/eil/error.hpp:34-49 measured it: an assert carries the raise
        // site (grid.cpp:93 goes through d_assert_msg), a bare `new Error` carries none,
        // so a port that attached __FILE__ to one of the four corner guards would print
        // the wrong CLASS here and fail the row (T-918, T-922).
        const bool has_site = std::string(e.file()).size() > 0;
        emitf("%s|class=%s|msg=[%s]|file=[%s]|line=%d", tag,
              has_site ? "core.exception.AssertError" : "object.Error", e.msg(),
              has_site ? dfile.c_str() : e.file(), has_site ? dline : static_cast<long>(e.line()));
    } catch (const eil::ExceptionBase& e) {
        emitf("%s|class=object.Exception|msg=[%s]|file=[%s]|line=%d", tag, e.msg(), dfile, dline);
    }
}

void dump_patch(const std::string& tag, const GMOPatch& gp) {
    emitf("%s str=%s", tag, gp.toString());
    emitf("%s classname=%s", tag, gp.classString());
    const std::pair<const char*, Vector3> cs[4] = {
        {"p00", gp.p00}, {"p10", gp.p10}, {"p11", gp.p11}, {"p01", gp.p01}};
    for (const auto& c : cs) {
        emitf("%s %s x=%.17g y=%.17g z=%.17g", tag, c.first, eil::re(c.second.x),
              eil::re(c.second.y), eil::re(c.second.z));
    }
    const std::pair<const char*, PathPtr> bs[4] = {
        {"south", gp.south}, {"north", gp.north}, {"west", gp.west}, {"east", gp.east}};
    for (const auto& b : bs) {
        emitf("%s %s class=%s str=%s", tag, b.first, b.second->classString(),
              b.second->toString());
        for (double t : {0.0, 0.5, 1.0}) {
            auto p = (*(b.second))(t);
            emitf("%s %s t=%.17g x=%.17g y=%.17g z=%.17g", tag, b.first, t, eil::re(p.x),
                  eil::re(p.y), eil::re(p.z));
        }
    }
    for (size_t i = 0; i < 4; ++i) {
        emitf("%s r_grid[%d] a=%.17g b=%.17g c=%.17g d=%.17g", tag, i, gp.r_grid[i][0],
              gp.r_grid[i][1], gp.r_grid[i][2], gp.r_grid[i][3]);
    }
    for (size_t i = 0; i < 4; ++i) {
        emitf("%s s_grid[%d] a=%.17g b=%.17g c=%.17g d=%.17g", tag, i, gp.s_grid[i][0],
              gp.s_grid[i][1], gp.s_grid[i][2], gp.s_grid[i][3]);
    }
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

// The probe's dump_probe: four (r,s) points for the groups whose observable is the grids.
void dump_probe(const std::string& tag, const ParametricSurface& ps) {
    const double q[4][2] = {{0.5, 0.5}, {0.1, 0.1}, {0.25, 0.75}, {0.9, 0.2}};
    for (const auto& p : q) {
        auto v = ps(p[0], p[1]);
        emitf("%s opCall r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g", tag, p[0], p[1], eil::re(v.x),
              eil::re(v.y), eil::re(v.z));
    }
}

void dump_area(const std::string& tag, ParametricSurface& ps, int nr, int ns) {
    auto a = ps.area(nr, ns);
    emitf("%s area nr=%d ns=%d x=%.17g y=%.17g z=%.17g", tag, nr, ns, eil::re(a.x), eil::re(a.y),
          eil::re(a.z));
}

// D compared printed bits, so the port does the same: two surfaces are "the same" when all
// 25 sweep points render identically at %.17g (which tells -nan from nan and a signed zero
// from zero, and nothing else).
std::string pts_eq(const std::string& tag, const ParametricSurface& a,
                   const ParametricSurface& b) {
    bool same = true;
    for (double r : {0.0, 0.2, 0.5, 0.75, 1.0}) {
        for (double s : {0.0, 0.25, 0.5, 0.75, 1.0}) {
            auto pa = a(r, s);
            auto pb = b(r, s);
            if (eil::format("%.17g %.17g %.17g", eil::re(pa.x), eil::re(pa.y), eil::re(pa.z)) !=
                eil::format("%.17g %.17g %.17g", eil::re(pb.x), eil::re(pb.y), eil::re(pb.z))) {
                same = false;
            }
        }
    }
    return eil::format("%s same=%s", tag, same);
}

bool grids_eq(const GMOPatch& a, const GMOPatch& b) {
    for (size_t i = 0; i < 4; ++i) {
        for (size_t j = 0; j < 4; ++j) {
            if (eil::format("%.17g", eil::re(a.r_grid[i][j])) !=
                eil::format("%.17g", eil::re(b.r_grid[i][j]))) {
                return false;
            }
            if (eil::format("%.17g", eil::re(a.s_grid[i][j])) !=
                eil::format("%.17g", eil::re(b.s_grid[i][j]))) {
                return false;
            }
        }
    }
    return true;
}

eil::Array<Vector3> pts(std::initializer_list<Vector3> v) {
    eil::Array<Vector3> a;
    for (const Vector3& p : v) a.append(p);
    return a;
}

// ------------------------------------------------------------------
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]);
        return 2;
    }

    //--- group 0: provenance --------------------------------------------------
    emit("gmo.src|dfile=geom/surface/gmopatch.d|dlines=171|sha256=ba95d67bf561b72e5efd63eeb3"
         "fe3ca56ddd7e4a67d93a472d8eb5e134e8758f|probe=probes/geom/surface/probe_gmopatch.d"
         "|cc=ldc2 1.40.0 -O0 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip"
         "|unittest=replayed-not-compiled");

    //--- group A: gmopatch.d's own unittest (D:150-171), in source order -------
    // D:159-162. The first corner is built from an array literal in D, which is the
    // Vector3(Slice<number>) constructor; the port spells the same thing the way
    // tests/test_geom_surface_coonspatch.cpp:186 does.
    Vector3 p00(eil::Array<number>{0.0, 0.1, 3.0}.all());
    Vector3 p10(1.0, 0.1, 3.0);
    Vector3 p11(1.0, 1.1, 3.0);
    Vector3 p01(0.0, 1.1, 3.0);
    auto ut = std::make_shared<GMOPatch>(p00, p10, p11, p01, 11, 11);
    Vector3 c1 = (*ut)(0.5, 0.5);
    emitf("gmo.ut.1 writeln=%s", std::string("c=") + eil::geom::d_tostr(c1));
    emitf("gmo.ut.1 full x=%.17g y=%.17g z=%.17g", eil::re(c1.x), eil::re(c1.y), eil::re(c1.z));
    // D:166 assert(approxEqualVectors(c, Vector3(0.477, 0.646, 3.0), 1.0e-1));
    bool a1 = eil::geom::approxEqualVectors(c1, Vector3(0.477, 0.646, 3.0), 1.0e-1);
    emitf("gmo.ut.1 assert_holds=%s", a1);
    check(a1, "gmopatch.d:166 approxEqualVectors(my_patch(0.5,0.5), Vector3(0.477,0.646,3.0), 1e-1)");
    Vector3 c2 = (*ut)(0.1, 0.1);
    emitf("gmo.ut.2 writeln=%s", std::string("c=") + eil::geom::d_tostr(c2));
    emitf("gmo.ut.2 full x=%.17g y=%.17g z=%.17g", eil::re(c2.x), eil::re(c2.y), eil::re(c2.z));
    // D:169 assert(approxEqualVectors(c, Vector3(0.117, 0.235, 3.0), 1.0e-1));
    bool a2 = eil::geom::approxEqualVectors(c2, Vector3(0.117, 0.235, 3.0), 1.0e-1);
    emitf("gmo.ut.2 assert_holds=%s", a2);
    check(a2, "gmopatch.d:169 approxEqualVectors(my_patch(0.1,0.1), Vector3(0.117,0.235,3.0), 1e-1)");
    dump_patch("gmo.ut", *ut);
    dump_sweep("gmo.ut", *ut);
    dump_area("gmo.ut", *ut, 10, 10);
    dump_area("gmo.ut", *ut, 3, 7);

    //--- group B: the default test-grid sizes (D:36, D:46 niv=11, njv=11) ------
    auto dflt = std::make_shared<GMOPatch>(p00, p10, p11, p01);
    emitf("gmo.def str_eq_ut=%s", dflt->toString() == ut->toString());
    emitf("gmo.def grids_eq_ut=%s", grids_eq(*dflt, *ut));
    emit(pts_eq("gmo.def", *dflt, *ut));

    //--- group C: the test-grid size is an optimiser input ---------------------
    auto s55 = std::make_shared<GMOPatch>(p00, p10, p11, p01, 5, 5);
    dump_patch("gmo.size55", *s55);
    dump_probe("gmo.size55", *s55);
    auto s33 = std::make_shared<GMOPatch>(p00, p10, p11, p01, 3, 3);
    dump_patch("gmo.size33", *s33);
    dump_probe("gmo.size33", *s33);
    auto s22 = std::make_shared<GMOPatch>(p00, p10, p11, p01, 2, 2);
    dump_patch("gmo.size22", *s22);
    dump_probe("gmo.size22", *s22);
    auto s2113 = std::make_shared<GMOPatch>(p00, p10, p11, p01, 21, 13);
    dump_patch("gmo.size2113", *s2113);
    dump_probe("gmo.size2113", *s2113);
    emitf("gmo.size grids_differ_55_vs_11=%s", !grids_eq(*s55, *ut));
    emitf("gmo.size grids_differ_33_vs_22=%s", !grids_eq(*s33, *s22));
    emitf("gmo.size grids_differ_2113_vs_11=%s", !grids_eq(*s2113, *ut));

    //--- group D: a non-planar, sheared corner set ----------------------------
    auto q00 = Vector3(0.5, -0.25, 0.1);
    auto q10 = Vector3(2.0, 0.75, -0.3);
    auto q11 = Vector3(1.25, 2.5, 0.05);
    auto q01 = Vector3(-0.5, 1.5, 0.9);
    auto sk = std::make_shared<GMOPatch>(q00, q10, q11, q01, 9, 7);
    dump_patch("gmo.skew", *sk);
    dump_sweep("gmo.skew", *sk);
    dump_area("gmo.skew", *sk, 10, 10);
    dump_area("gmo.skew", *sk, 3, 7);

    //--- group E: the four-Path constructor with curved boundaries -------------
    PathPtr bs = std::make_shared<eil::geom::Bezier>(
        pts({Vector3(0.0, 0.0, 0.0), Vector3(0.33, -0.2, 0.1), Vector3(0.67, -0.2, 0.1),
             Vector3(1.0, 0.0, 0.0)}));
    PathPtr bn = std::make_shared<eil::geom::Bezier>(
        pts({Vector3(0.0, 1.0, 0.0), Vector3(0.33, 1.2, 0.1), Vector3(0.67, 1.2, 0.1),
             Vector3(1.0, 1.0, 0.0)}));
    PathPtr bw = std::make_shared<eil::geom::Bezier>(
        pts({Vector3(0.0, 0.0, 0.0), Vector3(-0.15, 0.33, 0.1), Vector3(-0.15, 0.67, 0.1),
             Vector3(0.0, 1.0, 0.0)}));
    PathPtr be = std::make_shared<eil::geom::Bezier>(
        pts({Vector3(1.0, 0.0, 0.0), Vector3(1.15, 0.33, 0.1), Vector3(1.15, 0.67, 0.1),
             Vector3(1.0, 1.0, 0.0)}));
    auto bz = std::make_shared<GMOPatch>(bs, bn, bw, be, 5, 5);
    dump_patch("gmo.bezier", *bz);
    dump_sweep("gmo.bezier", *bz);
    dump_area("gmo.bezier", *bz, 10, 10);
    std::dynamic_pointer_cast<eil::geom::Bezier>(bs)->B[1] = Vector3(0.33, -0.2, 5.0);
    dump_patch("gmo.bezier.aftermut", *bz);
    emitf("gmo.bezier.callerstr str=%s", bs->toString());

    //--- group F: the same boundaries handed over explicitly -------------------
    PathPtr ls = std::make_shared<eil::geom::Line>(p00, p10);
    PathPtr ln = std::make_shared<eil::geom::Line>(p01, p11);
    PathPtr lw = std::make_shared<eil::geom::Line>(p00, p01);
    PathPtr le = std::make_shared<eil::geom::Line>(p10, p11);
    auto lp = std::make_shared<GMOPatch>(ls, ln, lw, le, 11, 11);
    emitf("gmo.lines str_eq_ut=%s", lp->toString() == ut->toString());
    emitf("gmo.lines grids_eq_ut=%s", grids_eq(*lp, *ut));
    emit(pts_eq("gmo.lines", *lp, *ut));
    dump_patch("gmo.lines", *lp);

    //--- group G: the four open-corner guards, in D's check order --------------
    {
        PathPtr s2 = std::make_shared<eil::geom::Line>(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
        PathPtr n2 = std::make_shared<eil::geom::Line>(Vector3(0.0, 1.0, 0.0), Vector3(1.0, 1.0, 0.0));
        PathPtr w2 = std::make_shared<eil::geom::Line>(Vector3(0.1, 0.0, 0.0), Vector3(0.1, 1.0, 0.0));
        PathPtr e2 = std::make_shared<eil::geom::Line>(Vector3(1.0, 0.0, 0.0), Vector3(1.0, 1.0, 0.0));
        dump_throw("gmo.open.p00", [&]() { GMOPatch(s2, n2, w2, e2); }, "", 0);
        dump_throw("gmo.open.p00.corners",
                   [&]() {
                       GMOPatch(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0),
                                Vector3(1.0, 1.0, 0.0), Vector3(0.1, 1.0, 0.0));
                   },
                   "", 0);
    }
    {
        PathPtr s2 = std::make_shared<eil::geom::Line>(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.1, 0.0));
        PathPtr n2 = std::make_shared<eil::geom::Line>(Vector3(0.0, 1.0, 0.0), Vector3(1.0, 1.0, 0.0));
        PathPtr w2 = std::make_shared<eil::geom::Line>(Vector3(0.0, 0.0, 0.0), Vector3(0.0, 1.0, 0.0));
        PathPtr e2 = std::make_shared<eil::geom::Line>(Vector3(1.0, 0.0, 0.0), Vector3(1.0, 1.0, 0.0));
        dump_throw("gmo.open.p10", [&]() { GMOPatch(s2, n2, w2, e2); }, "", 0);
    }
    {
        PathPtr s2 = std::make_shared<eil::geom::Line>(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
        PathPtr n2 = std::make_shared<eil::geom::Line>(Vector3(0.0, 1.0, 0.0), Vector3(1.0, 1.1, 0.0));
        PathPtr w2 = std::make_shared<eil::geom::Line>(Vector3(0.0, 0.0, 0.0), Vector3(0.0, 1.0, 0.0));
        PathPtr e2 = std::make_shared<eil::geom::Line>(Vector3(1.0, 0.0, 0.0), Vector3(1.0, 1.0, 0.0));
        dump_throw("gmo.open.p11", [&]() { GMOPatch(s2, n2, w2, e2); }, "", 0);
    }
    {
        PathPtr s2 = std::make_shared<eil::geom::Line>(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
        PathPtr n2 = std::make_shared<eil::geom::Line>(Vector3(0.1, 1.0, 0.0), Vector3(1.0, 1.0, 0.0));
        PathPtr w2 = std::make_shared<eil::geom::Line>(Vector3(0.0, 0.0, 0.0), Vector3(0.0, 1.0, 0.0));
        PathPtr e2 = std::make_shared<eil::geom::Line>(Vector3(1.0, 0.0, 0.0), Vector3(1.0, 1.0, 0.0));
        dump_throw("gmo.open.p01", [&]() { GMOPatch(s2, n2, w2, e2); }, "", 0);
    }
    {
        PathPtr s2 = std::make_shared<eil::geom::Line>(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.1, 0.0));
        PathPtr n2 = std::make_shared<eil::geom::Line>(Vector3(0.1, 1.1, 0.0), Vector3(1.0, 1.0, 0.0));
        PathPtr w2 = std::make_shared<eil::geom::Line>(Vector3(0.2, 0.0, 0.0), Vector3(0.0, 1.2, 0.0));
        PathPtr e2 = std::make_shared<eil::geom::Line>(Vector3(1.0, 0.3, 0.0), Vector3(0.9, 1.0, 0.0));
        dump_throw("gmo.open.all", [&]() { GMOPatch(s2, n2, w2, e2); }, "", 0);
    }
    {
        const double eps[4] = {1.0e-6, 1.0e-3, 5.0e-3, 1.5e-2};
        const char* names[4] = {"1e-6", "1e-3", "5e-3", "1.5e-2"};
        for (int e = 0; e < 4; ++e) {
            const double d = eps[e];
            PathPtr s2 = std::make_shared<eil::geom::Line>(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
            PathPtr n2 = std::make_shared<eil::geom::Line>(Vector3(0.0, 1.0, 0.0), Vector3(1.0, 1.0, 0.0));
            PathPtr w2 = std::make_shared<eil::geom::Line>(Vector3(d, 0.0, 0.0), Vector3(0.0, 1.0, 0.0));
            PathPtr e2 = std::make_shared<eil::geom::Line>(Vector3(1.0, 0.0, 0.0), Vector3(1.0, 1.0, 0.0));
            dump_throw(std::string("gmo.tol.") + names[e],
                       [&]() { GMOPatch(s2, n2, w2, e2); }, "", 0);
        }
    }

    //--- group H: default-constructed (NaN) corners ---------------------------
    dump_throw("gmo.nan.corners",
               [&]() { GMOPatch(Vector3(), Vector3(), Vector3(), Vector3()); }, "", 0);

    //--- group I: the copy constructor (D:91-103) -----------------------------
    auto cp = std::make_shared<GMOPatch>(*s55);
    dump_patch("gmo.copy", *cp);
    dump_probe("gmo.copy", *cp);
    emitf("gmo.copy grids_eq_source=%s", grids_eq(*cp, *s55));
    emitf("gmo.copy str_eq_source=%s", cp->toString() == s55->toString());
    emit(pts_eq("gmo.copy", *cp, *s55));
    s55->r_grid[1][2] = 0.25;
    s55->p11 = Vector3(7.0, 7.0, 7.0);
    emit("gmo.indep mutated=r_grid[1][2],p11");
    emitf("gmo.indep copy r_grid[1] a=%.17g b=%.17g c=%.17g d=%.17g", cp->r_grid[1][0],
          cp->r_grid[1][1], cp->r_grid[1][2], cp->r_grid[1][3]);
    emitf("gmo.indep copy p11 x=%.17g y=%.17g z=%.17g", eil::re(cp->p11.x), eil::re(cp->p11.y),
          eil::re(cp->p11.z));
    emitf("gmo.indep source r_grid[1] a=%.17g b=%.17g c=%.17g d=%.17g", s55->r_grid[1][0],
          s55->r_grid[1][1], s55->r_grid[1][2], s55->r_grid[1][3]);

    //--- group J: dup() (D:105-108) re-optimises at the DEFAULT sizes ----------
    auto dp = std::dynamic_pointer_cast<GMOPatch>(ut->dup());
    emitf("gmo.dup str_eq_ut=%s", dp->toString() == ut->toString());
    emitf("gmo.dup grids_eq_ut=%s", grids_eq(*dp, *ut));
    emit(pts_eq("gmo.dup", *dp, *ut));
    auto dd22 = std::dynamic_pointer_cast<GMOPatch>(s22->dup());
    auto src55 = std::make_shared<GMOPatch>(p00, p10, p11, p01, 5, 5);
    auto dd55 = std::dynamic_pointer_cast<GMOPatch>(src55->dup());
    emitf("gmo.dup55 str=%s", dd55->toString());
    emitf("gmo.dup55 grids_eq_ut=%s", grids_eq(*dd55, *ut));
    emitf("gmo.dup55 grids_eq_src55=%s", grids_eq(*dd55, *src55));
    emit(pts_eq("gmo.dup55", *dd55, *ut));
    emitf("gmo.dup22 grids_eq_ut=%s", grids_eq(*dd22, *ut));

    //--- group K: opCall outside the unit square and with non-finite arguments -
    const double oo[4][2] = {{-0.5, 1.5}, {2.0, 0.0}, {-1.0, -1.0}, {0.5, 1.0e10}};
    for (const auto& q : oo) {
        auto p = (*ut)(q[0], q[1]);
        emitf("gmo.oor r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g", q[0], q[1], eil::re(p.x),
              eil::re(p.y), eil::re(p.z));
    }
    const double nan_v = std::numeric_limits<double>::quiet_NaN();
    const double pinf = std::numeric_limits<double>::infinity();
    const double ninf = -std::numeric_limits<double>::infinity();
    const double nf[5][2] = {{nan_v, 0.5}, {0.5, nan_v}, {pinf, 0.5}, {ninf, 0.25}, {0.5, pinf}};
    for (int i = 0; i < 5; ++i) {
        const double r = nf[i][0];
        const double s = nf[i][1];
        dump_throw("gmo.nf" + std::to_string(i),
                   [&]() {
                       auto p = (*ut)(r, s);
                       emitf("gmo.nf%.17g r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g",
                             static_cast<double>(i), r, s, eil::re(p.x), eil::re(p.y),
                             eil::re(p.z));
                   },
                   "", 0);
    }

    //--- group L: sizes the construction path cannot serve ---------------------
    dump_throw("gmo.bad.njv1", [&]() { GMOPatch(p00, p10, p11, p01, 5, 1); }, "", 0);
    dump_throw("gmo.bad.njv0", [&]() { GMOPatch(p00, p10, p11, p01, 5, 0); }, "", 0);
    dump_throw("gmo.bad.niv1", [&]() { GMOPatch(p00, p10, p11, p01, 1, 5); }, GRID_PATH, 73);
    dump_throw("gmo.bad.niv0", [&]() { GMOPatch(p00, p10, p11, p01, 0, 5); }, GRID_PATH, 73);
    dump_throw("gmo.bad.niv1njv1", [&]() { GMOPatch(p00, p10, p11, p01, 1, 1); }, "", 0);

    //--- group M: four coincident corners --------------------------------------
    auto pt = std::make_shared<GMOPatch>(Vector3(1.0, 2.0, 3.0), Vector3(1.0, 2.0, 3.0),
                                        Vector3(1.0, 2.0, 3.0), Vector3(1.0, 2.0, 3.0), 5, 5);
    dump_patch("gmo.point", *pt);
    dump_probe("gmo.point", *pt);
    dump_area("gmo.point", *pt, 10, 10);

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
        "%s: geom_surface_gmopatch, %d/%d unittest assertions hold (gmopatch.d:150-171); "
        "probe replay %zu D lines, %zu C++ lines, %d differ; %zu of %zu rows byte-identical, "
        "0 rows accepted through a tolerance band\n",
        (diffs == 0 && g_failures == 0) ? "PASS" : "FAIL", g_checks - g_failures, g_checks,
        expected.size(), g_lines.size(), diffs, common - static_cast<size_t>(diffs), common);
    return (diffs == 0 && g_failures == 0) ? 0 : 1;
}
