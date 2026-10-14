// tests/test_geom_surface_meshpatch.cpp — C++ mirror of
// probes/geom/surface/probe_meshpatch.d, replayed line for line against
// baselines/e587b235/probes/geom_surface_meshpatch.txt (303 D rows).
//
// The D file this corpus was captured against is geom/surface/meshpatch.d, 100 lines,
// sha256 e8418bd6df549a844f9d0a7a8fb69d795211efc74fb76454816acde91b23e906 — the same
// value the corpus states in its own first row (emitted below as data, so a replay
// against a corpus from another revision shows up as a difference on line 1).
// meshpatch.d has no `unittest` block, so there are no ported D assertions here; the
// 303 rows are the module's whole reference.
//
// Row groups and what each one can catch:
//   mp.v.*          the Vector3[][] constructor (D:41-53): 12 mesh nodes, 4 corners, the
//                   5x5 opCall sweep, the cell-edge clamp sweep and two area() splits.
//   mp.v.raggedlong a second row longer than row 0 is truncated, because njv is taken
//                   from row 0 alone (D:44).
//   mp.v.empty / mp.v.emptyinner / mp.v.raggedshort
//                   D's un-bounds-checked indexing: ArrayIndexError with the index and
//                   the length in the message, blamed at D:44, D:49 and D:47.
//   mp.grid.*       the StructuredGrid constructor (D:23-39) over a Line-bounded
//                   CoonsPatch, 5x4.
//   mp.grid.nkv2 / mp.slab.nkv2
//                   the nkv != 1 guard (D:25-27): object.Error, and because it is a bare
//                   `new Error` the row states file=[] line=0, which this port reports
//                   from its OWN eil::ErrorBase — a site attached here would fail the row
//                   (T-922).
//   mp.blank.*      an unfilled lattice: every vertex is Vector3.init, so the mesh, the
//                   corners and opCall are all NaN (positive NaN, +nan).
//   mp.niv1.* / mp.1x1.*  the mesh of a 1-vertex-wide lattice is NaN with the SIGN the
//                   0/0 inside make_grid_from_surface left (D prints -nan, and
//                   mp.niv1 mesh rows carry it), and opCall then dies at D:89 — index [1]
//                   past a mesh of length 1 — which only happens if the
//                   `min(i, niv-2)` clamp is unsigned, as D's is. A signed clamp gives
//                   i = -1 and blames D:87 instead.
//   mp.grid1d.*     the mirror case with njv == 1, dying at D:88.
//   mp.grid.niv0 / mp.grid.njv0
//                   the corners come from the ARRAY (D:35-38), so a lattice with no
//                   vertices dies at D:35 rather than inside the grid.
//   mp.v.clamp.*    r/s at exactly 0, at each interior cell edge, one ULP either side of
//                   it, just below 0 and above 1 — these distinguish to!int's truncation
//                   toward zero from rounding or floor(), and D's max-then-min order.
//   mp.oc.*         the two to!int sites (D:81-82) on NaN and on +-Infinity / 1e30:
//                   std.conv.ConvException "Input was NaN", and its overflow child
//                   std.conv.ConvOverflowException with "Conversion positive overflow"
//                   or "Conversion negative overflow". SUBSTRATE GAP (backlog T-928) means
//                   the port raises the parent class with the child's message, so this
//                   test picks the D class from the message — see classify() below.
//   mp.oor.*        out-of-range r and s, clamped, then extrapolated by the bilinear.
//   mp.copy.* / mp.dup.* / mp.indep.*
//                   D:55-67 and D:69-72, and the aliasing check after the original's
//                   mesh[1][2] and p11 are overwritten.
//
// Byte-for-byte comparison, no tolerance band, no skipped row.
//
// Regenerate:
/*
  ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface \
    probes/geom/surface/probe_meshpatch.d -of=/tmp/probe_g6b_mp -od/tmp/obj_g6b
  /tmp/probe_g6b_mp > baselines/e587b235/probes/geom_surface_meshpatch.txt
*/
//
// Run: ./test_geom_surface_meshpatch <baseline.txt>
#include <algorithm>
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
#include "geom/gpath/line.hpp"
#include "geom/gpath/path.hpp"
#include "geom/grid/sgrid.hpp"
#include "geom/misc/univariatefunctions.hpp"
#include "geom/surface/coonspatch.hpp"
#include "geom/surface/meshpatch.hpp"
#include "geom/surface/parametricsurface.hpp"

namespace {

using eil::geom::CoonsPatch;
using eil::geom::MeshPatch;
using eil::geom::ParametricSurface;
using eil::geom::PathPtr;
using eil::geom::StructuredGrid;
using eil::geom::Vector3;
using eil::number;

std::vector<std::string> g_lines;
int g_failures = 0;

void emit(const std::string& s) { g_lines.push_back(s); }
template <typename... Args> void emitf(const std::string& fmt, const Args&... args) {
    emit(eil::format(fmt, args...));
}

// Where D raised, as the probe recorded it. A path inside the ldc2 shadow tree is part of
// an ArrayIndexError's identity in D but no C++ port can name it, so — exactly as
// tests/test_geom_grid_usgrid.cpp:33-46 documents for the grid corpora — the location is
// transcribed here as data and the gate grades the CLASS and the MESSAGE, which are what
// the port reproduces. The rows that carry a bare object.Error are the exception: those
// report the PORT's own (empty) site, because D's is empty too and that is the point.
const std::string MP_PATH = "/tmp/gdtk_shadow/geom/surface/meshpatch.d";
const std::string CONV_PATH = "/home/tang/packages/dlang/bin/../import/std/conv.d";

// The port's exception -> D's class, from substrate/include/eil/error.hpp's measured
// table (T-922). substrate now declares std.conv.ConvOverflowException as the child of
// ConversionException (T-928) and meshpatch.cpp raises it, so the overflow child is caught
// explicitly BEFORE its parent: the gate verifies the class the port threw rather than
// re-deriving it from the message text (which would hide a wrong-class regression).
void dump_throw(const std::string& tag, const std::function<void()>& fn, const std::string& dfile,
                long dline) {
    try {
        fn();
        emitf("%s|class=none|msg=[]|file=[]|line=-1", tag);
    } catch (const eil::RangeError& e) {
        emitf("%s|class=core.exception.ArrayIndexError|msg=[%s]|file=[%s]|line=%d", tag, e.msg(),
              dfile, dline);
    } catch (const eil::ConvOverflowException& e) {
        emitf("%s|class=std.conv.ConvOverflowException|msg=[%s]|file=[%s]|line=%d", tag, e.msg(),
              dfile, dline);
    } catch (const eil::ConversionException& e) {
        emitf("%s|class=std.conv.ConvException|msg=[%s]|file=[%s]|line=%d", tag, e.msg(), dfile,
              dline);
    } catch (const eil::ErrorBase& e) {
        // A bare new Error: D reports no site, and so does the port.
        emitf("%s|class=object.Error|msg=[%s]|file=[%s]|line=%d", tag, e.msg(), e.file(),
              static_cast<long>(e.line()));
    } catch (const eil::ExceptionBase& e) {
        emitf("%s|class=object.Exception|msg=[%s]|file=[%s]|line=%d", tag, e.msg(), dfile, dline);
    }
}

// The probe's mk_mesh(4,3): every node distinguishable, so a wrong i/j cannot hide.
eil::Array<eil::Array<Vector3>> mk_mesh(size_t ni, size_t nj) {
    eil::Array<eil::Array<Vector3>> g;
    g.set_length(ni);
    for (size_t i = 0; i < ni; ++i) {
        for (size_t j = 0; j < nj; ++j) {
            g[i].append(Vector3(static_cast<double>(10 * i + j),
                                static_cast<double>(100 * i + 10 * j),
                                static_cast<double>(i) + 0.5 * static_cast<double>(j)));
        }
    }
    return g;
}

std::shared_ptr<CoonsPatch> mk_coons() {
    return std::make_shared<CoonsPatch>(Vector3(0.0, 0.1, 3.0), Vector3(1.0, 0.1, 3.0),
                                       Vector3(1.0, 1.1, 3.0), Vector3(0.0, 1.1, 3.0));
}

std::shared_ptr<StructuredGrid> mk_surf_grid(const ParametricSurface& surf, size_t niv,
                                             size_t njv) {
    eil::geom::ClusterF cf = {
        std::make_shared<eil::geom::LinearFunction>(), std::make_shared<eil::geom::LinearFunction>(),
        std::make_shared<eil::geom::LinearFunction>(), std::make_shared<eil::geom::LinearFunction>()};
    return std::make_shared<StructuredGrid>(surf, niv, njv, cf);
}

void dump_patch(const std::string& tag, const MeshPatch& mp) {
    emitf("%s str=%s", tag, mp.toString());
    emitf("%s classname=%s", tag, mp.classString());
    std::string rl;
    for (size_t i = 0; i < mp.mesh.length(); ++i) {
        if (i) rl += ",";
        rl += eil::to_string(mp.mesh[i].length());
    }
    emitf("%s niv=%d njv=%d mesh.length=%d rowlen=%s", tag, mp.niv, mp.njv, mp.mesh.length(), rl);
    for (size_t i = 0; i < mp.mesh.length(); ++i) {
        for (size_t j = 0; j < mp.mesh[i].length(); ++j) {
            const Vector3& p = mp.mesh[i][j];
            emitf("%s mesh[%d][%d] x=%.17g y=%.17g z=%.17g", tag, i, j, eil::re(p.x),
                  eil::re(p.y), eil::re(p.z));
        }
    }
    const std::pair<const char*, Vector3> cs[4] = {
        {"p00", mp.p00}, {"p10", mp.p10}, {"p11", mp.p11}, {"p01", mp.p01}};
    for (const auto& c : cs) {
        emitf("%s %s x=%.17g y=%.17g z=%.17g", tag, c.first, eil::re(c.second.x),
              eil::re(c.second.y), eil::re(c.second.z));
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

void dump_area(const std::string& tag, ParametricSurface& ps, int nr, int ns) {
    auto a = ps.area(nr, ns);
    emitf("%s area nr=%d ns=%d x=%.17g y=%.17g z=%.17g", tag, nr, ns, eil::re(a.x), eil::re(a.y),
          eil::re(a.z));
}

// The probe's dump_clamp: the cell edges themselves, nextafter() either side of them, and
// values outside [0,1]. D's nextafter is the C library's, so the operands are identical.
void dump_clamp(const std::string& tag, MeshPatch& mp) {
    const double rs[13] = {-0.5,   -1.0e-3,  0.0,
                           1.0 / 3.0,
                           // nextafter(1/3, 0) and nextafter(1/3, 1)
                           0.33333333333333326, 0.33333333333333337,
                           0.5,
                           2.0 / 3.0,
                           // nextafter(2/3, 0) and nextafter(2/3, 1)
                           0.66666666666666652, 0.66666666666666674,
                           0.99999999999999989, 1.0,
                           1.5};
    for (double r : rs) {
        auto p = mp(r, 0.5);
        emitf("%s r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g", tag, r, 0.5, eil::re(p.x),
              eil::re(p.y), eil::re(p.z));
    }
    for (double s : rs) {
        auto p = mp(0.5, s);
        emitf("%s r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g", tag, 0.5, s, eil::re(p.x),
              eil::re(p.y), eil::re(p.z));
    }
}

// ------------------------------------------------------------------
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]);
        return 2;
    }

    //--- group 0: provenance --------------------------------------------------
    emit("mp.src|dfile=geom/surface/meshpatch.d|dlines=100|sha256=e8418bd6df549a844f9d0a7a"
         "8fb69d795211efc74fb76454816acde91b23e906|probe=probes/geom/surface/probe_meshpatch"
         ".d|cc=ldc2 1.40.0 -O0 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip");

    //--- group A: the Vector3[][] constructor ---------------------------------
    eil::Array<eil::Array<Vector3>> vg = mk_mesh(4, 3);
    MeshPatch mv(vg);
    dump_patch("mp.v", mv);
    dump_sweep("mp.v", mv);
    dump_clamp("mp.v.clamp", mv);
    dump_area("mp.v", mv, 10, 10);
    dump_area("mp.v", mv, 3, 7);

    //--- group B: the StructuredGrid constructor ------------------------------
    auto sg = mk_surf_grid(*mk_coons(), 5, 4);
    MeshPatch mg(*sg);
    dump_patch("mp.grid", mg);
    dump_sweep("mp.grid", mg);
    dump_area("mp.grid", mg, 10, 10);

    //--- group C: the nkv != 1 guard (D:25-27), a bare `new Error` -------------
    dump_throw("mp.grid.nkv2",
               [&]() {
                   StructuredGrid g2(3, 4, 2);
                   MeshPatch m(g2);
               },
               "", 0);
    {
        auto slab = mk_surf_grid(*mk_coons(), 3, 3)->makeSlabGrid(1.0);
        dump_throw("mp.slab.nkv2", [&]() { MeshPatch m(*slab); }, "", 0);
    }

    //--- group D: a blank grid, whose vertices are all Vector3.init -----------
    StructuredGrid blank(3, 2);
    MeshPatch mb(blank);
    dump_patch("mp.blank", mb);
    {
        auto bp = mb(0.5, 0.5);
        emitf("mp.blank opCall r=0.5 s=0.5 x=%.17g y=%.17g z=%.17g", eil::re(bp.x), eil::re(bp.y),
              eil::re(bp.z));
    }

    //--- group E: a 1D grid (njv == 1) ----------------------------------------
    {
        eil::geom::Line l(Vector3(0.0, 0.0, 0.0), Vector3(2.0, 0.0, 0.0));
        eil::geom::UnivariateFunctionPtr cl = std::make_shared<eil::geom::LinearFunction>(0.0, 1.0);
        StructuredGrid g1(l, 5, cl);
        MeshPatch m1(g1);
        dump_patch("mp.grid1d", m1);
        dump_throw("mp.grid1d.opcall", [&]() { auto p = m1(0.5, 0.5); (void)p; }, MP_PATH, 88);
    }

    //--- group F: a 1-wide 2D grid (niv == 1) --------------------------------
    {
        auto gw = mk_surf_grid(*mk_coons(), 1, 4);
        MeshPatch mw(*gw);
        dump_patch("mp.niv1", mw);
        dump_throw("mp.niv1.opcall", [&]() { auto p = mw(0.5, 0.5); (void)p; }, MP_PATH, 89);
    }

    //--- group F2: degenerate lattice shapes ---------------------------------
    {
        StructuredGrid g11(1, 1);
        MeshPatch m11(g11);
        dump_patch("mp.1x1", m11);
        dump_throw("mp.1x1.opcall", [&]() { auto p = m11(0.5, 0.5); (void)p; }, MP_PATH, 88);
        StructuredGrid g0a(0, 5);
        dump_throw("mp.grid.niv0", [&]() { MeshPatch m(g0a); }, MP_PATH, 35);
        StructuredGrid g0b(5, 0);
        dump_throw("mp.grid.njv0", [&]() { MeshPatch m(g0b); }, MP_PATH, 35);
    }

    //--- group G: array shapes the constructor cannot serve -------------------
    dump_throw("mp.v.empty",
               [&]() {
                   eil::Array<eil::Array<Vector3>> e;
                   MeshPatch m(e);
               },
               MP_PATH, 44);
    dump_throw("mp.v.emptyinner",
               [&]() {
                   eil::Array<eil::Array<Vector3>> e;
                   e.set_length(2);
                   MeshPatch m(e);
               },
               MP_PATH, 49);
    dump_throw("mp.v.raggedshort",
               [&]() {
                   eil::Array<eil::Array<Vector3>> e = mk_mesh(2, 3);
                   e[1].set_length(2); // D: e[1] = e[1][0 .. 2]
                   MeshPatch m(e);
               },
               MP_PATH, 47);
    {
        eil::Array<eil::Array<Vector3>> lg = mk_mesh(2, 4);
        // D: [lg[0][0 .. 3], lg[1]] — row 0 has 3 nodes, row 1 still has 4.
        eil::Array<eil::Array<Vector3>> trunc;
        eil::Array<Vector3> r0;
        for (size_t j = 0; j < 3; ++j) r0.append(lg[0][j]);
        trunc.append(r0);
        trunc.append(lg[1]);
        MeshPatch mt(trunc);
        dump_patch("mp.v.raggedlong", mt);
    }

    //--- group H: the two to!int sites on non-finite arguments (D:81-82) ------
    const double nan_v = std::numeric_limits<double>::quiet_NaN();
    const double pinf = std::numeric_limits<double>::infinity();
    const double ninf = -std::numeric_limits<double>::infinity();
    dump_throw("mp.oc.r.nan", [&]() { mv(nan_v, 0.5); }, CONV_PATH, 1548);
    dump_throw("mp.oc.s.nan", [&]() { mv(0.5, nan_v); }, CONV_PATH, 1548);
    dump_throw("mp.oc.r.pinf", [&]() { mv(pinf, 0.5); }, CONV_PATH, 1573);
    dump_throw("mp.oc.r.ninf", [&]() { mv(ninf, 0.5); }, CONV_PATH, 1567);
    dump_throw("mp.oc.r.big", [&]() { mv(1.0e30, 0.5); }, CONV_PATH, 1573);
    dump_throw("mp.oc.s.big", [&]() { mv(0.5, -1.0e30); }, CONV_PATH, 1567);

    //--- group I: out-of-range r and s, clamped rather than wrapped -----------
    for (double q : {-1.0, -0.25, 1.25, 2.0}) {
        auto p = mv(q, 0.5);
        emitf("mp.oor r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g", q, 0.5, eil::re(p.x), eil::re(p.y),
              eil::re(p.z));
        auto q2 = mv(0.5, q);
        emitf("mp.oor r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g", 0.5, q, eil::re(q2.x),
              eil::re(q2.y), eil::re(q2.z));
    }

    //--- group J: the copy constructor and dup --------------------------------
    MeshPatch mc(mv); // D:55-67
    dump_patch("mp.copy", mc);
    dump_sweep("mp.copy", mc);
    auto dph = mv.dup(); // D:69-72
    MeshPatch* dp = dynamic_cast<MeshPatch*>(dph.get());
    dump_patch("mp.dup", *dp);
    dump_sweep("mp.dup", *dp);
    dump_area("mp.dup", *dp, 10, 10);

    //--- group K: aliasing. Mutate the original, then re-dump the copies. -----
    mv.mesh[1][2] = Vector3(9.0, 9.0, 9.0);
    mv.p11 = Vector3(8.0, 8.0, 8.0);
    emit("mp.indep mutated=mesh[1][2],p11");
    emitf("mp.indep copy mesh[1][2] x=%.17g y=%.17g z=%.17g", eil::re(mc.mesh[1][2].x),
          eil::re(mc.mesh[1][2].y), eil::re(mc.mesh[1][2].z));
    emitf("mp.indep copy p11 x=%.17g y=%.17g z=%.17g", eil::re(mc.p11.x), eil::re(mc.p11.y),
          eil::re(mc.p11.z));
    emitf("mp.indep dup mesh[1][2] x=%.17g y=%.17g z=%.17g", eil::re(dp->mesh[1][2].x),
          eil::re(dp->mesh[1][2].y), eil::re(dp->mesh[1][2].z));
    emitf("mp.indep dup p11 x=%.17g y=%.17g z=%.17g", eil::re(dp->p11.x), eil::re(dp->p11.y),
          eil::re(dp->p11.z));
    {
        // D:84's operands for the last row are the expressions `1.0/3.0 + 0.05` and
        // `2.0/3.0 + 0.05`, which D folds in `real` (80-bit; substrate/include/eil/
        // number.hpp:31-34) and narrows only when it stores — wave-02-brief section 5's
        // "wide compute, narrow store" rule, the same one tests/test_format_corpus.cpp:260
        // applies. Measured here: folding them in double gives 0.38333333333333331 where the
        // corpus row states 0.38333333333333336 (1 ULP), and that ULP moves the interpolated
        // point. Not a port bug and not T-930 (that is about to!double of a text literal):
        // the input differs, so the output does.
        const double r_edge = static_cast<double>(
            static_cast<eil::real>(1.0) / static_cast<eil::real>(3.0) + eil::real(0.05));
        const double s_edge = static_cast<double>(
            static_cast<eil::real>(2.0) / static_cast<eil::real>(3.0) + eil::real(0.05));
        auto af = mv(r_edge, s_edge);
        emitf("mp.indep orig aftermut r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g", r_edge, s_edge,
              eil::re(af.x), eil::re(af.y), eil::re(af.z));
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
        "%s: geom_surface_meshpatch, probe replay %zu D lines, %zu C++ lines, %d differ; "
        "%zu of %zu rows byte-identical, 0 rows accepted through a tolerance band "
        "(meshpatch.d has no unittest block)\n",
        (diffs == 0 && g_failures == 0) ? "PASS" : "FAIL", expected.size(), g_lines.size(), diffs,
        common - static_cast<size_t>(diffs), common);
    return (diffs == 0 && g_failures == 0) ? 0 : 1;
}
