// tests/test_geom_surface_controlpointpatch.cpp — C++ mirror of
// probes/geom/surface/probe_controlpointpatch.d, replayed line for line against
// baselines/e587b235/probes/geom_surface_controlpointpatch.txt (802 D rows), plus the
// `unittest` block of geom/surface/controlpointpatch.d (D:298-340) ported assertion for
// assertion, in source order.
//
// Row groups and what each one can catch:
//   cpp.ut.*              the D unittest's 4x5 grid through the 5-arg constructor:
//                         nCtrlPts, all 20 control points through getCtrlPt, a 5x5 (r,s)
//                         sweep containing 0.0 and 1.0 exactly plus a point in each
//                         parametric quadrant, and area() at (10,10), (3,7), (1,1). The
//                         sweep is what exercises the private Omega/G/H branch structure:
//                         r_hat = r*(mN-2)+1 with mN=4, mM=5 makes r_hat-alpha span
//                         [-2,2] for alpha in 1..3 and s_hat-beta span [-3,3] for beta in
//                         1..4, so all four Omega arms and G/H's three arms each are hit.
//   cpp.ut.assert.*       the two points D:336-339 asserts on, at %.17g and %.6g text.
//   cpp.dup.* / cpp.copy.* D:146-149 vs D:134-144.
//   cpp.aftermut.*        D:52 mC[i] = C[i].dup() — the caller's grid is copied, so a
//                         later mutation of C[2][2] must not move the patch.
//   cpp.afterset.*        the other direction: setCtrlPt must not move the caller's grid.
//   cpp.vtk.*             writeCtrlPtsAsVtkXml: the file body, read back whole with
//                         newlines marked <newline>, for a 4x5 grid, for a guide-patch
//                         grid and for a 1x1 grid (whose WholeExtent becomes "0 0 0 0 0 0").
//                         The loop order is j-outer/i-inner (D:210-211), which is NOT the
//                         i-outer order area() uses, so a transposed writer is visible.
//   cpp.gcoons.* / cpp.gchannel.* / cpp.ge2w.* / cpp.gaopatch.*
//                         one full dump per guide-patch BRANCH. Channel and channel-e2w
//                         both build a ChannelPatch but from different path pairs, set
//                         different flags, swap the arguments of the sampling call
//                         (D:108 vs D:123) and correct a different pair of boundaries
//                         afterwards (D:113-117 vs D:127-130) — the 20-row control-point
//                         grids are where that is visible.
//   cpp.alias.<name>*     the remaining 13 accepted spellings. Each row set is emitted
//                         INSIDE the guard, so a dropped alias shows up as threw=Error.
//   cpp.bad.<name>        the default branch (D:97-102): Error, not Exception, and the
//                         message is four concatenations including a format("%s") — its
//                         embedded newlines are marked <newline>.
//   cpp.nonull.* / cpp.no.west|east|south|north.*
//                         the four boundary Paths are optional (D:46-49) and opCall tests
//                         each again (D:169-172); with none present only T() contributes.
//   cpp.ragged.*          D:43-44 take mN from C.length and mM from C[0].length, so a grid
//                         whose rows differ in length is accepted with mM = 4. Walking
//                         row 1 past its 2 entries is an observable in itself: the control
//                         points printed before the throw are part of the corpus.
//   cpp.n1m1.* / cpp.n2m2.* mN-2 = -1 and 0, so r_hat runs backwards and stands still.
//   cpp.g1.* / cpp.g2.*   the guide constructor's dr = 1.0/(ncpi-2): a NEGATIVE step at
//                         ncpi = 1, where rPos[0] = 0.0 is then overwritten by
//                         rPos[$-1] = 1.0, and a division by zero at ncpi = 2.
//   cpp.g0 / cpp.gneg / cpp.gnegj / cpp.empty.grid
//                         ncpi = 0 indexes C[0] on an empty grid (ArrayIndexError, index
//                         [0] length 0); a negative ncpi or ncpj raises D's
//                         "Memory allocation failed" Error at D:60/D:61, before the paths
//                         are duped — measured, and which of the two allocations fails is
//                         what the pair of rows distinguishes.
//   cpp.get.* / cpp.set.*  no bounds test at all in D:186-194; getCtrlPt(0,-1) converts
//                         the int to size_t, so D reports index
//                         [18446744073709551615], which eil::Array::operator[] reproduces.
//   cpp.vtk.badpath       std.stdio.File raises std.exception.ErrnoException; the substrate
//                         has none, so the port throws ExceptionBase with the same text
//                         (controlpointpatch.cpp), the way write_surface_as_vtk_xml does.
//   cpp.oor.* / cpp.naninf.* out-of-range, infinite and NaN parameters.
//
// Byte-for-byte comparison, no tolerance band.
//
// The type= column: D prints e.classinfo.name. The port has no classinfo, so
// d_class_name() below maps the port classes the corpus actually contains back to the D
// names they stand for — eil::RangeError for core.exception.ArrayIndexError, the
// ErrorBase carrying "Memory allocation failed" for
// core.exception.OutOfMemoryError, the ExceptionBase carrying "Cannot open file `" for
// std.exception.ErrnoException, and ErrorBase/ExceptionBase themselves for
// object.Error/object.Exception. The threw= branch and the message text come from the
// port; only this label is a mapping, and the mapping is what the replay asserts.
//
// Regenerate (block comment so the shell line-splices do not trip -Wcomment):
/*
  ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface \
    probes/geom/surface/probe_controlpointpatch.d -of=/tmp/probe_cpp -od/tmp/probe_obj_w2_surf2
  /tmp/probe_cpp > baselines/e587b235/probes/geom_surface_controlpointpatch.txt
*/
//
// Run: ./test_geom_surface_controlpointpatch <baseline.txt>
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "eil/array.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/line.hpp"
#include "geom/gpath/path.hpp"
#include "geom/surface/controlpointpatch.hpp"
#include "geom/surface/parametricsurface.hpp"

namespace {

using eil::geom::ControlPointPatch;
using eil::geom::ParametricSurface;
using eil::geom::PathPtr;
using eil::geom::Vector3;
using eil::number;
using Grid = eil::Array<eil::Array<Vector3>>;
using CP = std::shared_ptr<ControlPointPatch>;

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

std::string mark_newlines(const std::string& s) {
    std::string out;
    for (char c : s) out += (c == '\n') ? std::string("<newline>") : std::string(1, c);
    return out;
}

std::string d_class_name(const eil::ErrorBase& e) {
    if (dynamic_cast<const eil::RangeError*>(&e) != nullptr)
        return "core.exception.ArrayIndexError";
    if (e.msg() == "Memory allocation failed") return "core.exception.OutOfMemoryError";
    return "object.Error";
}
std::string d_class_name(const eil::ExceptionBase& e) {
    if (e.msg().compare(0, 18, "Cannot open file `") == 0) return "std.exception.ErrnoException";
    return "object.Exception";
}

void dump_throw(const std::string& tag, const std::function<void()>& dg) {
    try {
        dg();
        emitf("%s threw=none", tag);
    } catch (const eil::ErrorBase& e) {
        emitf("%s threw=Error type=%s msg=%s", tag, d_class_name(e), mark_newlines(e.msg()));
    } catch (const eil::ExceptionBase& e) {
        emitf("%s threw=Exception type=%s msg=%s", tag, d_class_name(e), mark_newlines(e.msg()));
    }
}

std::string one_line(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string body = ss.str();
    std::string out;
    size_t pos = 0;
    while (true) {
        size_t nl = body.find('\n', pos);
        if (nl == std::string::npos) { out += body.substr(pos); break; }
        out += body.substr(pos, nl - pos) + "<newline>";
        pos = nl + 1;
    }
    return out;
}

size_t file_len(const std::string& path) {
    std::ifstream in(path, std::ios::ate | std::ios::binary);
    return static_cast<size_t>(in.tellg());
}

void dump_state(const std::string& tag, ControlPointPatch& cp) {
    emitf("%s str=%s", tag, cp.toString());
    emitf("%s classname=%s", tag, cp.classString());
    int nci = -999, ncj = -999;
    cp.nCtrlPts(nci, ncj);
    emitf("%s ncpi=%d ncpj=%d", tag, nci, ncj);
}

void dump_ctrl_pts(const std::string& tag, ControlPointPatch& cp, int nci, int ncj) {
    for (int i = 0; i < nci; ++i) {
        for (int j = 0; j < ncj; ++j) {
            auto p = cp.getCtrlPt(i, j);
            emitf("%s C[%d][%d] x=%.17g y=%.17g z=%.17g", tag, i, j, eil::re(p.x), eil::re(p.y),
                  eil::re(p.z));
        }
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

// The 4x5 control-point grid of the D unittest (D:299-327).
Grid ut_grid() {
    const int N = 4, M = 5;
    const double L = 1.0;
    Grid C;
    C.set_length(N);
    for (int i = 0; i < N; ++i) C[i].set_length(M);
    C[0][0] = Vector3(0.0, 0.0);
    C[1][0] = Vector3(0.25 * L, 0.0);
    C[2][0] = Vector3(0.75 * L, 0.0);
    C[3][0] = Vector3(L, 0.0);
    Vector3 dy(0.0, L / 6.0);
    for (int i = 0; i < N; ++i) C[i][1] = C[i][0] + dy;
    dy = Vector3(0.0, 3.0 * L / 6.0);
    for (int i = 0; i < N; ++i) C[i][2] = C[i][0] + dy;
    dy = Vector3(0.0, 5.0 * L / 6.0);
    for (int i = 0; i < N; ++i) C[i][3] = C[i][0] + dy;
    dy = Vector3(0.0, L);
    for (int i = 0; i < N; ++i) C[i][4] = C[i][0] + dy;
    return C;
}

PathPtr line(const Vector3& a, const Vector3& b) {
    return std::make_shared<eil::geom::Line>(a, b);
}

CP from_grid(const PathPtr& s, const PathPtr& n, const PathPtr& w, const PathPtr& e,
             const Grid& C) {
    return std::make_shared<ControlPointPatch>(s, n, w, e, C);
}

CP from_guide(const PathPtr& s, const PathPtr& n, const PathPtr& w, const PathPtr& e, int ncpi,
              int ncpj, const std::string& guide) {
    return std::make_shared<ControlPointPatch>(s, n, w, e, ncpi, ncpj, guide);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]);
        return 2;
    }

    auto C = ut_grid();
    PathPtr north = line(C[0][4], C[3][4]);
    PathPtr east = line(C[3][0], C[3][4]);
    PathPtr south = line(C[0][0], C[3][0]);
    PathPtr west = line(C[0][0], C[0][4]);

    //--- group A: the D unittest's patch (D:334-339) ---------------------------
    auto cp = from_grid(south, north, west, east, C);
    dump_state("cpp.ut", *cp);
    dump_ctrl_pts("cpp.ut", *cp, 4, 5);
    dump_sweep("cpp.ut", *cp);
    dump_area("cpp.ut", *cp, 10, 10);
    dump_area("cpp.ut", *cp, 3, 7);
    dump_area("cpp.ut", *cp, 1, 1);
    {
        auto p = (*cp)(0.5, 0.5);
        emitf("cpp.ut.assert r=0.5 s=0.5 x=%.17g y=%.17g z=%.17g str=%s", eil::re(p.x),
              eil::re(p.y), eil::re(p.z), p.toString());
        emitf("cpp.ut.assert approx=%s",
              eil::geom::approxEqualVectors(p, Vector3(0.5, 0.5)));
        p = (*cp)(1.0, 1.0);
        emitf("cpp.ut.assert r=1 s=1 x=%.17g y=%.17g z=%.17g str=%s", eil::re(p.x), eil::re(p.y),
              eil::re(p.z), p.toString());
        emitf("cpp.ut.assert approx11=%s",
              eil::geom::approxEqualVectors(p, Vector3(1.0, 1.0)));
    }
    emitf("cpp.ut str.as.base=%s", std::static_pointer_cast<ParametricSurface>(cp)->toString());
    {
        auto dp = std::dynamic_pointer_cast<ControlPointPatch>(cp->dup());
        dump_state("cpp.dup", *dp);
        dump_ctrl_pts("cpp.dup", *dp, 4, 5);
        dump_sweep("cpp.dup", *dp);
        dump_area("cpp.dup", *dp, 10, 10);
    }
    {
        auto cpc = std::make_shared<ControlPointPatch>(*cp);
        dump_state("cpp.copy", *cpc);
        dump_ctrl_pts("cpp.copy", *cpc, 4, 5);
        dump_sweep("cpp.copy", *cpc);
        dump_area("cpp.copy", *cpc, 10, 10);
    }
    C[2][2] = Vector3(7.0, 7.0, 7.0);
    dump_ctrl_pts("cpp.aftermut", *cp, 4, 5);
    emitf("cpp.aftermut callerC x=%.17g y=%.17g z=%.17g", eil::re(C[2][2].x),
          eil::re(C[2][2].y), eil::re(C[2][2].z));
    cp->setCtrlPt(1, 3, Vector3(-4.0, -5.0, -6.0));
    dump_ctrl_pts("cpp.afterset", *cp, 4, 5);
    emitf("cpp.afterset callerC x=%.17g y=%.17g z=%.17g", eil::re(C[1][3].x), eil::re(C[1][3].y),
          eil::re(C[1][3].z));
    cp->setCtrlPt(1, 3, Vector3(0.25, 0.5, 0.0));  // restore for the groups below

    //--- group B: the VTK-XML writer ------------------------------------------
    cp->writeCtrlPtsAsVtkXml("/tmp/probe_cpp_ut");
    emitf("cpp.vtk.ut size=%d body=%s", static_cast<int>(file_len("/tmp/probe_cpp_ut.vts")),
          one_line("/tmp/probe_cpp_ut.vts"));
    {
        auto g = from_guide(south, north, west, east, 4, 5, "coons");
        g->writeCtrlPtsAsVtkXml("/tmp/probe_cpp_coons");
        emitf("cpp.vtk.coons body=%s", one_line("/tmp/probe_cpp_coons.vts"));
        auto s1 = from_guide(south, north, west, east, 1, 1, "coons");
        s1->writeCtrlPtsAsVtkXml("/tmp/probe_cpp_one");
        emitf("cpp.vtk.one body=%s", one_line("/tmp/probe_cpp_one.vts"));
    }

    //--- group C: every guide-patch branch, fully dumped ----------------------
    {
        auto gc = from_guide(south, north, west, east, 4, 5, "coons");
        dump_state("cpp.gcoons", *gc);
        dump_ctrl_pts("cpp.gcoons", *gc, 4, 5);
        dump_sweep("cpp.gcoons", *gc);
        dump_area("cpp.gcoons", *gc, 10, 10);
        dump_area("cpp.gcoons", *gc, 3, 7);
        dump_area("cpp.gcoons", *gc, 1, 1);

        auto gch = from_guide(south, north, west, east, 4, 5, "channel");
        dump_state("cpp.gchannel", *gch);
        dump_ctrl_pts("cpp.gchannel", *gch, 4, 5);
        dump_sweep("cpp.gchannel", *gch);
        dump_area("cpp.gchannel", *gch, 10, 10);
        dump_area("cpp.gchannel", *gch, 3, 7);

        auto gew = from_guide(south, north, west, east, 4, 5, "channel-e2w");
        dump_state("cpp.ge2w", *gew);
        dump_ctrl_pts("cpp.ge2w", *gew, 4, 5);
        dump_sweep("cpp.ge2w", *gew);
        dump_area("cpp.ge2w", *gew, 10, 10);
        dump_area("cpp.ge2w", *gew, 3, 7);

        auto gao = from_guide(south, north, west, east, 4, 5, "aopatch");
        dump_state("cpp.gaopatch", *gao);
        dump_ctrl_pts("cpp.gaopatch", *gao, 4, 5);
        dump_sweep("cpp.gaopatch", *gao);
        dump_area("cpp.gaopatch", *gao, 10, 10);
    }

    //--- group D: every accepted spelling, then the default branch ------------
    for (const std::string& name :
         {"Coons", "COONS", "tfi", "TFI", "Channel", "CHANNEL", "Channel-e2w", "CHANNEL-E2W",
          "channel_e2w", "AOPatch", "AOPATCH", "area-orthogonal", "area_orthogonal"}) {
        std::string tag = "cpp.alias." + name;
        dump_throw(tag, [&]() {
            auto g = from_guide(south, north, west, east, 4, 5, name);
            emitf("%s str=%s", tag, g->toString());
            int nci = -999, ncj = -999;
            g->nCtrlPts(nci, ncj);
            emitf("%s ncpi=%d ncpj=%d", tag, nci, ncj);
            auto p0 = g->getCtrlPt(1, 2);
            emitf("%s C[1][2] x=%.17g y=%.17g z=%.17g", tag, eil::re(p0.x), eil::re(p0.y),
                  eil::re(p0.z));
            auto q = (*g)(0.5, 0.25);
            emitf("%s opCall x=%.17g y=%.17g z=%.17g", tag, eil::re(q.x), eil::re(q.y),
                  eil::re(q.z));
        });
    }
    for (const std::string& bad :
         {"", "bogus", "CoonsPatch", "channel-e2", "e2w-channel", "AOpatch", "channel_e2w_x",
          "area orthogonal", "area-orthogonol"}) {
        std::string tag = "cpp.bad." + (bad.empty() ? std::string("empty") : bad);
        dump_throw(tag, [&]() { from_guide(south, north, west, east, 4, 5, bad); });
    }

    //--- group E: the optional-boundary cases ---------------------------------
    {
        PathPtr nil;
        auto nb = from_grid(nil, nil, nil, nil, C);
        dump_state("cpp.nonull", *nb);
        dump_sweep("cpp.nonull", *nb);
        dump_area("cpp.nonull", *nb, 10, 10);
        auto nbd = std::dynamic_pointer_cast<ControlPointPatch>(nb->dup());
        dump_state("cpp.nonull.dup", *nbd);
        dump_sweep("cpp.nonull.dup", *nbd);
        auto w0 = from_grid(south, north, nil, east, C);
        dump_state("cpp.no.west", *w0);
        dump_sweep("cpp.no.west", *w0);
        auto e0 = from_grid(south, north, west, nil, C);
        dump_state("cpp.no.east", *e0);
        dump_sweep("cpp.no.east", *e0);
        auto s0 = from_grid(nil, north, west, east, C);
        dump_state("cpp.no.south", *s0);
        dump_sweep("cpp.no.south", *s0);
        auto n0 = from_grid(south, nil, west, east, C);
        dump_state("cpp.no.north", *n0);
        dump_sweep("cpp.no.north", *n0);
    }

    //--- group F: control-point counts and the degenerate ones ----------------
    {
        Grid ragged;
        ragged.set_length(3);
        ragged[0].set_length(4);
        ragged[1].set_length(2);
        ragged[2].set_length(4);
        for (int i = 0; i < 3; ++i)
            for (size_t j = 0; j < ragged[i].length(); ++j)
                ragged[i][j] = Vector3(static_cast<double>(i) + 0.5 * static_cast<double>(j),
                                       static_cast<double>(j), 0.0);
        auto rg = from_grid(south, north, west, east, ragged);
        dump_state("cpp.ragged", *rg);
        dump_throw("cpp.ragged.pts", [&]() { dump_ctrl_pts("cpp.ragged", *rg, 3, 4); });
        dump_throw("cpp.ragged.sweep", [&]() {
            for (double r : {0.0, 0.5, 1.0}) {
                for (double s : {0.0, 0.5, 1.0}) {
                    auto p = (*rg)(r, s);
                    emitf("cpp.ragged interior r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g", r, s,
                          eil::re(p.x), eil::re(p.y), eil::re(p.z));
                }
            }
        });
    }
    {
        Grid one;
        one.set_length(1);
        one[0].set_length(1);
        one[0][0] = Vector3(3.0, 4.0, 5.0);
        auto p1 = from_grid(south, north, west, east, one);
        dump_state("cpp.n1m1", *p1);
        dump_sweep("cpp.n1m1", *p1);
        dump_area("cpp.n1m1", *p1, 1, 1);

        Grid two;
        two.set_length(2);
        for (int i = 0; i < 2; ++i) two[i].set_length(2);
        two[0][0] = Vector3(0.0, 0.0, 0.0);
        two[1][0] = Vector3(1.0, 0.0, 0.0);
        two[0][1] = Vector3(0.0, 1.0, 0.0);
        two[1][1] = Vector3(1.0, 1.0, 0.0);
        auto p2 = from_grid(south, north, west, east, two);
        dump_state("cpp.n2m2", *p2);
        dump_sweep("cpp.n2m2", *p2);
        dump_area("cpp.n2m2", *p2, 3, 7);
    }
    {
        auto tmp1 = from_guide(south, north, west, east, 1, 1, "coons");
        dump_state("cpp.g1", *tmp1);
        auto g1 = from_guide(south, north, west, east, 1, 1, "coons");
        dump_ctrl_pts("cpp.g1", *g1, 1, 1);
        dump_sweep("cpp.g1", *g1);
        auto g2 = from_guide(south, north, west, east, 2, 3, "coons");
        dump_state("cpp.g2", *g2);
        dump_ctrl_pts("cpp.g2", *g2, 2, 3);
        dump_sweep("cpp.g2", *g2);
        dump_throw("cpp.g0", [&]() { from_guide(south, north, west, east, 0, 3, "coons"); });
        dump_throw("cpp.gneg", [&]() { from_guide(south, north, west, east, -1, 3, "coons"); });
        dump_throw("cpp.gnegj", [&]() { from_guide(south, north, west, east, 3, -1, "coons"); });
        Grid empty;
        dump_throw("cpp.empty.grid", [&]() { from_grid(south, north, west, east, empty); });
    }

    //--- group G: getCtrlPt / setCtrlPt / nCtrlPts out of range ---------------
    dump_throw("cpp.get.oob", [&]() { cp->getCtrlPt(4, 0); });
    dump_throw("cpp.get.neg", [&]() { cp->getCtrlPt(-1, 0); });
    dump_throw("cpp.get.oob2", [&]() { cp->getCtrlPt(0, 5); });
    dump_throw("cpp.set.oob", [&]() { cp->setCtrlPt(0, 5, Vector3(1.0, 1.0, 1.0)); });
    dump_throw("cpp.vtk.badpath", [&]() { cp->writeCtrlPtsAsVtkXml("/no_such_dir_xyz/out"); });

    //--- group H: out-of-range and non-finite parameters ----------------------
    {
        for (double r : {-1.0, -0.5, 1.5, 2.0}) {
            for (double s : {-1.0, -0.5, 1.5, 2.0}) {
                auto p = (*cp)(r, s);
                emitf("cpp.oor r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g", r, s, eil::re(p.x),
                      eil::re(p.y), eil::re(p.z));
            }
        }
        for (double r : {std::numeric_limits<double>::quiet_NaN(),
                         std::numeric_limits<double>::infinity(),
                         -std::numeric_limits<double>::infinity()}) {
            auto p = (*cp)(r, 0.5);
            emitf("cpp.naninf r=%.17g x=%.17g y=%.17g z=%.17g", r, eil::re(p.x), eil::re(p.y),
                  eil::re(p.z));
        }
    }

    //--- part A: the `unittest` block of geom/surface/controlpointpatch.d -----
    // D:298-340, two asserts, ported in source order. The grid and the four Lines are
    // rebuilt exactly as D:303-332 does.
    {
        const int N = 4, M = 5;
        const double L = 1.0;
        Grid U;
        U.set_length(N);
        for (int i = 0; i < N; ++i) U[i].set_length(M);
        U[0][0] = Vector3(0.0, 0.0);
        U[1][0] = Vector3(0.25 * L, 0.0);
        U[2][0] = Vector3(0.75 * L, 0.0);
        U[3][0] = Vector3(L, 0.0);
        Vector3 dy(0.0, L / 6.0);
        for (int i = 0; i < N; ++i) U[i][1] = U[i][0] + dy;
        dy = Vector3(0.0, 3.0 * L / 6.0);
        for (int i = 0; i < N; ++i) U[i][2] = U[i][0] + dy;
        dy = Vector3(0.0, 5.0 * L / 6.0);
        for (int i = 0; i < N; ++i) U[i][3] = U[i][0] + dy;
        dy = Vector3(0.0, L);
        for (int i = 0; i < N; ++i) U[i][4] = U[i][0] + dy;

        PathPtr uN = line(U[0][4], U[3][4]);
        PathPtr uE = line(U[3][0], U[3][4]);
        PathPtr uS = line(U[0][0], U[3][0]);
        PathPtr uW = line(U[0][0], U[0][4]);
        auto ctrlPtPatch = from_grid(uS, uN, uW, uE, U);

        auto p = (*ctrlPtPatch)(0.5, 0.5);
        check(eil::geom::approxEqualVectors(p, Vector3(0.5, 0.5)),
              "controlpointpatch.d:337 approxEqualVectors(p, Vector3(0.5, 0.5))");
        p = (*ctrlPtPatch)(1.0, 1.0);
        check(eil::geom::approxEqualVectors(p, Vector3(1.0, 1.0)),
              "controlpointpatch.d:339 approxEqualVectors(p, Vector3(1.0, 1.0))");
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
        "%s: geom_surface_controlpointpatch, %d/%d unittest assertions hold "
        "(controlpointpatch.d:337,339); probe replay %zu D lines, %zu C++ lines, %d differ; "
        "%zu of %zu rows byte-identical, 0 rows accepted through a tolerance band\n",
        (diffs == 0 && g_failures == 0) ? "PASS" : "FAIL", g_checks - g_failures, g_checks,
        expected.size(), g_lines.size(), diffs, common - static_cast<size_t>(diffs), common);
    return (diffs == 0 && g_failures == 0) ? 0 : 1;
}
