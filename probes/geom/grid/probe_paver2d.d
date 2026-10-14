// probes/geom/grid/probe_paver2d.d — what geom/grid/paver2d.d
// (module geom.grid.paver2d) produces, measured against ldc2 1.40.0 at gdtk
// e587b235.
//
// Build and capture (reproducible; the gdtk tree itself is read-only):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/grid \
//     probes/geom/grid/probe_paver2d.d -of=/tmp/probe_g3 -od/tmp/probe_obj_g3
//   /tmp/probe_g3 > baselines/e587b235/probes/geom_grid_paver2d.txt
// @nogc has to go because ldc2 1.40 rejects `new` inside @nogc; it is a
// compile-time attribute with no runtime effect and the C++ port has none either.
//
// paver2d.d declares NO `unittest` block (verified:
//   grep -c unittest /home/tang/packages/gdtk/src/geom/grid/paver2d.d  ->  0)
// so this replay is the whole gate for the module.
//
// MEASURED DEPENDENCY, correcting the wave-02-grid brief: paver2d.d does NOT
// reference PavedGrid / POINT_LIST / FACE_LIST / CELL_LIST at all
// (   grep -n "PavedGrid\|POINT_LIST\|FACE_LIST\|CELL_LIST" geom/grid/paver2d.d  ->  empty   ).
// What fill_interior actually depends on is geom/grid/usgrid.d (stage G5):
//   USGFace          paver2d.d:15,33        <- usgrid.d:154 (ctor usgrid.d:168)
//   USGCell          paver2d.d:17,44        <- usgrid.d:218 (ctor usgrid.d:225)
//   USGCell_type     paver2d.d:44           <- usgrid.d:40, enumerator triangle = 1
//   makeFaceTag      paver2d.d:31,40,41,42  <- usgrid.d:112
//   size_t[string]   paver2d.d:15           <- D associative array
// The two `*_demo.d` files in this slice additionally need sgrid.d's
// UnstructuredGrid, so their corpora are captured here as well and are
// reproducible without any C++ code existing.
//
// What each group pins down:
//   nt.*   enum NodeType {side, rowend, corner, reversal} (paver2d.d:12) as D
//          actually prints it: writeln of each member, the type's .init (which
//          member an uninitialised NodeType is), to!string per member, text() of
//          four of them, %d vs %s under writefln, cast(int)/cast(long)/
//          cast(ushort), sizeof, the relational operators D gives an enum for free,
//          and the fallback for an out-of-range value (cast(NodeType)7, -1, int.max),
//          which D renders as the number rather than as a name.
//          free. paver2d.NodeType is otherwise DEAD CODE in the D tree
//          (grep -rn "NodeType" gdtk/src --include=*.d matches only this
//          declaration outside extern/D-YAML), so nothing constrains its spelling
//          but this probe.
//          Three things this probe first carried and DROPPED, because the port has
//          no faithful way to emit the same bytes: `.stringof`/`typeof(...).stringof`
//          and `is(NodeType : int)`/`NodeType.min`/`.max` are compile-time
//          constructs, and `%s` of a D enum ARRAY prints `[side, rowend, ...]`
//          unquoted while the port's `%s` of a string array quotes its elements
//          (wave-02 brief section 3) — so an array-of-NodeType row would compare two
//          different format contracts, not two enum spellings. Also measured:
//          NodeType has NO `.ordinal` property (ldc2: "no property `ordinal` for
//          type `int`"), so that row is a compile error, not an observable.
//   fi.*   fill_interior (paver2d.d:14-52) driven with state built exactly the way
//          its only caller builds it (usgrid.d:465-499, transcribed in
//          load_from_bpoints below). Rows are emitted AFTER each call, so the two
//          writeln lines paver2d.d:19 and paver2d.d:47 appear in the corpus at the
//          position the module puts them, byte for byte, and a call that dies
//          before line 47 proves the second line is not printed.
//          Cases: fi.sq4/fi.hex6/fi.poly12 the caller's closed polygon (n-1 cells,
//          the closing triangle id[n-1],id[0],mid is NOT made - paver2d.d:36 loop
//          bound is 0..n-1); fi.n1 a single-point boundary (mid = v/1, no cells);
//          fi.n2a/fi.n2b a two-point boundary where the mid-point arithmetic
//          degenerates (mid is exactly on the boundary segment, the cell has zero
//          area, and (0.1+0.2)/2 pins the sum-then-divide rounding of
//          paver2d.d:24-25); fi.three a three-point boundary where sum-then-divide
//          differs from a naive mean; fi.n0 the empty boundary, where `0 .. n-1`
//          underflows a size_t (paver2d.d:36) and indexing bndry_vtx_ids[0] dies
//          with D's bounds text AFTER the mid vertex was already appended;
//          fi.miss a boundary whose faces were never made, so the read of
//          faceIndices[makeFaceTag([id_j, id_jp1])] (paver2d.d:40) hits a missing
//          key -> "Range violation" (an Error, not an Exception); fi.dupids and
//          fi.rev exercise the associative-array overwrite at paver2d.d:32;
//          fi.nonseq strided ids so the mean is over a subset of the vertex list;
//          fi.nanhi/fi.nanlo NaN vertices (D's .init and an explicitly negated
//          NaN) to pin the sign that reaches mid through `mid += v` and `mid /= n`;
//          fi.inf a huge vertex so mid overflows to -inf; fi.hollow the ids the
//          caller would make for a two-segment boundary array; fi.twice two calls
//          in a row on the same state (mid appended twice, tags re-set).
//          Every coordinate is printed at %.17g, because paver2d.d:25 divides by a
//          size_t count and that rounding is part of the contract.

import std.algorithm;
import std.array;
import std.conv;
import std.format;
import std.math;
import std.stdio;
import std.traits;

import geom.grid.paver2d;
import geom.grid.usgrid;
import geom.elements.vector3;

//----------------------------------------------------------------- helpers

/// The state the only caller of fill_interior builds before calling it
/// (usgrid.d:465-499: the vertex copy, bndry_vtx_ids and the boundary faces).
/// The distance checks, the BoundaryFaceSet list and sort_faces_in_boundary_order
/// are usgrid.d's own business and are deliberately NOT reproduced here.
void load_from_bpoints(ref Vector3[] vertices, ref USGFace[] faces,
                       ref size_t[string] faceIndices, ref size_t[] bndry_vtx_ids,
                       const Vector3[][] bPoints)
{
    foreach (i, bpa; bPoints) {
        if (i == 0) {
            vertices ~= Vector3(bpa[0]);
            bndry_vtx_ids ~= vertices.length - 1;
        }
        foreach (j; 1 .. bpa.length) {
            vertices ~= Vector3(bpa[j]);
            bndry_vtx_ids ~= vertices.length - 1;
            // Also, build a face object on the boundary.
            size_t[] vtx_id_list = [vertices.length-2, vertices.length-1];
            string faceTag = makeFaceTag(vtx_id_list);
            faceIndices[faceTag] = faces.length; // before new face
            faces ~= new USGFace(vtx_id_list);
        }
    }
}

void dump_state(string name, const Vector3[] vertices, const USGFace[] faces,
                const size_t[string] faceIndices, const USGCell[] cells)
{
    writeln("case=", name, " END nvertices=", vertices.length, " nfaces=", faces.length,
            " ncells=", cells.length, " naa=", faceIndices.length);
    foreach (i, v; vertices) {
        writefln("case=%s vtx=%d x=%.17g y=%.17g z=%.17g", name, i, v.x, v.y, v.z);
    }
    foreach (i, f; faces) {
        // The tag is recomputed from the face's own vertex list, so this row shows
        // what paver2d.d:33 handed to USGFace's constructor, not what the class
        // printed for itself.
        writefln("case=%s face=%d ids=%s tag=%s", name, i, f.vtx_id_list,
                 makeFaceTag(f.vtx_id_list));
    }
    foreach (i, c; cells) {
        writefln("case=%s cell=%d type=%d id=%d vtx=%s faces=%s outsign=%s", name, i,
                 cast(int)c.cell_type, c.id, c.vtx_id_list, c.face_id_list,
                 c.outsign_list);
    }
    // D's associative-array iteration order is randomized per process, so the keys
    // are sorted first and the dump is byte-reproducible.
    string[] tags = faceIndices.byKey().array();
    sort(tags);
    foreach (k; tags) {
        writefln("case=%s aa=%s val=%d", name, k, faceIndices[k]);
    }
}

/// One fill_interior case: build the caller's state, call, dump (or report the
/// Throwable the call left behind).
void run_case(string name, const Vector3[][] bPoints, const size_t[] ids_override)
{
    writeln("case=", name, " BEGIN");
    Vector3[] vertices;
    USGFace[] faces;
    size_t[string] faceIndices;
    USGCell[] cells;
    size_t[] bndry_vtx_ids;
    load_from_bpoints(vertices, faces, faceIndices, bndry_vtx_ids, bPoints);
    if (ids_override.length > 0) {
        bndry_vtx_ids = ids_override.dup;
    }
    writeln("case=", name, " PRE nvertices=", vertices.length, " nfaces=", faces.length,
            " ncells=", cells.length, " naa=", faceIndices.length,
            " ids=", bndry_vtx_ids);
    try {
        fill_interior(vertices, faces, faceIndices, cells, bndry_vtx_ids);
        writeln("case=", name, " done");
    }
    catch (Throwable t) {
        // .msg only: .file()/.line() name paver2d.d and cannot be reproduced from
        // the port, so they are not in the corpus (same rule as geom_grid_grid).
        writeln("case=", name, " THROW class=", t.classinfo.name, " msg=", t.msg);
    }
    dump_state(name, vertices, faces, faceIndices, cells);
}

/// A case that needs ids but no boundary faces at all (fi.n0, fi.miss).
void run_bare_case(string name, const Vector3[] vs, const size_t[] ids, bool make_faces)
{
    writeln("case=", name, " BEGIN");
    Vector3[] vertices;
    USGFace[] faces;
    size_t[string] faceIndices;
    USGCell[] cells;
    size_t[] bndry_vtx_ids;
    foreach (v; vs) { vertices ~= v; }
    foreach (i; 0 .. ids.length) { bndry_vtx_ids ~= ids[i]; }
    if (make_faces) {
        foreach (j; 0 .. ids.length-1) {
            size_t[] vtx_id_list = [ids[j], ids[j+1]];
            string faceTag = makeFaceTag(vtx_id_list);
            faceIndices[faceTag] = faces.length;
            faces ~= new USGFace(vtx_id_list);
        }
    }
    writeln("case=", name, " PRE nvertices=", vertices.length, " nfaces=", faces.length,
            " ncells=", cells.length, " naa=", faceIndices.length, " ids=", bndry_vtx_ids);
    try {
        fill_interior(vertices, faces, faceIndices, cells, bndry_vtx_ids);
        writeln("case=", name, " done");
    }
    catch (Throwable t) {
        writeln("case=", name, " THROW class=", t.classinfo.name, " msg=", t.msg);
    }
    dump_state(name, vertices, faces, faceIndices, cells);
}

void main()
{
    //============================= nt: enum NodeType (paver2d.d:12)
    writeln("nt.writerside=", NodeType.side);
    writeln("nt.writerrowend=", NodeType.rowend);
    writeln("nt.writercorner=", NodeType.corner);
    writeln("nt.writerreversal=", NodeType.reversal);
    writeln("nt.init=", NodeType.init);
    writeln("nt.tostring side=", NodeType.side.to!string);
    writeln("nt.tostring rowend=", NodeType.rowend.to!string);
    writeln("nt.tostring corner=", NodeType.corner.to!string);
    writeln("nt.tostring reversal=", NodeType.reversal.to!string);
    writeln("nt.text all=", text(NodeType.side, NodeType.rowend, NodeType.corner,
                                NodeType.reversal));
    writeln("nt.sizeof=", NodeType.side.sizeof);
    writeln("nt.vinit=", Vector3.init); // control: D's struct .init is NaN per field
    // The row tags are spelled dfmt/sfmt, not "%d"/"%s": a literal `%d` inside the
    // format string is a live specifier in Phobos and would consume an argument
    // (measured: the first version died with FormatException "Orphan format
    // specifier: %d").
    writefln("nt.dfmt side=%d rowend=%d corner=%d reversal=%d", NodeType.side,
             NodeType.rowend, NodeType.corner, NodeType.reversal);
    writefln("nt.sfmt side=%s rowend=%s corner=%s reversal=%s", NodeType.side,
             NodeType.rowend, NodeType.corner, NodeType.reversal);
    writefln("nt.cast int side=%d rowend=%d corner=%d reversal=%d",
             cast(int)NodeType.side, cast(int)NodeType.rowend, cast(int)NodeType.corner,
             cast(int)NodeType.reversal);
    writefln("nt.cast long=%d,%d,%d,%d", cast(long)NodeType.side,
             cast(long)NodeType.rowend, cast(long)NodeType.corner,
             cast(long)NodeType.reversal);
    writefln("nt.cast ushort=%d,%d,%d,%d", cast(ushort)NodeType.side,
             cast(ushort)NodeType.rowend, cast(ushort)NodeType.corner,
             cast(ushort)NodeType.reversal);
    // Out-of-range values: D's generated enum toString falls back to the number.
    // Measured, because the port's renderer needs the same fallback.
    writeln("nt.oor7=", cast(NodeType)7);
    writeln("nt.oor7.tostring=", (cast(NodeType)7).to!string);
    writeln("nt.oorminus1=", cast(NodeType)(-1));
    writeln("nt.oormax=", cast(NodeType)int.max);
    writefln("nt.cmp=%d,%d,%d,%d", cast(int)(NodeType.side < NodeType.rowend),
             cast(int)(NodeType.corner >= NodeType.reversal),
             cast(int)(NodeType.reversal == NodeType.reversal),
             cast(int)(NodeType.side != NodeType.corner));

    //============================= fi: fill_interior (paver2d.d:14-52)
    // fi.sq4 — the four-corner boundary the paver2d demo's shape would give, with
    // the caller's own boundary faces present.
    run_case("fi.sq4", [[Vector3(0,0), Vector3(1,0), Vector3(1,1), Vector3(0,1),
                         Vector3(0,0)]], size_t[].init);
    // fi.hex6 — six points, ids 0..5.
    run_case("fi.hex6", [[Vector3(0,0), Vector3(2,0), Vector3(3,1), Vector3(2,2),
                          Vector3(0.5,3), Vector3(0,0)]], size_t[].init);
    // fi.poly12 — a 12-point closed polygon, the scale at which the mid-point mean
    // actually matters. The coordinates are explicit decimal literals rather than
    // cos/sin expressions, for two reasons: backlog T-909 (ldc2's compile-time
    // folding of literal arithmetic is not always correctly rounded) and the
    // wave-02 note that Phobos and glibc differ by 1 ULP on transcendentals. Both
    // sides of the gate must start from the same doubles.
    run_case("fi.poly12", [[
        Vector3(1.5, 0),
        Vector3(1.4330127018922194, 0.49999999999999994),
        Vector3(1.25, 0.8660254037844386),
        Vector3(1, 1),
        Vector3(0.75000000000000011, 0.86602540378443871),
        Vector3(0.56698729810778059, 0.49999999999999994),
        Vector3(0.5, 1.2246467991473532e-16),
        Vector3(0.56698729810778059, -0.49999999999999972),
        Vector3(0.74999999999999978, -0.86602540378443837),
        Vector3(0.99999999999999989, -1),
        Vector3(1.25, -0.8660254037844386),
        Vector3(1.4330127018922192, -0.50000000000000044),
        Vector3(1.5, 0)]], size_t[].init);
    // fi.n1 — single-point boundary.
    run_case("fi.n1", [[Vector3(-0.5, 3.25)]], size_t[].init);
    // fi.n2a — two-point boundary, mid-point degeneracy with exact halves.
    run_case("fi.n2a", [[Vector3(-0.5, 0), Vector3(0.5, 1)]], size_t[].init);
    // fi.n2b — two-point boundary pinned to the sum-then-divide rounding:
    // (0.1+0.2)/2 is NOT 0.15 exactly.
    run_case("fi.n2b", [[Vector3(0.1, 0.7), Vector3(0.2, -0.1)]], size_t[].init);
    // fi.three — three points, so mid is a third.
    run_case("fi.three", [[Vector3(0.1, 0.33333333333333331), Vector3(0.2, 0.2),
                           Vector3(0.3, 7), Vector3(0.1, 0.33333333333333331)]],
             size_t[].init);
    // fi.hollow — two boundary segments as the caller would hand them over, with
    // the shared end point repeated.
    run_case("fi.hollow", [[Vector3(0,0), Vector3(1,0), Vector3(1,1)],
                           [Vector3(1,1), Vector3(0,1), Vector3(0,0)]], size_t[].init);
    // fi.nonseq — ids that skip vertices, so the mean is over a subset.
    run_bare_case("fi.nonseq", [Vector3(1,1), Vector3(2,3), Vector3(4,5), Vector3(8,9),
                                Vector3(16,17), Vector3(0,0)], [1, 3, 5], true);
    // fi.dupids — a repeated id, so two interior faces share one tag and
    // faceIndices[that tag] is overwritten (paver2d.d:32).
    run_bare_case("fi.dupids", [Vector3(0.25, 0.75), Vector3(1.5, -2.5)], [0, 0, 1], true);
    // fi.rev — ids in the opposite order to the faces the caller made.
    run_bare_case("fi.rev", [Vector3(0,0), Vector3(3,0), Vector3(3,4), Vector3(1,2)],
                  [3, 2, 1, 0], true);
    // fi.nanhi — a vertex that is Vector3.init (three NaNs, positive).
    {
        Vector3 nanv;
        run_bare_case("fi.nanhi", [nanv, Vector3(1, 2), Vector3(3, 4)], [0, 1, 2], true);
    }
    // fi.nanlo — first accumulated vertex carries a negative NaN, so `mid += v`
    // picks up that sign (measured, not assumed).
    run_bare_case("fi.nanlo", [Vector3(-double.nan, 0, 0), Vector3(1, 2), Vector3(3, 4)],
                  [0, 1, 2], true);
    // fi.inf — an infinite vertex, so mid overflows through the division.
    run_bare_case("fi.inf", [Vector3(1.7976931348623157e308, -1.7976931348623157e308, 0),
                             Vector3(1.7976931348623157e308, 0, 0)], [0, 1], true);
    // fi.n0 — empty boundary: n-1 underflows the size_t range at paver2d.d:36.
    run_bare_case("fi.n0", [Vector3(1, 2)], size_t[].init, false);
    // fi.miss — boundary faces never made, so paver2d.d:40 reads a missing key.
    run_bare_case("fi.miss", [Vector3(0,0), Vector3(1,0), Vector3(1,1)],
                  [0, 1, 2], false);
    // fi.twice — two calls on one state.
    {
        writeln("case=fi.twice BEGIN");
        Vector3[] vertices;
        USGFace[] faces;
        size_t[string] faceIndices;
        USGCell[] cells;
        size_t[] bndry_vtx_ids;
        load_from_bpoints(vertices, faces, faceIndices, bndry_vtx_ids,
                          [[Vector3(0,0), Vector3(2,0), Vector3(2,2), Vector3(0,0)]]);
        writeln("case=fi.twice PRE nvertices=", vertices.length, " nfaces=", faces.length,
                " ncells=", cells.length, " naa=", faceIndices.length,
                " ids=", bndry_vtx_ids);
        fill_interior(vertices, faces, faceIndices, cells, bndry_vtx_ids);
        writeln("case=fi.twice mid1=", vertices[vertices.length-1].x, " ",
                vertices[vertices.length-1].y);
        fill_interior(vertices, faces, faceIndices, cells, bndry_vtx_ids);
        writeln("case=fi.twice done");
        dump_state("fi.twice", vertices, faces, faceIndices, cells);
    }
} // end main()
