// probes/geom/surface/probe_controlpointpatch.d — what geom/surface/controlpointpatch.d
// produces.
//
// ControlPointPatch (D:30-296) implements Eiseman (1988)'s control-point form: a grid of
// control points C[i][j] plus up to four optional bounding Paths whose conformity is
// applied selectively in opCall (D:161-174). The control points come either from the
// caller (5-arg constructor, D:41-53) or from a *guide patch* sampled at interior
// parameter positions (7-arg constructor, D:55-132).
//
// What is measured rather than reasoned:
//  * the guide-patch switch (D:81-103) has 17 accepted spellings in four branches and one
//    default that throws `new Error` with a THREE-PART message assembled with
//    format("... '%s' ...", guidePatch). Because the message contains literal newlines,
//    the replayed text has them replaced by the marker <newline> (the same convention the
//    VTK-body rows use, probe_parametricsurface.d:124). Every spelling is exercised, so a
//    port that drops an alias falls into the default and the row changes.
//  * branch 2 and 3 both build a ChannelPatch but with DIFFERENT argument pairs
//    (south/north vs west/east) and only branch 3 sets channelBoundedOnEW, which swaps the
//    argument order of the sampling call (D:123 `gpatch(sPos[j], rPos[i])`) and the
//    boundary that gets corrected afterwards. Each branch's control-point grid is dumped in
//    full — that is the only place the swap is visible.
//  * D:63 the 7-arg constructor DELEGATES to the 5-arg one in the middle of its body, so
//    mN/mM/the four paths/mC are all set before rPos/sPos are computed and before gpatch is
//    chosen. The order of effects is preserved in the port.
//  * dr = 1.0/(ncpi - 2) (D:65-66) is a division by zero at ncpi == 2 and a NEGATIVE step
//    at ncpi == 1. Both are dumped: rPos[0] = 0.0 is then overwritten by rPos[$-1] = 1.0
//    when ncpi == 1, so rPos is [1.0], which is observable in the sampled control points.
//    ncpi == 0 makes `C[0].length` at D:44 an index error, and ncpi < 0 makes
//    `C.length = ncpi` raise D's allocation Error. Both messages are captured.
//  * D:46-49 `if (north !is null) mNorth = north.dup();` — the four boundary Paths are
//    OPTIONAL, and opCall (D:169-172) tests each one again before adding its conformity
//    term. All four present, none present, and each one missing alone are dumped, because
//    dropping one term moves the surface by (1 - G)*(west(s) - F(s_hat,1)), which is not
//    small. Only the 5-arg constructor is used for the null cases: with the guide-patch
//    constructor a null Path is dereferenced inside CoonsPatch, and D's
//    NullPointerException has no ported equivalent (see the port header).
//  * D:43-44 mN = C.length and mM = C[0].length, taken from DIFFERENT rows' extents: the
//    port must keep the ragged-array behaviour, so a patch built from rows of unequal
//    length is dumped (nCtrlPts reports mC[0].length, while opCall runs to mM).
//  * D:51-52 `mC[i] = C[i].dup()` is a deep copy: mutating the caller's array afterwards
//    must not move the patch, and mutating the patch with setCtrlPt must not move the
//    caller's array. Both directions are measured.
//  * the private Eiseman functions Omega/G/H (D:224-250) have four, three and three
//    branches. They are only reachable through opCall, and the argument that selects a
//    branch is r_hat - alpha with r_hat = r*(mN-2)+1, so the 4x5 grid of the D unittest,
//    swept over r and s including exactly 0.0 and 1.0 and a point in each parametric
//    quadrant, covers r_hat - alpha in [-2, 2]: every Omega branch is hit. A wider
//    out-of-range (r,s) sweep is included to reach the r < -1 and r > 1 arms again.
//  * nCtrlPts (D:181-184) reads mC[0].length, so it throws on an empty mC; getCtrlPt and
//    setCtrlPt (D:186-194) index mC[i][j] with no bounds test at all, so both negative and
//    past-the-end indices are captured.
//  * writeCtrlPtsAsVtkXml (D:196-222) appends ".vts" itself and writes the points in
//    j-outer/i-inner order with "%20.16e". The whole file body is dumped back through
//    readFile, newlines marked <newline>, for a 4x5 patch, for a guide-patch-built patch
//    and for a 1x1 patch (where WholeExtent becomes "0 0 0 0 0 0").
//  * area() is the base sum (parametricsurface.d:40-66) at the D defaults (10,10), an
//    asymmetric (3,7) and (1,1).
//  * toString() (D:176-179) is the constant "ControlPointPatch()" — it carries no state,
//    so every stored member is dumped separately instead.
//
// Build and capture (see probes/geom/surface/probe_parametricsurface.d for why the
// reference tree is copied with @nogc stripped):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface \
//     probes/geom/surface/probe_controlpointpatch.d -of=/tmp/probe_cpp -od/tmp/probe_obj_w2_surf2
//   /tmp/probe_cpp > baselines/e587b235/probes/geom_surface_controlpointpatch.txt
import std.array;
import std.conv;
import std.file;
import std.format;
import std.math;
import std.stdio;
import std.string;

import geom.elements;
import geom.gpath;
import geom.surface.controlpointpatch;
import geom.surface.parametricsurface;
import nm.number;

/// The file body on one line, newlines marked, as probe_parametricsurface.d does.
string one_line(string path) {
    return join(split(cast(string)read(path), "\n"), "<newline>");
}

void dump_state(const char[] tag, ControlPointPatch cp) {
    writefln("%s str=%s", tag, cp.toString());
    writefln("%s classname=%s", tag, split(to!string(typeid(cp).name), ".")[$-1]);
    int nci = -999, ncj = -999;
    cp.nCtrlPts(nci, ncj);
    writefln("%s ncpi=%d ncpj=%d", tag, nci, ncj);
}

void dump_ctrl_pts(const char[] tag, ControlPointPatch cp, int nci, int ncj) {
    foreach (i; 0 .. nci) {
        foreach (j; 0 .. ncj) {
            auto p = cp.getCtrlPt(i, j);
            writefln("%s C[%d][%d] x=%.17g y=%.17g z=%.17g", tag, i, j, p.x, p.y, p.z);
        }
    }
}

void dump_sweep(const char[] tag, ParametricSurface ps) {
    foreach (r; [0.0, 0.2, 0.5, 0.75, 1.0]) {
        foreach (s; [0.0, 0.25, 0.5, 0.75, 1.0]) {
            auto p = ps(r, s);
            writefln("%s opCall r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g", tag, r, s, p.x, p.y,
                     p.z);
        }
    }
}

void dump_area(const char[] tag, ParametricSurface ps, int nr, int ns) {
    auto a = ps.area(nr, ns);
    writefln("%s area nr=%d ns=%d x=%.17g y=%.17g z=%.17g", tag, nr, ns, a.x, a.y, a.z);
}

void dump_throw(const char[] tag, void delegate() dg) {
    try {
        dg();
        writefln("%s threw=none", tag);
    } catch (Error e) {
        writefln("%s threw=Error type=%s msg=%s", tag, e.classinfo.name,
                 replace(e.msg, "\n", "<newline>"));
    } catch (Exception e) {
        writefln("%s threw=Exception type=%s msg=%s", tag, e.classinfo.name,
                 replace(e.msg, "\n", "<newline>"));
    }
}

/// Build the 4x5 control-point grid of the D unittest (D:299-327).
Vector3[][] ut_grid() {
    int N = 4;
    int M = 5;
    double L = 1.0;
    Vector3[][] C;
    C.length = N;
    foreach (i; 0 .. N) C[i].length = M;
    C[0][0] = Vector3(0.0, 0.0);
    C[1][0] = Vector3(0.25 * L, 0.0);
    C[2][0] = Vector3(0.75 * L, 0.0);
    C[3][0] = Vector3(L, 0.0);
    auto dy = Vector3(0.0, L / 6.0);
    foreach (i; 0 .. N) C[i][1] = C[i][0] + dy;
    dy = Vector3(0.0, 3.0 * L / 6.0);
    foreach (i; 0 .. N) C[i][2] = C[i][0] + dy;
    dy = Vector3(0.0, 5.0 * L / 6.0);
    foreach (i; 0 .. N) C[i][3] = C[i][0] + dy;
    dy = Vector3(0.0, L);
    foreach (i; 0 .. N) C[i][4] = C[i][0] + dy;
    return C;
}

void main()
{
    auto C = ut_grid();
    Path north = new Line(C[0][4], C[3][4]);
    Path east = new Line(C[3][0], C[3][4]);
    Path south = new Line(C[0][0], C[3][0]);
    Path west = new Line(C[0][0], C[0][4]);

    //--- group A: the D unittest's patch (D:334-339) ----------------------------
    auto cp = new ControlPointPatch(south, north, west, east, C);
    dump_state("cpp.ut", cp);
    dump_ctrl_pts("cpp.ut", cp, 4, 5);
    dump_sweep("cpp.ut", cp);
    dump_area("cpp.ut", cp, 10, 10);
    dump_area("cpp.ut", cp, 3, 7);
    dump_area("cpp.ut", cp, 1, 1);
    {
        auto p = cp(0.5, 0.5);
        writefln("cpp.ut.assert r=0.5 s=0.5 x=%.17g y=%.17g z=%.17g str=%s", p.x, p.y, p.z,
                 to!string(p));
        writefln("cpp.ut.assert approx=%s", approxEqualVectors(p, Vector3(0.5, 0.5)));
        p = cp(1.0, 1.0);
        writefln("cpp.ut.assert r=1 s=1 x=%.17g y=%.17g z=%.17g str=%s", p.x, p.y, p.z,
                 to!string(p));
        writefln("cpp.ut.assert approx11=%s", approxEqualVectors(p, Vector3(1.0, 1.0)));
    }
    writefln("cpp.ut str.as.base=%s", (cast(ParametricSurface)cp).toString());
    auto dp = cp.dup();
    dump_state("cpp.dup", dp);
    dump_ctrl_pts("cpp.dup", dp, 4, 5);
    dump_sweep("cpp.dup", dp);
    dump_area("cpp.dup", dp, 10, 10);
    auto cpc = new ControlPointPatch(cp);
    dump_state("cpp.copy", cpc);
    dump_ctrl_pts("cpp.copy", cpc, 4, 5);
    dump_sweep("cpp.copy", cpc);
    dump_area("cpp.copy", cpc, 10, 10);

    // D:52 mC[i] = C[i].dup() — deep copy. Mutate the CALLER's grid afterwards.
    C[2][2] = Vector3(7.0, 7.0, 7.0);
    dump_ctrl_pts("cpp.aftermut", cp, 4, 5);
    writefln("cpp.aftermut callerC x=%.17g y=%.17g z=%.17g", C[2][2].x, C[2][2].y, C[2][2].z);
    // and the other direction: setCtrlPt must not move the caller's array
    cp.setCtrlPt(1, 3, Vector3(-4.0, -5.0, -6.0));
    dump_ctrl_pts("cpp.afterset", cp, 4, 5);
    writefln("cpp.afterset callerC x=%.17g y=%.17g z=%.17g", C[1][3].x, C[1][3].y, C[1][3].z);
    cp.setCtrlPt(1, 3, Vector3(0.25, 0.5, 0.0));  // restore for the groups below

    //--- group B: the VTK-XML writer ------------------------------------------
    cp.writeCtrlPtsAsVtkXml("/tmp/probe_cpp_ut");
    writefln("cpp.vtk.ut size=%d body=%s",
             cast(int)(cast(string)read("/tmp/probe_cpp_ut.vts")).length,
             one_line("/tmp/probe_cpp_ut.vts"));
    {
        auto g = new ControlPointPatch(south, north, west, east, 4, 5, "coons");
        g.writeCtrlPtsAsVtkXml("/tmp/probe_cpp_coons");
        writefln("cpp.vtk.coons body=%s", one_line("/tmp/probe_cpp_coons.vts"));
        auto s1 = new ControlPointPatch(south, north, west, east, 1, 1, "coons");
        s1.writeCtrlPtsAsVtkXml("/tmp/probe_cpp_one");
        writefln("cpp.vtk.one body=%s", one_line("/tmp/probe_cpp_one.vts"));
    }

    //--- group C: every guide-patch branch, fully dumped ----------------------
    {
        auto gc = new ControlPointPatch(south, north, west, east, 4, 5, "coons");
        dump_state("cpp.gcoons", gc);
        dump_ctrl_pts("cpp.gcoons", gc, 4, 5);
        dump_sweep("cpp.gcoons", gc);
        dump_area("cpp.gcoons", gc, 10, 10);
        dump_area("cpp.gcoons", gc, 3, 7);
        dump_area("cpp.gcoons", gc, 1, 1);

        auto gch = new ControlPointPatch(south, north, west, east, 4, 5, "channel");
        dump_state("cpp.gchannel", gch);
        dump_ctrl_pts("cpp.gchannel", gch, 4, 5);
        dump_sweep("cpp.gchannel", gch);
        dump_area("cpp.gchannel", gch, 10, 10);
        dump_area("cpp.gchannel", gch, 3, 7);

        auto gew = new ControlPointPatch(south, north, west, east, 4, 5, "channel-e2w");
        dump_state("cpp.ge2w", gew);
        dump_ctrl_pts("cpp.ge2w", gew, 4, 5);
        dump_sweep("cpp.ge2w", gew);
        dump_area("cpp.ge2w", gew, 10, 10);
        dump_area("cpp.ge2w", gew, 3, 7);

        auto gao = new ControlPointPatch(south, north, west, east, 4, 5, "aopatch");
        dump_state("cpp.gaopatch", gao);
        dump_ctrl_pts("cpp.gaopatch", gao, 4, 5);
        dump_sweep("cpp.gaopatch", gao);
        dump_area("cpp.gaopatch", gao, 10, 10);
    }

    //--- group D: every accepted spelling, lightly ----------------------------
    // A dropped alias would fall through to the default and throw, so each alias only
    // needs its identity rows plus one sampled control point and one surface value.
    foreach (name; ["Coons", "COONS", "tfi", "TFI", "Channel", "CHANNEL", "Channel-e2w",
                    "CHANNEL-E2W", "channel_e2w", "AOPatch", "AOPATCH", "area-orthogonal",
                    "area_orthogonal"]) {
        string tag = "cpp.alias." ~ name;
        dump_throw(tag, () {
            auto g = new ControlPointPatch(south, north, west, east, 4, 5, name);
            writefln("%s str=%s", tag, g.toString());
            int nci = -999, ncj = -999;
            g.nCtrlPts(nci, ncj);
            writefln("%s ncpi=%d ncpj=%d", tag, nci, ncj);
            auto p0 = g.getCtrlPt(1, 2);
            writefln("%s C[1][2] x=%.17g y=%.17g z=%.17g", tag, p0.x, p0.y, p0.z);
            auto q = g(0.5, 0.25);
            writefln("%s opCall x=%.17g y=%.17g z=%.17g", tag, q.x, q.y, q.z);
        });
    }
    // the default branch, D:97-102.
    foreach (bad; ["", "bogus", "CoonsPatch", "channel-e2", "e2w-channel", "AOpatch",
                   "channel_e2w_x", "area orthogonal", "area-orthogonol"]) {
        string tag = "cpp.bad." ~ (bad.length == 0 ? "empty" : bad);
        dump_throw(tag, () {
            new ControlPointPatch(south, north, west, east, 4, 5, bad);
        });
    }

    //--- group E: the optional-boundary cases ---------------------------------
    {
        // no boundaries at all: opCall's four `if` arms all skip and only T() runs.
        auto nb = new ControlPointPatch(cast(Path)null, cast(Path)null, cast(Path)null,
                                       cast(Path)null, C);
        dump_state("cpp.nonull", nb);
        dump_sweep("cpp.nonull", nb);
        dump_area("cpp.nonull", nb, 10, 10);
        auto nbd = nb.dup();
        dump_state("cpp.nonull.dup", nbd);
        dump_sweep("cpp.nonull.dup", nbd);
        // one boundary missing at a time
        auto w0 = new ControlPointPatch(south, north, cast(Path)null, east, C);
        dump_state("cpp.no.west", w0);
        dump_sweep("cpp.no.west", w0);
        auto e0 = new ControlPointPatch(south, north, west, cast(Path)null, C);
        dump_state("cpp.no.east", e0);
        dump_sweep("cpp.no.east", e0);
        auto s0 = new ControlPointPatch(cast(Path)null, north, west, east, C);
        dump_state("cpp.no.south", s0);
        dump_sweep("cpp.no.south", s0);
        auto n0 = new ControlPointPatch(south, cast(Path)null, west, east, C);
        dump_state("cpp.no.north", n0);
        dump_sweep("cpp.no.north", n0);
    }

    //--- group F: control-point counts and the degenerate ones ----------------
    Vector3[][] ragged;
    ragged.length = 3;
    ragged[0].length = 4;
    ragged[1].length = 2;
    ragged[2].length = 4;
    foreach (i; 0 .. 3) foreach (j; 0 .. ragged[i].length) {
        ragged[i][j] = Vector3(cast(double)i + 0.5 * j, cast(double)j, 0.0);
    }
    {
        auto rg = new ControlPointPatch(south, north, west, east, ragged);
        dump_state("cpp.ragged", rg);
        // Row 1 only has 2 entries, so the walk off the end of that row is itself an
        // observable: the rows printed before the throw are part of the corpus.
        dump_throw("cpp.ragged.pts", () { dump_ctrl_pts("cpp.ragged", rg, 3, 4); });
        dump_throw("cpp.ragged.sweep", () {
            foreach (r; [0.0, 0.5, 1.0]) foreach (s; [0.0, 0.5, 1.0]) {
                auto p = rg(r, s);
                writefln("cpp.ragged interior r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g", r, s,
                         p.x, p.y, p.z);
            }
        });
    }
    {
        // 1x1 and 2x2 grids: mN-2 and mM-2 are -1 and 0, so r_hat runs backwards and
        // s_hat stands still.
        Vector3[][] one;
        one.length = 1;
        one[0].length = 1;
        one[0][0] = Vector3(3.0, 4.0, 5.0);
        auto p1 = new ControlPointPatch(south, north, west, east, one);
        dump_state("cpp.n1m1", p1);
        dump_sweep("cpp.n1m1", p1);
        dump_area("cpp.n1m1", p1, 1, 1);

        Vector3[][] two;
        two.length = 2;
        foreach (i; 0 .. 2) { two[i].length = 2; }
        two[0][0] = Vector3(0.0, 0.0, 0.0);
        two[1][0] = Vector3(1.0, 0.0, 0.0);
        two[0][1] = Vector3(0.0, 1.0, 0.0);
        two[1][1] = Vector3(1.0, 1.0, 0.0);
        auto p2 = new ControlPointPatch(south, north, west, east, two);
        dump_state("cpp.n2m2", p2);
        dump_sweep("cpp.n2m2", p2);
        dump_area("cpp.n2m2", p2, 3, 7);
    }
    {
        // the guide constructor's ncpi/ncpj edges: 1, 2 (dr = 1/0) and 0 (C[0] index error)
        dump_state("cpp.g1", new ControlPointPatch(south, north, west, east, 1, 1, "coons"));
        auto g1 = new ControlPointPatch(south, north, west, east, 1, 1, "coons");
        dump_ctrl_pts("cpp.g1", g1, 1, 1);
        dump_sweep("cpp.g1", g1);
        auto g2 = new ControlPointPatch(south, north, west, east, 2, 3, "coons");
        dump_state("cpp.g2", g2);
        dump_ctrl_pts("cpp.g2", g2, 2, 3);
        dump_sweep("cpp.g2", g2);
        dump_throw("cpp.g0", () {
            new ControlPointPatch(south, north, west, east, 0, 3, "coons");
        });
        dump_throw("cpp.gneg", () {
            new ControlPointPatch(south, north, west, east, -1, 3, "coons");
        });
        dump_throw("cpp.gnegj", () {
            new ControlPointPatch(south, north, west, east, 3, -1, "coons");
        });
        // an empty grid through the 5-arg constructor: mM = C[0].length indexes row 0.
        Vector3[][] empty;
        dump_throw("cpp.empty.grid", () { new ControlPointPatch(south, north, west, east, empty); });
    }

    //--- group G: getCtrlPt / setCtrlPt / nCtrlPts out-of-range ---------------
    dump_throw("cpp.get.oob", () { cp.getCtrlPt(4, 0); });
    dump_throw("cpp.get.neg", () { cp.getCtrlPt(-1, 0); });
    dump_throw("cpp.get.oob2", () { cp.getCtrlPt(0, 5); });
    dump_throw("cpp.set.oob", () { cp.setCtrlPt(0, 5, Vector3(1.0, 1.0, 1.0)); });
    // An unwritable directory: std.stdio.File raises, and the port has no ErrnoException,
    // so this row records which type and text actually escape (see parametricsurface.cpp's
    // equivalent note for write_surface_as_vtk_xml).
    dump_throw("cpp.vtk.badpath", () { cp.writeCtrlPtsAsVtkXml("/no_such_dir_xyz/out"); });

    //--- group H: out-of-range (r,s), which walks Omega's r < -1 and r > 1 arms
    {
        foreach (r; [-1.0, -0.5, 1.5, 2.0]) {
            foreach (s; [-1.0, -0.5, 1.5, 2.0]) {
                auto p = cp(r, s);
                writefln("cpp.oor r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g", r, s, p.x, p.y, p.z);
            }
        }
        // NaN and inf parameters
        foreach (r; [double.nan, double.infinity, -double.infinity]) {
            auto p = cp(r, 0.5);
            writefln("cpp.naninf r=%.17g x=%.17g y=%.17g z=%.17g", r, p.x, p.y, p.z);
        }
    }
}
