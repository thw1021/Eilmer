// probes/geom/surface/probe_nurbssurface.d — what geom/surface/nurbssurface.d
// (156 lines) produces.
//
// NURBSSurface is Piegl & Tiller's A4.3 de Boor surface (D:119-135) over a weighted
// control net, the two knot vectors and the two degrees, evaluated through the shared
// kernels in geom/misc/nurbs_utils.d (findSpan, basisFuns, NURBSWorkspace). Five of its
// constructor checks throw `new Error` (D:28, :35, :42, :50, :58) and the out-of-range
// parameter inside findSpan throws `new Exception` (nurbs_utils.d:33) — different
// throwable classes, so both are captured with the class name D prints.
//
// Why measuring rather than reasoning:
//  * mNu, mNv, temp, mSw, mS and the two workspaces are *static* class fields (D:111-117):
//    one buffer per process, and EVERY constructor resizes it to its own degree
//    (D:66-70, D:81-85). Group "ord" therefore builds surfaces in a deliberate order and
//    evaluates an earlier one between two constructions, because that is the only way to
//    see the interference. It is preserved, not fixed — the same semantics
//    src/geom/gpath/nurbs.cpp:32-38 documents for the curve class.
//  * the copy constructor has a real bug: D:80 writes `this.m_b = other.m_p;` where the
//    primary constructor sets m_b = Pw[0].length-1 (D:53). m_b is the control-point count
//    in v, so a copy of a surface whose p and q differ is evaluated with the WRONG span
//    search in the v direction. Group "copy" dumps the same (u,v) through the original,
//    through dup() (which goes back through the validating constructor and is therefore
//    correct) and through the copy constructor, so the difference is a row in the corpus
//    rather than a comment.
//  * toString() (D:97) interpolates to!string of a nested array (double[4][][]), of two
//    double[] knot vectors and of two ints — four different D formatters, all dumped.
//    m_a and m_b are NOT in the text, so they can only be observed through evaluation.
//  * deBoor divides by the accumulated weight (D:133 `mS[i] = mSw[i]/mSw[3]`), so a net
//    whose weights are all zero produces 0/0 -> nan. Group "zw" dumps that rather than
//    avoiding it.
//  * mPw[uspan-m_p+k][vspan-m_q+l] is a double subscript into a jagged array; with the
//    static shared buffers a wrong span reaches an index outside the row and D raises a
//    RangeError, whose exact message is part of the corpus.
//  * the module has no classString override (no ParametricSurface declares one) and no
//    enforce; its `unittest` block (D:138-156) is the Ex4.3 case and is ported as
//    assertions in tests/test_geom_surface_nurbssurface.cpp.
//
// Build and capture (one line, so the header comment stays a single // comment):
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface probes/geom/surface/probe_nurbssurface.d -of=/tmp/probe_ns -od=/tmp/w2_surfB/obj_ns
//   then /tmp/probe_ns > baselines/e587b235/probes/geom_surface_nurbssurface.txt
import std.algorithm;
import std.array;
import std.conv;
import std.format;
import std.math;
import std.stdio;

import geom.elements;
import geom.surface.nurbssurface;
import geom.surface.parametricsurface;
import nm.number;

string esc(string s) { return replace(replace(s, "\n", "<newline>"), "\r", "<cr>"); }

void dump_v3(string key, const Vector3 v) {
    writefln("%s x=%.17g y=%.17g z=%.17g", key, v.x, v.y, v.z);
}

double[4] ptw(double x, double y, double z, double w) { double[4] r = [x, y, z, w]; return r; }

// A nu x nv net with weight w for every point.
double[4][][] grid(int nu, int nv, double w) {
    double[4][][] p;
    p.length = nu;
    foreach (int i; 0 .. nu) {
        p[i].length = nv;
        foreach (int j; 0 .. nv) {
            p[i][j] = ptw(cast(double) i, cast(double) j, 0.25 * (i + j), w);
        }
    }
    return p;
}

// A 4x4 net that is NOT planar and has non-uniform weights: the m_b quirk changes the
// v-direction span, and on a planar net every span gives the same point, so the bug
// would be invisible. Coordinates are deliberately irrational-ish products.
double[4][][] net44(double w0) {
    double[4][][] p;
    p.length = 4;
    foreach (int i; 0 .. 4) {
        p[i].length = 4;
        foreach (int j; 0 .. 4) {
            p[i][j] = ptw(0.5 * i + 0.1 * i * j, 0.25 * j * j - 0.1 * i,
                          0.2 * (i + 1) * (j + 1), w0 + 0.2 * i * j);
        }
    }
    return p;
}

void try_build(string key, double[4][][] Pw, double[] U, int p, double[] V, int q) {
    try {
        auto s = new NURBSSurface(Pw, U, p, V, q);
        writefln("%s built str=%s", key, s.toString());
    } catch (Throwable t) {
        writefln("%s thrown type=%s msg=%s", key, t.classinfo.name, esc(t.msg));
    }
}

void eval_at(string key, NURBSSurface s, double u, double v) {
    try {
        dump_v3(key, s.opCall(u, v));
    } catch (Throwable t) {
        writefln("%s thrown type=%s msg=%s", key, t.classinfo.name, esc(t.msg));
    }
}

void main()
{
    // --- Ex4.3 from Piegl & Tiller (the module's own unittest case) --------------
    double[4][][] Pw43 = [[[0.0, 0.0, 0.0, 0.0], [0.0, 0.0, 0.0, 0.0], [0.0, 0.0, 0.0, 0.0],
                           [0.0, 0.0, 0.0, 0.0], [0.0, 0.0, 0.0, 0.0]],
                          [[0.0, 0.0, 0.0, 0.0], [0.0, 0.0, 0.0, 0.0], [0.0, 0.0, 0.0, 0.0],
                           [0.0, 0.0, 0.0, 0.0], [0.0, 0.0, 0.0, 0.0]],
                          [[0.0, 0.0, 0.0, 0.0], [0.0, 2.0, 4.0, 1.0], [0.0, 6.0, 4.0, 2.0],
                           [0.0, 2.0, 0.0, 1.0], [0.0, 0.0, 0.0, 0.0]],
                          [[0.0, 0.0, 0.0, 0.0], [4.0, 6.0, 8.0, 2.0], [12.0, 24.0, 12.0, 6.0],
                           [4.0, 6.0, 0.0, 2.0], [0.0, 0.0, 0.0, 0.0]],
                          [[0.0, 0.0, 0.0, 0.0], [4.0, 2.0, 4.0, 1.0], [8.0, 6.0, 4.0, 2.0],
                           [4.0, 2.0, 0.0, 1.0], [0.0, 0.0, 0.0, 0.0]],
                          [[0.0, 0.0, 0.0, 0.0], [0.0, 0.0, 0.0, 0.0], [0.0, 0.0, 0.0, 0.0],
                           [0.0, 0.0, 0.0, 0.0], [0.0, 0.0, 0.0, 0.0]],
                          [[0.0, 0.0, 0.0, 0.0], [0.0, 0.0, 0.0, 0.0], [0.0, 0.0, 0.0, 0.0],
                           [0.0, 0.0, 0.0, 0.0], [0.0, 0.0, 0.0, 0.0]],
                          [[0.0, 0.0, 0.0, 0.0], [0.0, 0.0, 0.0, 0.0], [0.0, 0.0, 0.0, 0.0],
                           [0.0, 0.0, 0.0, 0.0], [0.0, 0.0, 0.0, 0.0]]];
    double[] U43 = [0.0, 0.0, 0.0, 1.0, 2.0, 3.0, 4.0, 4.0, 5.0, 5.0, 5.0];
    double[] V43 = [0.0, 0.0, 0.0, 1.0, 2.0, 3.0, 3.0, 3.0];
    auto e43 = new NURBSSurface(Pw43, U43, 2, V43, 2);

    // --- group 1: toString() text, and the unittest point ------------------------
    writefln("ns.e43 str=%s", e43.toString());
    eval_at("ns.e43.unittest u=2.5 v=1", e43, 5.0 / 2.0, 1.0);
    foreach (u; [0.0, 1.0, 2.0, 2.5, 3.5, 4.0, 5.0]) {
        foreach (v; [0.0, 1.0, 2.0, 2.5, 3.0]) {
            eval_at(format("ns.e43.sweep u=%.17g v=%.17g", u, v), e43, u, v);
        }
    }

    // --- group 2: a clamped quadratic 4x4 net (domain [0,1]x[0,1]) ---------------
    double[] Uc = [0.0, 0.0, 0.0, 0.5, 1.0, 1.0, 1.0];
    auto g = new NURBSSurface(grid(4, 4, 1.0), Uc, 2, Uc, 2);
    writefln("ns.grid str=%s", g.toString());
    foreach (u; [0.0, 0.25, 0.5, 0.75, 1.0]) {
        foreach (v; [0.0, 0.25, 0.5, 0.75, 1.0]) {
            eval_at(format("ns.grid.sweep u=%.17g v=%.17g", u, v), g, u, v);
        }
    }
    // the four corners must land on the corner control points
    eval_at("ns.grid.corner00", g, 0.0, 0.0);
    eval_at("ns.grid.corner10", g, 1.0, 0.0);
    eval_at("ns.grid.corner01", g, 0.0, 1.0);
    eval_at("ns.grid.corner11", g, 1.0, 1.0);

    // non-unit weights: the division by mSw[3] (D:133) is then visible.
    auto gw = new NURBSSurface(grid(4, 4, 2.5), Uc, 2, Uc, 2);
    writefln("ns.gridw str=%s", gw.toString());
    foreach (u; [0.0, 0.5, 1.0]) {
        foreach (v; [0.0, 0.5, 1.0]) {
            eval_at(format("ns.gridw.sweep u=%.17g v=%.17g", u, v), gw, u, v);
        }
    }
    // all-zero weights -> 0/0.
    auto gz = new NURBSSurface(grid(4, 4, 0.0), Uc, 2, Uc, 2);
    writefln("ns.gridz str=%s", gz.toString());
    foreach (u; [0.0, 0.5, 1.0]) {
        foreach (v; [0.0, 0.5, 1.0]) {
            eval_at(format("ns.gridz.sweep u=%.17g v=%.17g", u, v), gz, u, v);
        }
    }

    // --- group 3: the five constructor guards ------------------------------------
    double[] U1 = [0.0, 0.0, 0.0, 1.0, 1.0, 1.0];
    try_build("ns.guards.oneRow", grid(1, 2, 1.0), U1, 0, Uc, 1);
    try_build("ns.guards.oneCol", grid(2, 1, 1.0), Uc, 1, U1, 0);
    try_build("ns.guards.pTooBig", grid(4, 4, 1.0), Uc, 4, Uc, 2);
    try_build("ns.guards.qTooBig", grid(4, 4, 1.0), Uc, 2, Uc, 5);
    try_build("ns.guards.shortU", grid(4, 4, 1.0), Uc[0 .. 5], 2, Uc, 2);
    try_build("ns.guards.shortV", grid(4, 4, 1.0), Uc, 2, Uc[0 .. 5], 2);
    try_build("ns.guards.negP", grid(4, 4, 1.0), Uc, -1, Uc, 2);
    try_build("ns.guards.negQ", grid(4, 4, 1.0), Uc, 2, Uc, -1);
    try_build("ns.guards.zeroRows", grid(0, 2, 1.0), Uc, 2, Uc, 2);
    // a valid degree-0 surface, the smallest legal one
    try_build("ns.guards.deg0", grid(2, 2, 1.0), U1, 0, U1, 0);

    // --- group 4: findSpan's Exception, and the tol clamping ---------------------
    eval_at("ns.range.u.below", g, -0.1, 0.5);
    eval_at("ns.range.u.above", g, 1.1, 0.5);
    eval_at("ns.range.v.below", g, 0.5, -0.25);
    eval_at("ns.range.v.above", g, 0.5, 2.0);
    eval_at("ns.clamp.u.below", g, -1e-13, 0.5);
    eval_at("ns.clamp.u.above", g, 1.0 + 1e-13, 0.5);
    eval_at("ns.clamp.v.below", g, 0.5, -1e-13);
    eval_at("ns.clamp.v.above", g, 0.5, 1.0 + 1e-13);
    // Ex4.3 outside its own domain
    eval_at("ns.e43.outside", e43, 5.5, 1.0);

    // --- group 5: construction order against the shared static buffers ----------
    // A degree-3 x degree-1 surface next (a=4 needs 9 knots, b=1 needs 4).
    double[] Ucubic = [0.0, 0.0, 0.0, 0.0, 0.5, 1.0, 1.0, 1.0, 1.0];
    double[] Vlin = [0.0, 0.0, 1.0, 1.0];
    eval_at("ns.ord.before", g, 0.25, 0.75);
    auto cub = new NURBSSurface(grid(5, 2, 1.0), Ucubic, 3, Vlin, 1);
    writefln("ns.ord.cub str=%s", cub.toString());
    eval_at("ns.ord.cub.a", cub, 0.0, 0.0);
    eval_at("ns.ord.cub.b", cub, 0.5, 1.0);
    // MEASURED, and the opposite of the intuition: the cubic surface has q = 1, so it
    // SHRINKS mNv and temp to length 2 even though it grew mNu — and the quadratic
    // surface then fails in the v direction with "index [2] is out of bounds for array of
    // length 2" (row ns.ord.afterBig). The u degree is irrelevant to that row.
    eval_at("ns.ord.afterBig", g, 0.25, 0.75);
    // now a degree-1 surface, which SHRINKS the shared buffers under the quadratic one
    auto lin = new NURBSSurface(grid(2, 2, 1.0), Ulin(), 1, Ulin(), 1);
    writefln("ns.ord.lin str=%s", lin.toString());
    eval_at("ns.ord.lin.a", lin, 0.0, 0.5);
    eval_at("ns.ord.afterSmall", g, 0.25, 0.75);
    eval_at("ns.ord.afterSmall2", g, 0.0, 1.0);
    // rebuilding the quadratic restores it
    auto g2 = new NURBSSurface(grid(4, 4, 1.0), Uc, 2, Uc, 2);
    eval_at("ns.ord.rebuilt", g2, 0.25, 0.75);
    // ... and the OLD quadratic object works again, because only the shared buffers
    // were too small; its own mU/m_a/m_p were never touched.
    eval_at("ns.ord.old.afterRebuild", g, 0.25, 0.75);

    // --- group 6: dup() vs the copy constructor (the m_b quirk) ------------------
    // Construction order matters here because the buffers are static: every construct
    // resizes them to ITS degree, so the cubic x linear trio is built and evaluated
    // first, then the two quadratic trios.
    //
    // v = 1.0 is EXCLUDED from the copy's sweep, measured: the copy of a 4x4 quadratic
    // surface (m_b = 2 instead of 3) makes findSpan's binary search fail to converge for
    // any v in (U[n+1], U[last]] — cp(0.0, 1.0) and cp(1.0, 1.0) did not return within
    // 6 s (timeout exit status 124) while cp(0.0, 0.5) and cp(0.5, 0.5) returned in
    // under a millisecond (0, 1.5, 0.375 and 1.5, 1.5, 0.75). There is no iteration cap
    // in findSpan (nurbs_utils.d:40-45), so the reference itself spins forever and no row
    // can be written for it. The original and its dup DO take v = 1.0 and those rows are
    // kept, so the correct behaviour is still on the record.
    auto cubD = cast(NURBSSurface) cub.dup();
    auto cubC = new NURBSSurface(cub);
    foreach (u; [0.0, 0.5, 1.0]) {
        foreach (v; [0.0, 1.0]) {
            eval_at(format("ns.cub.orig u=%.17g v=%.17g", u, v), cub, u, v);
            eval_at(format("ns.cub.dup u=%.17g v=%.17g", u, v), cubD, u, v);
            eval_at(format("ns.cub.copy u=%.17g v=%.17g", u, v), cubC, u, v);
        }
    }
    // A non-planar quadratic net, where a wrong v span changes the point.
    auto gp = new NURBSSurface(net44(1.0), Uc, 2, Uc, 2);
    writefln("ns.gp str=%s", gp.toString());
    auto gpD = cast(NURBSSurface) gp.dup();
    auto gpC = new NURBSSurface(gp);
    writefln("ns.gp.dup str=%s", gpD.toString());
    writefln("ns.gp.copy str=%s", gpC.toString());
    foreach (u; [0.0, 0.3, 1.0]) {
        foreach (v; [0.0, 0.25, 0.5]) {
            eval_at(format("ns.gp.orig u=%.17g v=%.17g", u, v), gp, u, v);
            eval_at(format("ns.gp.dup u=%.17g v=%.17g", u, v), gpD, u, v);
            eval_at(format("ns.gp.copy u=%.17g v=%.17g", u, v), gpC, u, v);
        }
    }
    // The planar 4x4 net: here the wrong span still lands on the same point, which is
    // itself worth having in the corpus (the bug is not always visible).
    auto dp = cast(NURBSSurface) g.dup();
    auto cp = new NURBSSurface(g);
    writefln("ns.dup str=%s", dp.toString());
    writefln("ns.copy str=%s", cp.toString());
    foreach (u; [0.0, 0.25, 0.5, 1.0]) {
        foreach (v; [0.0, 0.5]) {
            eval_at(format("ns.orig u=%.17g v=%.17g", u, v), g, u, v);
            eval_at(format("ns.dup u=%.17g v=%.17g", u, v), dp, u, v);
            eval_at(format("ns.copy u=%.17g v=%.17g", u, v), cp, u, v);
        }
    }
    foreach (u; [0.0, 0.5, 1.0]) {
        eval_at(format("ns.orig.v1 u=%.17g", u), g, u, 1.0);
        eval_at(format("ns.dup.v1 u=%.17g", u), dp, u, 1.0);
    }

    // --- group 7: area() through the base class ---------------------------------
    try {
        auto a1 = g.area();
        writefln("ns.area.default x=%.17g y=%.17g z=%.17g", a1.x, a1.y, a1.z);
    } catch (Throwable t) {
        writefln("ns.area.default thrown type=%s msg=%s", t.classinfo.name, esc(t.msg));
    }
    try {
        auto a2 = g.area(3, 7);
        writefln("ns.area.r3s7 x=%.17g y=%.17g z=%.17g", a2.x, a2.y, a2.z);
    } catch (Throwable t) {
        writefln("ns.area.r3s7 thrown type=%s msg=%s", t.classinfo.name, esc(t.msg));
    }
    try {
        auto a3 = e43.area(4, 2);
        writefln("ns.area.e43r4s2 x=%.17g y=%.17g z=%.17g", a3.x, a3.y, a3.z);
    } catch (Throwable t) {
        writefln("ns.area.e43r4s2 thrown type=%s msg=%s", t.classinfo.name, esc(t.msg));
    }
}

// two knots either side of a linear domain
double[] Ulin() { return [0.0, 0.0, 1.0, 1.0]; }
