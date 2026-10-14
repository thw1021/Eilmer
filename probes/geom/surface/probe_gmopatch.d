// probes/geom/surface/probe_gmopatch.d — what geom/surface/gmopatch.d produces.
//
// Reference file captured against: gdtk/src/geom/surface/gmopatch.d, 171 lines,
//   sha256 = ba95d67bf561b72e5efd63eeb3fe3ca56ddd7e4a67d93a472d8eb5e134e8758f
// (gmopatch.d contains no @nogc, so the shadow copy below is byte-identical to the
// gdtk file and this sha is the sha of what was actually executed; it is also
// emitted as row 1 of the corpus, so the corpus states its own revision).
//
// gmopatch.d HAS one `unittest` block (D:150-171), and its non-complex branch is what
// runs (this capture does not define version=complex_numbers, per backlog T-904). This
// probe does NOT compile with -unittest: it replays that block's statements in source
// order instead (group A below, rows gmo.ut.*), so the two `writeln("c=", c)` lines are
// carried as data with their exact bytes and both asserts are carried as booleans the
// port has to match. That is the convention every other probe in probes/geom/surface/
// uses (see probe_coonspatch.d, probe_aopatch.d).
//
// Build and capture (reproducible; the gdtk tree itself is read-only):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface \
//     probes/geom/surface/probe_gmopatch.d -of=/tmp/probe_g6b_gmo -od/tmp/obj_g6b
//   /tmp/probe_g6b_gmo > baselines/e587b235/probes/geom_surface_gmopatch.txt
// No -O flag: every baseline in this repository is an ldc2 -O0 capture. @nogc is stripped
// for ldc2 1.40 compatibility with `throw new ...` in @nogc members elsewhere in gdtk
// (geom/grid/sgrid.d:217 and friends); it has no runtime effect and the C++ port has no
// @nogc to carry.
// REPRODUCIBILITY GATE run before this corpus was committed: the capture was executed
// twice into two files, `cmp` said byte-identical, and `grep -cE '0x[0-9a-f]{7,}'` over
// the corpus is 0. Nothing here prints an address or a wall-clock: construction cost is
// not measured, only the geometry. The whole corpus is deterministic because the
// redistribution is a Nelder-Mead run with a FIXED initial simplex
// (sgrid.d:1410 `ddata ~= 0.05`, no randomness anywhere in nm/nelmin.d) — measured, not
// assumed: two runs of the same binary agree on all 32 redistributed parameters.
//
// Why this module needs a corpus of its own rather than inheriting grid's: GMOPatch is
// the only surface that mutates its OWN members through a StructuredGrid call. The
// constructor (D:45-89) builds a CoonsPatch over the four boundaries, makes a
// StructuredGrid test grid over it with the uniform 0, 1/3, 2/3, 1 parameter grids
// (D:26-33), and calls my_grid.determine_rs_grids(my_patch, cf, r_grid, s_grid), which
// takes r_grid and s_grid BY REF (sgrid.d:1396 `ref double[4][4]`) and leaves the
// optimised values behind. Everything observable about the patch after construction is
// those 32 numbers plus the four corners, and opCall (D:110-135) re-derives a
// tensor-product cubic-Bezier remap from them. So the groups are:
//
//   gmo.src         provenance row: D file, sha256, line count, probe, compiler.
//   gmo.ut.*        gmopatch.d's unittest (D:150-171) replayed statement by statement,
//                   plus the full member dump and (r,s) sweep of the same patch.
//                   The two assert outcomes are rows; the two writeln lines are carried
//                   with their exact bytes ("c=Vector3(0.476931, 0.646263, 3)").
//   gmo.def.*       the default arguments niv=11, njv=11 (D:36, D:46): construction with
//                   the sizes omitted, compared against gmo.ut.
//   gmo.size55/33/22/2113
//                   the test-grid size is an INPUT to the optimiser, so the redistributed
//                   parameters depend on it. These four groups pin that dependency (a
//                   port that optimises once, or ignores niv/njv, cannot reproduce them).
//   gmo.skew.*      a non-planar corner set, so area() has all three components and the
//                   redistribution has no symmetry to hide behind.
//   gmo.bezier.*    the four-Path constructor (D:45-89) with curved boundaries, including
//                   the aliasing check (D:52-55 dups each Path, so the caller's later
//                   mutation must not move the patch's boundary) and the boundary pairing
//                   of the corner constructor (D:38-41) compared against it.
//   gmo.lines.*     the same four boundaries handed over explicitly: `same_as_gmo.ut`
//                   rows say whether the two constructors agree, which is the only way to
//                   catch a swapped north/south/west/east parameter list (D:45-46 keeps
//                   the original paper's order, NOT the NESW order used elsewhere).
//   gmo.open.*      the four `throw new Error(text("GMOPatch open corner ..."))` guards
//                   (D:65-80) in check order — p00, p10, p11, p01 — one corner open at a
//                   time, plus a patch with all four open, which must still report p00.
//                   A bare new Error carries NO site (T-922), so file=[] line=0 is part
//                   of what these rows pin.
//   gmo.tol.*       approxEqualVectors' default tolerances (vector3.d, maxRelDiff 1.0e-2
//                   / maxAbsDiff 1.0e-5) decide what counts as open: 1.0e-6 passes,
//                   1.0e-3 fails on abs, 5.0e-3 at a magnitude-1 corner passes on rel,
//                   1.5e-2 fails both.
//   gmo.nan.*       the corner guards with default-constructed (NaN) corners: NaN != NaN,
//                   so it throws and the message prints Vector3(nan, nan, nan).
//   gmo.copy.*      D:91-103, the copy constructor: NO re-optimisation, r_grid/s_grid
//                   copied by value (D:101-102 `r_grid[][] = other.r_grid[][]`), corners
//                   copied verbatim, boundaries duped. Followed by the aliasing check:
//                   after mutating the source's r_grid the copy must not move.
//   gmo.dup.*       D:105-108, dup() re-enters the CHECKING constructor with the DEFAULT
//                   11x11 test grid, so a dup of a 5x5-built patch must equal gmo.ut, not
//                   gmo.size55. Those two booleans are the whole point of this group.
//   gmo.oor.*       opCall outside [0,1] — the remap is a polynomial, so it extrapolates;
//                   and with NaN / +-Infinity parameters, which reaches the signed-NaN
//                   behaviour of the SSE default QNaN (0/0 style) in both toolchains.
//   gmo.bad.*       the sizes a caller can hand in that the module cannot serve. These
//                   are throw sites of the CONSTRUCTION path (they come from sgrid.d
//                   through D:87-88), and they are the reason niv/njv are not silently
//                   clamped anywhere: njv==1 and njv==0 die in measure_of_badness with a
//                   bare object.Error "Not implemented." (sgrid.d:1391), and niv<=1 dies
//                   with core.exception.AssertError "index i is invalid" at grid.d:73
//                   while measure_of_badness reaches for this[i-1,j] (sgrid.d:1336).
//   gmo.point.*     four coincident corners: the optimiser runs on a degenerate grid and
//                   leaves particular values (its objective is NaN there), which is a
//                   deterministic observable and is captured rather than reasoned about.
//
// The `version (complex_numbers)` branch of gmopatch.d's unittest (D:153-157) calls
// util.test_runner's skip() and is not compiled (backlog T-904), so no row here depends
// on it; note D:154-156's own comment says the GMO patch is INCOMPATIBLE with complex
// numbers, which is why the non-complex branch is the reference.
import std.algorithm;
import std.array;
import std.conv;
import std.format;
import std.math;
import std.stdio;
import std.string;

import geom.elements;
import geom.gpath;
import geom.grid.sgrid;
import geom.misc.univariatefunctions;
import geom.surface.coonspatch;
import geom.surface.gmopatch;
import geom.surface.parametricsurface;
import nm.number;

void dump_throw(const char[] tag, void delegate() dg) {
    try {
        dg();
        writefln("%s|class=none|msg=[]|file=[]|line=-1", tag);
    } catch (Throwable t) {
        // class/msg/file/line only. Throwable.toString() is NOT used: its first line
        // prefixes a mangled class name and it appends a backtrace of addresses.
        writefln("%s|class=%s|msg=[%s]|file=[%s]|line=%d", tag, t.classinfo.name, t.msg, t.file,
                 t.line);
    }
}

/// The members of a GMOPatch, in the order D declares them (D:22-33), plus the two
/// redistributed parameter grids at full precision.
void dump_patch(const char[] tag, GMOPatch gp) {
    writefln("%s str=%s", tag, gp.toString());
    writefln("%s classname=%s", tag, split(to!string(typeid(gp).name), ".")[$-1]);
    foreach (name; ["p00", "p10", "p11", "p01"]) {
        Vector3 c = (name == "p00") ? gp.p00 : (name == "p10") ? gp.p10
                  : (name == "p11") ? gp.p11 : gp.p01;
        writefln("%s %s x=%.17g y=%.17g z=%.17g", tag, name, c.x, c.y, c.z);
    }
    foreach (name; ["south", "north", "west", "east"]) {
        Path b = (name == "south") ? gp.south : (name == "north") ? gp.north
               : (name == "west") ? gp.west : gp.east;
        writefln("%s %s class=%s str=%s", tag, name, b.classString(), b.toString());
        foreach (t; [0.0, 0.5, 1.0]) {
            auto p = b(t);
            writefln("%s %s t=%.17g x=%.17g y=%.17g z=%.17g", tag, name, t, p.x, p.y, p.z);
        }
    }
    foreach (i; 0 .. 4) {
        writefln("%s r_grid[%d] a=%.17g b=%.17g c=%.17g d=%.17g", tag, i, gp.r_grid[i][0],
                 gp.r_grid[i][1], gp.r_grid[i][2], gp.r_grid[i][3]);
    }
    foreach (i; 0 .. 4) {
        writefln("%s s_grid[%d] a=%.17g b=%.17g c=%.17g d=%.17g", tag, i, gp.s_grid[i][0],
                 gp.s_grid[i][1], gp.s_grid[i][2], gp.s_grid[i][3]);
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

/// A shorter (r,s) probe for the groups whose observable is the parameter grids.
void dump_probe(const char[] tag, ParametricSurface ps) {
    foreach (q; [[0.5, 0.5], [0.1, 0.1], [0.25, 0.75], [0.9, 0.2]]) {
        auto p = ps(q[0], q[1]);
        writefln("%s opCall r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g", tag, q[0], q[1], p.x, p.y,
                 p.z);
    }
}

void dump_area(const char[] tag, ParametricSurface ps, int nr, int ns) {
    auto a = ps.area(nr, ns);
    writefln("%s area nr=%d ns=%d x=%.17g y=%.17g z=%.17g", tag, nr, ns, a.x, a.y, a.z);
}

/// Point-wise equality of two surfaces over the parameters that matter here. Because the
/// comparison is on the printed bits, it says exactly what the corpus reader needs: the
/// port's values are the same numbers D printed.
string pts_eq(const char[] tag, ParametricSurface a, ParametricSurface b) {
    bool same = true;
    foreach (r; [0.0, 0.2, 0.5, 0.75, 1.0]) {
        foreach (s; [0.0, 0.25, 0.5, 0.75, 1.0]) {
            auto pa = a(r, s);
            auto pb = b(r, s);
            // format() compares the printed bits, so -0.0 and 0.0 are told apart only
            // where the format itself tells them apart; NaN signs are part of the text.
            if (format("%.17g %.17g %.17g", pa.x, pa.y, pa.z)
                != format("%.17g %.17g %.17g", pb.x, pb.y, pb.z)) {
                same = false;
            }
        }
    }
    return format("%s same=%s", tag, same);
}

/// Equality of the two redistributed grids, at full precision.
bool grids_eq(GMOPatch a, GMOPatch b) {
    foreach (i; 0 .. 4) {
        foreach (j; 0 .. 4) {
            if (format("%.17g", a.r_grid[i][j]) != format("%.17g", b.r_grid[i][j])) return false;
            if (format("%.17g", a.s_grid[i][j]) != format("%.17g", b.s_grid[i][j])) return false;
        }
    }
    return true;
}

CoonsPatch coons_of(Vector3 p00, Vector3 p10, Vector3 p11, Vector3 p01) {
    return new CoonsPatch(p00, p10, p11, p01);
}

StructuredGrid test_grid(ParametricSurface surf, size_t niv, size_t njv) {
    auto cf = [new LinearFunction(), new LinearFunction(), new LinearFunction(),
               new LinearFunction()];
    return new StructuredGrid(surf, niv, njv, cf);
}

void main()
{
    //--- group 0: provenance --------------------------------------------------
    writefln("gmo.src|dfile=geom/surface/gmopatch.d|dlines=%d|sha256=%s|probe=probes/geom/surface/probe_gmopatch.d|cc=ldc2 1.40.0 -O0 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip|unittest=replayed-not-compiled",
             171, "ba95d67bf561b72e5efd63eeb3fe3ca56ddd7e4a67d93a472d8eb5e134e8758f");

    //--- group A: gmopatch.d's own unittest (D:150-171), in source order -------
    auto p00 = Vector3([0.0, 0.1, 3.0]);
    auto p10 = Vector3(1.0, 0.1, 3.0);
    auto p11 = Vector3(1.0, 1.1, 3.0);
    auto p01 = Vector3(0.0, 1.1, 3.0);
    auto ut = new GMOPatch(p00, p10, p11, p01, 11, 11);
    auto c1 = ut(0.5, 0.5);
    writefln("gmo.ut.1 writeln=%s", "c=" ~ to!string(c1));
    writefln("gmo.ut.1 full x=%.17g y=%.17g z=%.17g", c1.x, c1.y, c1.z);
    writefln("gmo.ut.1 assert_holds=%s", approxEqualVectors(c1, Vector3(0.477, 0.646, 3.0), 1.0e-1));
    auto c2 = ut(0.1, 0.1);
    writefln("gmo.ut.2 writeln=%s", "c=" ~ to!string(c2));
    writefln("gmo.ut.2 full x=%.17g y=%.17g z=%.17g", c2.x, c2.y, c2.z);
    writefln("gmo.ut.2 assert_holds=%s", approxEqualVectors(c2, Vector3(0.117, 0.235, 3.0), 1.0e-1));
    // the same patch, fully dumped
    dump_patch("gmo.ut", ut);
    dump_sweep("gmo.ut", ut);
    dump_area("gmo.ut", ut, 10, 10);
    dump_area("gmo.ut", ut, 3, 7);

    //--- group B: the default test-grid sizes (D:36, D:46 niv=11, njv=11) ------
    auto dflt = new GMOPatch(p00, p10, p11, p01);
    writefln("gmo.def str_eq_ut=%s", dflt.toString() == ut.toString());
    writefln("gmo.def grids_eq_ut=%s", grids_eq(dflt, ut));
    writefln("%s", pts_eq("gmo.def", dflt, ut));

    //--- group C: the test-grid size is an optimiser input ---------------------
    auto s55 = new GMOPatch(p00, p10, p11, p01, 5, 5);
    dump_patch("gmo.size55", s55);
    dump_probe("gmo.size55", s55);
    auto s33 = new GMOPatch(p00, p10, p11, p01, 3, 3);
    dump_patch("gmo.size33", s33);
    dump_probe("gmo.size33", s33);
    auto s22 = new GMOPatch(p00, p10, p11, p01, 2, 2);
    dump_patch("gmo.size22", s22);
    dump_probe("gmo.size22", s22);
    auto s2113 = new GMOPatch(p00, p10, p11, p01, 21, 13);
    dump_patch("gmo.size2113", s2113);
    dump_probe("gmo.size2113", s2113);
    writefln("gmo.size grids_differ_55_vs_11=%s", !grids_eq(s55, ut));
    writefln("gmo.size grids_differ_33_vs_22=%s", !grids_eq(s33, s22));
    writefln("gmo.size grids_differ_2113_vs_11=%s", !grids_eq(s2113, ut));

    //--- group D: a non-planar, sheared corner set ----------------------------
    auto q00 = Vector3(0.5, -0.25, 0.1);
    auto q10 = Vector3(2.0, 0.75, -0.3);
    auto q11 = Vector3(1.25, 2.5, 0.05);
    auto q01 = Vector3(-0.5, 1.5, 0.9);
    auto sk = new GMOPatch(q00, q10, q11, q01, 9, 7);
    dump_patch("gmo.skew", sk);
    dump_sweep("gmo.skew", sk);
    dump_area("gmo.skew", sk, 10, 10);
    dump_area("gmo.skew", sk, 3, 7);

    //--- group E: the four-Path constructor with curved boundaries -------------
    Path bs = new Bezier([Vector3(0.0, 0.0, 0.0), Vector3(0.33, -0.2, 0.1),
                          Vector3(0.67, -0.2, 0.1), Vector3(1.0, 0.0, 0.0)]);
    Path bn = new Bezier([Vector3(0.0, 1.0, 0.0), Vector3(0.33, 1.2, 0.1),
                          Vector3(0.67, 1.2, 0.1), Vector3(1.0, 1.0, 0.0)]);
    Path bw = new Bezier([Vector3(0.0, 0.0, 0.0), Vector3(-0.15, 0.33, 0.1),
                          Vector3(-0.15, 0.67, 0.1), Vector3(0.0, 1.0, 0.0)]);
    Path be = new Bezier([Vector3(1.0, 0.0, 0.0), Vector3(1.15, 0.33, 0.1),
                          Vector3(1.15, 0.67, 0.1), Vector3(1.0, 1.0, 0.0)]);
    auto bz = new GMOPatch(bs, bn, bw, be, 5, 5);
    dump_patch("gmo.bezier", bz);
    dump_sweep("gmo.bezier", bz);
    dump_area("gmo.bezier", bz, 10, 10);
    // D:52-55 dups every boundary, so the caller's later mutation must not move the
    // patch's own boundary. The patch is dumped again after bs is changed under foot.
    (cast(Bezier)bs).B[1] = Vector3(0.33, -0.2, 5.0);
    dump_patch("gmo.bezier.aftermut", bz);
    writefln("gmo.bezier.callerstr str=%s", bs.toString());

    //--- group F: the same boundaries handed over explicitly -------------------
    Path ls = new Line(p00, p10);
    Path ln = new Line(p01, p11);
    Path lw = new Line(p00, p01);
    Path le = new Line(p10, p11);
    auto lp = new GMOPatch(ls, ln, lw, le, 11, 11);
    writefln("gmo.lines str_eq_ut=%s", lp.toString() == ut.toString());
    writefln("gmo.lines grids_eq_ut=%s", grids_eq(lp, ut));
    writefln("%s", pts_eq("gmo.lines", lp, ut));
    dump_patch("gmo.lines", lp);

    //--- group G: the four open-corner guards, in D's check order --------------
    // south/north run along r; west/east run along s. One corner open at a time.
    {   // p00 open: south(0) != west(0)
        Path s2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
        Path n2 = new Line(Vector3(0.0, 1.0, 0.0), Vector3(1.0, 1.0, 0.0));
        Path w2 = new Line(Vector3(0.1, 0.0, 0.0), Vector3(0.1, 1.0, 0.0));
        Path e2 = new Line(Vector3(1.0, 0.0, 0.0), Vector3(1.0, 1.0, 0.0));
        dump_throw("gmo.open.p00", () { new GMOPatch(s2, n2, w2, e2); });
        // and the same geometry through the corner constructor
        dump_throw("gmo.open.p00.corners",
                   () { new GMOPatch(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0),
                                     Vector3(1.0, 1.0, 0.0), Vector3(0.1, 1.0, 0.0)); });
    }
    {   // p10 open: south(1) != east(0)
        Path s2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.1, 0.0));
        Path n2 = new Line(Vector3(0.0, 1.0, 0.0), Vector3(1.0, 1.0, 0.0));
        Path w2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(0.0, 1.0, 0.0));
        Path e2 = new Line(Vector3(1.0, 0.0, 0.0), Vector3(1.0, 1.0, 0.0));
        dump_throw("gmo.open.p10", () { new GMOPatch(s2, n2, w2, e2); });
    }
    {   // p11 open: north(1) != east(1)
        Path s2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
        Path n2 = new Line(Vector3(0.0, 1.0, 0.0), Vector3(1.0, 1.1, 0.0));
        Path w2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(0.0, 1.0, 0.0));
        Path e2 = new Line(Vector3(1.0, 0.0, 0.0), Vector3(1.0, 1.0, 0.0));
        dump_throw("gmo.open.p11", () { new GMOPatch(s2, n2, w2, e2); });
    }
    {   // p01 open: north(0) != west(1)
        Path s2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
        Path n2 = new Line(Vector3(0.1, 1.0, 0.0), Vector3(1.0, 1.0, 0.0));
        Path w2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(0.0, 1.0, 0.0));
        Path e2 = new Line(Vector3(1.0, 0.0, 0.0), Vector3(1.0, 1.0, 0.0));
        dump_throw("gmo.open.p01", () { new GMOPatch(s2, n2, w2, e2); });
    }
    {   // all four open: still the p00 message, because the guards run in order
        Path s2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.1, 0.0));
        Path n2 = new Line(Vector3(0.1, 1.1, 0.0), Vector3(1.0, 1.0, 0.0));
        Path w2 = new Line(Vector3(0.2, 0.0, 0.0), Vector3(0.0, 1.2, 0.0));
        Path e2 = new Line(Vector3(1.0, 0.3, 0.0), Vector3(0.9, 1.0, 0.0));
        dump_throw("gmo.open.all", () { new GMOPatch(s2, n2, w2, e2); });
    }
    {   // the tolerance boundary of approxEqualVectors, at a magnitude-1 corner
        foreach (eps; ["1e-6", "1e-3", "5e-3", "1.5e-2"]) {
            double d = to!double(eps);
            Path s2 = new Line(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
            Path n2 = new Line(Vector3(0.0, 1.0, 0.0), Vector3(1.0, 1.0, 0.0));
            Path w2 = new Line(Vector3(d, 0.0, 0.0), Vector3(0.0, 1.0, 0.0));
            Path e2 = new Line(Vector3(1.0, 0.0, 0.0), Vector3(1.0, 1.0, 0.0));
            dump_throw("gmo.tol." ~ eps, () { new GMOPatch(s2, n2, w2, e2); });
        }
    }

    //--- group H: default-constructed (NaN) corners ---------------------------
    dump_throw("gmo.nan.corners",
               () { new GMOPatch(Vector3(), Vector3(), Vector3(), Vector3()); });

    //--- group I: the copy constructor (D:91-103) -----------------------------
    auto cp = new GMOPatch(s55);
    dump_patch("gmo.copy", cp);
    dump_probe("gmo.copy", cp);
    writefln("gmo.copy grids_eq_source=%s", grids_eq(cp, s55));
    writefln("gmo.copy str_eq_source=%s", cp.toString() == s55.toString());
    writefln("%s", pts_eq("gmo.copy", cp, s55));
    // D:101-102 copy the grids BY VALUE (static arrays), and D:93-96 dup the paths.
    s55.r_grid[1][2] = 0.25;
    s55.p11 = Vector3(7.0, 7.0, 7.0);
    writefln("gmo.indep mutated=r_grid[1][2],p11");
    writefln("gmo.indep copy r_grid[1] a=%.17g b=%.17g c=%.17g d=%.17g", cp.r_grid[1][0],
             cp.r_grid[1][1], cp.r_grid[1][2], cp.r_grid[1][3]);
    writefln("gmo.indep copy p11 x=%.17g y=%.17g z=%.17g", cp.p11.x, cp.p11.y, cp.p11.z);
    writefln("gmo.indep source r_grid[1] a=%.17g b=%.17g c=%.17g d=%.17g", s55.r_grid[1][0],
             s55.r_grid[1][1], s55.r_grid[1][2], s55.r_grid[1][3]);

    //--- group J: dup() (D:105-108) re-optimises at the DEFAULT sizes ----------
    auto dp = cast(GMOPatch)ut.dup();
    writefln("gmo.dup str_eq_ut=%s", dp.toString() == ut.toString());
    writefln("gmo.dup grids_eq_ut=%s", grids_eq(dp, ut));
    writefln("%s", pts_eq("gmo.dup", dp, ut));
    auto dd22 = cast(GMOPatch)s22.dup();
    auto src55 = new GMOPatch(p00, p10, p11, p01, 5, 5);
    auto dd55 = cast(GMOPatch)src55.dup();
    writefln("gmo.dup55 str=%s", dd55.toString());
    writefln("gmo.dup55 grids_eq_ut=%s", grids_eq(dd55, ut));
    writefln("gmo.dup55 grids_eq_src55=%s", grids_eq(dd55, src55));
    writefln("%s", pts_eq("gmo.dup55", dd55, ut));
    writefln("gmo.dup22 grids_eq_ut=%s", grids_eq(dd22, ut));

    //--- group K: opCall outside the unit square and with non-finite arguments -
    foreach (q; [[-0.5, 1.5], [2.0, 0.0], [-1.0, -1.0], [0.5, 1.0e10]]) {
        auto p = ut(q[0], q[1]);
        writefln("gmo.oor r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g", q[0], q[1], p.x, p.y, p.z);
    }
    foreach (i, q; [[double.nan, 0.5], [0.5, double.nan], [double.infinity, 0.5],
                    [-double.infinity, 0.25], [0.5, double.infinity]]) {
        dump_throw("gmo.nf" ~ to!string(i), () { auto p = ut(q[0], q[1]);
                                                 writefln("gmo.nf%.17g r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g",
                                                          cast(double)i, q[0], q[1], p.x, p.y, p.z); });
    }

    //--- group L: sizes the construction path cannot serve ---------------------
    dump_throw("gmo.bad.njv1", () { new GMOPatch(p00, p10, p11, p01, 5, 1); });
    dump_throw("gmo.bad.njv0", () { new GMOPatch(p00, p10, p11, p01, 5, 0); });
    dump_throw("gmo.bad.niv1", () { new GMOPatch(p00, p10, p11, p01, 1, 5); });
    dump_throw("gmo.bad.niv0", () { new GMOPatch(p00, p10, p11, p01, 0, 5); });
    dump_throw("gmo.bad.niv1njv1", () { new GMOPatch(p00, p10, p11, p01, 1, 1); });

    //--- group M: four coincident corners --------------------------------------
    auto pt = new GMOPatch(Vector3(1.0, 2.0, 3.0), Vector3(1.0, 2.0, 3.0),
                           Vector3(1.0, 2.0, 3.0), Vector3(1.0, 2.0, 3.0), 5, 5);
    dump_patch("gmo.point", pt);
    dump_probe("gmo.point", pt);
    dump_area("gmo.point", pt, 10, 10);
}
