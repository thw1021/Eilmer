// probes/geom/surface/probe_meshpatch.d — what geom/surface/meshpatch.d produces.
//
// Reference file captured against: gdtk/src/geom/surface/meshpatch.d, 100 lines,
//   sha256 = e8418bd6df549a844f9d0a7a8fb69d795211efc74fb76454816acde91b23e906
// (verified against the shadow copy below: meshpatch.d contains no @nogc, so the
// shadow bytes are identical to the gdtk bytes; `sha256sum` of both is the value
// above, and it is also emitted as row 1 of the corpus so the corpus states its
// own revision).
//
// meshpatch.d has NO `unittest` block (`grep -c '^unittest' meshpatch.d` == 0) and is
// not touched by geom/surface/surface_demo.d, so this probe — not a D unittest — is the
// whole reference for the module. Its `version(complex_numbers)` axis is not compiled
// (backlog T-904); `number` is `double`.
//
// Build and capture (reproducible; the gdtk tree itself is read-only):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface \
//     probes/geom/surface/probe_meshpatch.d -of=/tmp/probe_g6b_mp -od/tmp/obj_g6b
//   /tmp/probe_g6b_mp > baselines/e587b235/probes/geom_surface_meshpatch.txt
// No -O flag: every baseline in this repository is an ldc2 -O0 capture. @nogc is stripped
// because ldc2 1.40 rejects `throw new ...` inside @nogc members elsewhere in gdtk
// (e.g. geom/grid/sgrid.d:217); it is a compile-time attribute with no runtime effect.
// REPRODUCIBILITY GATE that was run before this corpus was committed: the capture above
// was executed twice into two files and `cmp` reported them byte-identical, and
// `grep -cE '0x[0-9a-f]{7,}'` over the corpus is 0 (no row carries an address; throw rows
// carry class/msg/file/line only, never Throwable.toString(), whose backtrace is
// address-laden and run-to-run volatile).
//
// What is measured rather than reasoned (row groups, all mp.*):
//   mp.src      the provenance row: D file, sha256, line count, this probe, the ldc2
//               build command. Frozen in the corpus so a replay can prove WHICH revision
//               it was captured against; two agents in this project delivered a corpus 33
//               rows behind its own probe, so the sha is part of the gate, not a comment.
//   mp.v.*      the Vector3[][] constructor (D:41-53): niv = grid.length, njv =
//               grid[0].length, the mesh copied row by row, and the four corners taken
//               from the CORNERS OF THE ARRAY, not from a surface evaluation:
//               p00 = [0][0], p10 = [niv-1][0], p01 = [0][njv-1], p11 = [niv-1][njv-1]
//               (D:49-52). Note njv comes from row 0 only, so a ragged input decides the
//               whole grid's width from row 0 — measured by mp.v.ragged* below.
//   mp.v.empty / mp.v.emptyinner / mp.v.raggedshort
//               what D's un-bounds-checked indexing does at those shapes. D raises
//               core.exception.ArrayIndexError with the shadow-tree file and the D line,
//               which is why throw rows carry file/line: the class name alone would not
//               distinguish meshpatch.d:44 from meshpatch.d:47.
//   mp.v.raggedlong  a second row LONGER than row 0 is silently truncated to njv.
//   mp.grid.*   the StructuredGrid constructor (D:23-39) over a Line-bounded CoonsPatch
//               and a 5x4 test grid: niv/njv come from the grid, mesh[i][j] is *grid[i,j]
//               (D:33), and the corners again come from the array corners.
//   mp.grid.nkv2, mp.slab.nkv2
//               the nkv != 1 guard (D:25-27) — a bare `throw new Error(...)`, which by
//               T-922 carries NO site: the row must show file=[] line=0.
//   mp.blank.*  a blank (never filled) StructuredGrid: D's vertices.length leaves every
//               vertex at Vector3.init, i.e. NaN (wave-02-brief section 5), so the mesh,
//               the corners, opCall and area() are all NaN. This group is the guard
//               against a port that zero-fills the array instead of copying .init.
//   mp.grid1d.* a 1D grid (njv == 1): the constructor succeeds because nkv is 1, and
//               ds = 1.0/(njv-1) is then a division by zero, which only bites in opCall
//               (D:87-90) where mesh[i][j+1] indexes past a row of length 1.
//   mp.niv1.*   the mirror case, niv == 1: dr = 1.0/0 and mesh[i+1] indexes past the
//               mesh. Together with the group above these two rows pin down that
//               D's `i = min(i, niv-2)` (D:81) does NOT wrap: niv-2 is a size_t that
//               wraps to 18446744073709551614, yet the measured index stays 0, so the
//               clamp is signed and the failure is the mesh index, not the clamp.
//   mp.1x1 / mp.grid.niv0 / mp.grid.njv0
//               degenerate lattices. A 1x1 grid passes the nkv guard, holds one NaN
//               vertex, and dies in opCall at D:88; a grid with zero vertices in one
//               direction gets past `mesh.length = niv` and dies at D:35's
//               `p00 = mesh[0][0]` — the row states which line D blamed, which is the
//               only way to tell "the corners are read from the mesh" from a port that
//               derives them from the grid instead.
//   mp.clamp.*  cell selection itself: r/s at exactly 0, at each interior cell edge
//               (i/(niv-1) computed in double), one ULP either side of an edge, just
//               below 0 and above 1, and at 0.5. These are the rows that distinguish
//               `to!int` truncation-toward-zero from rounding or floor(), and that
//               distinguish D's `min/max` clamp order (D:81-82).
//   mp.oc.*     opCall over the normal (r,s) sweep, plus area() at the D defaults and an
//               asymmetric split.
//   mp.nan.*    / mp.inf.*  the two `to!int(...)` sites (D:81, D:82) on non-finite
//               arguments: Phobos raises std.conv.ConvException "Input was NaN" and
//               std.conv.ConvOverflowException "Conversion positive/negative overflow"
//               from std/conv.d. These are the rows that make the missing
//               ConvOverflowException in substrate (backlog T-928) visible instead of
//               silently folded into one class.
//   mp.copy.*   D:55-67 (`this(ref const(MeshPatch) other)`) — copies niv/njv, rebuilds
//               each row by appending, and copies the four corners VERBATIM (D:63-66),
//               so it does not re-derive them.
//   mp.dup.*    D:69-72 — goes back through the Vector3[][] constructor from this.mesh.
//   mp.indep.*  aliasing check: mutate the original's mesh and corners after a copy/dup
//               and re-dump the copy. A missing row-level `~=` (D:61) would show here.
//
// Free functions and base-class members MeshPatch does not override (parametricsurface.d
// :40-66 area()) are exercised through the MeshPatch handle because that is the only way
// the module's own bilinear opCall is reached.
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
import geom.surface.meshpatch;
import geom.surface.parametricsurface;
import nm.number;

/// Every throw row: D's class, message, and the site D itself reports. file/line are
/// recorded because T-922 measured that a bare `new Error` has file == "" and line == 0
/// while AssertError/ArrayIndexError/GeometryException carry the raise site; Throwable
/// .toString() is NOT used (its backtrace carries addresses and changes every run).
void dump_throw(const char[] tag, void delegate() dg) {
    try {
        dg();
        writefln("%s|class=none|msg=[]|file=[]|line=-1", tag);
    } catch (Throwable t) {
        writefln("%s|class=%s|msg=[%s]|file=[%s]|line=%d", tag, t.classinfo.name, t.msg, t.file,
                 t.line);
    }
}

/// The members of a MeshPatch, in the order D declares them (D:18-21).
void dump_patch(const char[] tag, MeshPatch mp) {
    writefln("%s str=%s", tag, mp.toString());
    writefln("%s classname=%s", tag, split(to!string(typeid(mp).name), ".")[$-1]);
    string rl;
    foreach (i, row; mp.mesh) {
        if (i) rl ~= ",";
        rl ~= to!string(row.length);
    }
    writefln("%s niv=%d njv=%d mesh.length=%d rowlen=%s", tag, mp.niv, mp.njv, mp.mesh.length, rl);
    foreach (i; 0 .. mp.mesh.length) {
        foreach (j; 0 .. mp.mesh[i].length) {
            auto p = mp.mesh[i][j];
            writefln("%s mesh[%d][%d] x=%.17g y=%.17g z=%.17g", tag, i, j, p.x, p.y, p.z);
        }
    }
    foreach (name; ["p00", "p10", "p11", "p01"]) {
        Vector3 c = (name == "p00") ? mp.p00 : (name == "p10") ? mp.p10
                  : (name == "p11") ? mp.p11 : mp.p01;
        writefln("%s %s x=%.17g y=%.17g z=%.17g", tag, name, c.x, c.y, c.z);
    }
}

/// The standard (r,s) sweep used by every other geom/surface probe.
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

/// Cell selection, at the edges themselves and one ULP either side of them.
void dump_clamp(const char[] tag, MeshPatch mp) {
    double[] rs = [-0.5, -1.0e-3, 0.0, 1.0 / 3.0, nextafter(1.0 / 3.0, 0.0),
                   nextafter(1.0 / 3.0, 1.0), 0.5, 2.0 / 3.0, nextafter(2.0 / 3.0, 0.0),
                   nextafter(2.0 / 3.0, 1.0), 0.9999999999999999, 1.0, 1.5];
    foreach (r; rs) {
        auto p = mp(r, 0.5);
        writefln("%s r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g", tag, r, 0.5, p.x, p.y, p.z);
    }
    foreach (s; rs) {
        auto p = mp(0.5, s);
        writefln("%s r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g", tag, 0.5, s, p.x, p.y, p.z);
    }
}

/// A deterministic node lattice: every node distinguishable, so a wrong i/j pair cannot
/// hide behind a symmetry.
Vector3[][] mk_mesh(size_t ni, size_t nj) {
    Vector3[][] g;
    g.length = ni;
    foreach (i; 0 .. ni) {
        foreach (j; 0 .. nj) {
            g[i] ~= Vector3(cast(double)(10*i + j), cast(double)(100*i + 10*j),
                            cast(double)(i) + 0.5*cast(double)(j));
        }
    }
    return g;
}

/// A 2D StructuredGrid over the linear identity cluster functions, the way
/// geom/surface/gmopatch.d:85-87 builds its test grid.
StructuredGrid mk_surf_grid(ParametricSurface surf, size_t niv, size_t njv) {
    auto cf = [new LinearFunction(), new LinearFunction(), new LinearFunction(),
               new LinearFunction()];
    return new StructuredGrid(surf, niv, njv, cf);
}

CoonsPatch mk_coons() {
    return new CoonsPatch(Vector3(0.0, 0.1, 3.0), Vector3(1.0, 0.1, 3.0),
                          Vector3(1.0, 1.1, 3.0), Vector3(0.0, 1.1, 3.0));
}

void main()
{
    //--- group 0: provenance --------------------------------------------------
    writefln("mp.src|dfile=geom/surface/meshpatch.d|dlines=%d|sha256=%s|probe=probes/geom/surface/probe_meshpatch.d|cc=ldc2 1.40.0 -O0 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip",
             100, "e8418bd6df549a844f9d0a7a8fb69d795211efc74fb76454816acde91b23e906");

    //--- group A: the Vector3[][] constructor ---------------------------------
    auto vg = mk_mesh(4, 3);
    auto mv = new MeshPatch(vg);
    dump_patch("mp.v", mv);
    dump_sweep("mp.v", mv);
    dump_clamp("mp.v.clamp", mv);
    dump_area("mp.v", mv, 10, 10);
    dump_area("mp.v", mv, 3, 7);

    //--- group B: the StructuredGrid constructor ------------------------------
    auto sg = mk_surf_grid(mk_coons(), 5, 4);
    auto mg = new MeshPatch(sg);
    dump_patch("mp.grid", mg);
    dump_sweep("mp.grid", mg);
    dump_area("mp.grid", mg, 10, 10);

    //--- group C: the nkv != 1 guard (D:25-27), a bare `new Error` -------------
    dump_throw("mp.grid.nkv2", () { auto g2 = new StructuredGrid(3, 4, 2); new MeshPatch(g2); });
    dump_throw("mp.slab.nkv2", () { auto sl = mk_surf_grid(mk_coons(), 3, 3).makeSlabGrid(1.0);
                                    new MeshPatch(sl); });

    //--- group D: a blank grid, whose vertices are all Vector3.init -----------
    auto blank = new StructuredGrid(3, 2);
    auto mb = new MeshPatch(blank);
    dump_patch("mp.blank", mb);
    auto bp = mb(0.5, 0.5);
    writefln("mp.blank opCall r=0.5 s=0.5 x=%.17g y=%.17g z=%.17g", bp.x, bp.y, bp.z);

    //--- group E: a 1D grid (njv == 1) ----------------------------------------
    auto g1 = new StructuredGrid(new Line(Vector3(0.0, 0.0, 0.0), Vector3(2.0, 0.0, 0.0)), 5);
    auto m1 = new MeshPatch(g1);
    dump_patch("mp.grid1d", m1);
    dump_throw("mp.grid1d.opcall", () { auto p = m1(0.5, 0.5); writefln("%s", p); });

    //--- group F: a 1-wide 2D grid (niv == 1) --------------------------------
    auto gw = mk_surf_grid(mk_coons(), 1, 4);
    auto mw = new MeshPatch(gw);
    dump_patch("mp.niv1", mw);
    dump_throw("mp.niv1.opcall", () { auto p = mw(0.5, 0.5); writefln("%s", p); });

    //--- group F2: degenerate lattice shapes ---------------------------------
    // A 1x1 lattice: nkv is 1 so the guard passes, the mesh is one NaN vertex, and
    // opCall's mesh[i][j+1] (D:88) is the first thing that cannot be satisfied.
    auto g11 = new StructuredGrid(1, 1);
    auto m11 = new MeshPatch(g11);
    dump_patch("mp.1x1", m11);
    dump_throw("mp.1x1.opcall", () { auto p = m11(0.5, 0.5); writefln("%s", p); });
    // A lattice with no vertices in one direction: the constructor gets as far as
    // p00 = mesh[0][0] (D:35) and dies there, after niv/njv and mesh.length are set.
    dump_throw("mp.grid.niv0", () { auto g0 = new StructuredGrid(0, 5); new MeshPatch(g0); });
    dump_throw("mp.grid.njv0", () { auto g0 = new StructuredGrid(5, 0); new MeshPatch(g0); });

    //--- group G: array shapes the constructor cannot serve -------------------
    dump_throw("mp.v.empty", () { Vector3[][] e; new MeshPatch(e); });
    dump_throw("mp.v.emptyinner", () { Vector3[][] e; e.length = 2; new MeshPatch(e); });
    dump_throw("mp.v.raggedshort", () {
        Vector3[][] e = mk_mesh(2, 3);
        e[1] = e[1][0 .. 2];
        new MeshPatch(e);
    });
    Vector3[][] lg = mk_mesh(2, 4);
    Vector3[][] trunc = [lg[0][0 .. 3], lg[1]];
    auto mt = new MeshPatch(trunc);
    dump_patch("mp.v.raggedlong", mt);

    //--- group H: the two to!int sites on non-finite arguments (D:81-82) ------
    dump_throw("mp.oc.r.nan", () { mv(double.nan, 0.5); });
    dump_throw("mp.oc.s.nan", () { mv(0.5, double.nan); });
    dump_throw("mp.oc.r.pinf", () { mv(double.infinity, 0.5); });
    dump_throw("mp.oc.r.ninf", () { mv(-double.infinity, 0.5); });
    dump_throw("mp.oc.r.big", () { mv(1.0e30, 0.5); });
    dump_throw("mp.oc.s.big", () { mv(0.5, -1.0e30); });

    //--- group I: out-of-range r and s, clamped rather than wrapped -----------
    foreach (q; [-1.0, -0.25, 1.25, 2.0]) {
        auto p = mv(q, 0.5);
        writefln("mp.oor r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g", q, 0.5, p.x, p.y, p.z);
        auto q2 = mv(0.5, q);
        writefln("mp.oor r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g", 0.5, q, q2.x, q2.y, q2.z);
    }

    //--- group J: the copy constructor and dup --------------------------------
    auto mc = new MeshPatch(mv);
    dump_patch("mp.copy", mc);
    dump_sweep("mp.copy", mc);
    auto dp = cast(MeshPatch)mv.dup();
    dump_patch("mp.dup", dp);
    dump_sweep("mp.dup", dp);
    dump_area("mp.dup", dp, 10, 10);

    //--- group K: aliasing. Mutate the original, then re-dump the copies. -----
    mv.mesh[1][2] = Vector3(9.0, 9.0, 9.0);
    mv.p11 = Vector3(8.0, 8.0, 8.0);
    writefln("mp.indep mutated=mesh[1][2],p11");
    writefln("mp.indep copy mesh[1][2] x=%.17g y=%.17g z=%.17g", mc.mesh[1][2].x,
             mc.mesh[1][2].y, mc.mesh[1][2].z);
    writefln("mp.indep copy p11 x=%.17g y=%.17g z=%.17g", mc.p11.x, mc.p11.y, mc.p11.z);
    writefln("mp.indep dup mesh[1][2] x=%.17g y=%.17g z=%.17g", dp.mesh[1][2].x,
             dp.mesh[1][2].y, dp.mesh[1][2].z);
    writefln("mp.indep dup p11 x=%.17g y=%.17g z=%.17g", dp.p11.x, dp.p11.y, dp.p11.z);
    // and the mutated original still reports the new values through opCall
    auto af = mv(1.0/3.0 + 0.05, 2.0/3.0 + 0.05);
    writefln("mp.indep orig aftermut r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g",
             1.0/3.0 + 0.05, 2.0/3.0 + 0.05, af.x, af.y, af.z);
}
