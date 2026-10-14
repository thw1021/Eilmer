// probes/geom/grid/probe_usgrid.d — what geom/grid/usgrid.d (module geom.grid.usgrid)
// produces, measured against ldc2 1.40.0 at gdtk e587b235.
//
// UnstructuredGrid is the face/cell topology grid: three helper classes (USGFace,
// USGCell, BoundaryFaceSet) that each own four text/binary (de)serialisation paths,
// six constructors (empty, from another, 2D paving, Heather Muir's PavedGrid, from a
// StructuredGrid, from an imported file), an in-place face sort by boundary membership,
// four file readers and five file writers, cell splitting, grid joining, and the
// vector-area closure tests that gate every import.
//
// Build and capture (reproducible; the gdtk tree itself is read-only):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/grid \
//     probes/geom/grid/probe_usgrid.d -of=/tmp/probe_usg -od/tmp/probe_obj_usg
//   /tmp/probe_usg > baselines/e587b235/probes/geom_grid_usgrid.txt
// No -O flag: every baseline in this repository is an ldc2 -O0 capture, and this one is
// too (T-919/T-920 showed twice that an optimisation-level artefact can masquerade as a
// D behaviour).
// @nogc has to go because ldc2 1.40 rejects `throw new ...` inside usgrid.d's own @nogc
// members (D:62, :78, :94, :857, :868, :993, :999, :1005, :1021, :1037) — a compile-time
// attribute with no runtime effect, and the C++ port has no @nogc to carry.
// Run the binary twice and compare before trusting anything; see the note at the foot
// of this header about what was measured to be non-deterministic and therefore left out.
//
// Every artefact the probe writes goes under /tmp/probe_usg_dir/, never the repository
// working tree. Input grids (SU2 text, gzipped native text, raw binary) are synthesised
// here with fixed names, so the port can regenerate byte-identical inputs.
//
// ---------------------------------------------------------------------------
// What each group pins down (row prefixes, all us.*):
//   us.name.*   USGCell_type/cell_names/vtk_element_types (D:40-60), convert_cell_type
//               (D:62), n_vertices_for_vtk_element (D:78), cell_type_from_name (D:94) —
//               including the deliberate "polygon" rejection (D:101 is commented out) —
//               to!string of every enum member (D:280 uses it) and to!USGCell_type of
//               in-range and out-of-range integers (D:278).
//   us.tag.*    makeFaceTag (D:112): sorted-vertex key, the empty list, duplicates,
//               size_t.max, and the fact that the caller's array is NOT reordered.
//   us.orient.* sameOrientation (D:134): 2-point order sensitivity, cyclic equivalence
//               in 3D, both assert messages, the empty-list pair (returns false).
//   us.tstr.*   every to!string spelling usgrid actually uses (29 sites): size_t, int,
//               char[], enum member, Vector3, and the doubles that reach a message.
//               D renders a double as %.6g, not %.17g (wave-02-brief section 5), so each
//               row carries both spellings plus the bit pattern.
//   us.face.*   USGFace (D:154): all four constructors, toString (D:196), toIOString
//               (D:201), rawWrite/rawRead round trip, the .dup independence of
//               vtx_id_list, and the fact that the copy constructor drops left_cell and
//               right_cell (D:172-178 says so; measured here).
//   us.cell.*   USGCell (D:218): all four constructors, toString (D:290), toIOString
//               (D:301), raw round trip. THE SHADOWED-PARAMETER QUIRK (D:236-238):
//               `this(const USGCell other, size_t id) { id = other.id; ... }` assigns to
//               the PARAMETER, so this.id is never written and a copied cell comes back
//               with id == 0 (size_t.init), not the passed id and not other.id.
//   us.bfs.*    BoundaryFaceSet (D:328): the four constructors, toString (D:389) which
//               indexes face_id_list[0] and therefore throws on a tag-only set,
//               toIOString, raw round trip with an empty tag (D:371 branch).
//   us.ctor.*   UnstructuredGrid(dimensions,label) (D:434) for 1/2/3, and 0 and -1
//               (no validation at all), with the empty containers.
//   us.str.*    toString (D:875) — first/last vertex, face, cell, and the whole
//               boundaries array (every element goes through its own toString).
//               An empty grid throws in the vertices[0] format at D:880.
//   us.dup.*    dup() (D:848) and the copy constructor (D:440): what is copied, what is
//               new, what is dropped (face left/right cell links), and that faceIndices
//               is rebuilt from the duplicated faces. Mutation-after-dup rows.
//   us.sg2d.*   the StructuredGrid constructor (D:553, 2D branch D:561-661) on a real
//               lattice: counts, the vertex/face/cell ordering, faceIndices, the four
//               boundary sets, the west/east/south/north outsigns, the left_cell /
//               right_cell wiring (D:656-659), the three asserts (D:821-823), and the
//               effect of sort_faces_in_boundary_order (D:888) on faces / cell
//               face_id_list / boundaries / faceIndices.
//               DEPENDS ON sgrid.d (stage G4): these rows construct a StructuredGrid,
//               which is exactly what usgrid.d:553 takes and what usgrid.d's own
//               unittest does at D:2492. Tagged sg-* in the row text.
//   us.sg3d.*   the same for the 3D branch (D:662-820) with i/j/k faces and the six
//               boundaries, plus the hex cell face order (D:798-804) and outsigns.
//   us.sgbad.*  StructuredGrid inputs the constructor is not written for: a 1D lattice
//               falls into the "Assume dimensions == 3" branch and reads bcTags[2] of a
//               2-element array (D:568); a lattice whose bcTags are set names the
//               boundaries; a 2x2 lattice (one cell); a label-propagation row
//               (D:555, D:573). DEPENDS ON sgrid.d.
//   us.pave.*   the Vector3[][] paving constructor (D:458): the closed-region walk, the
//               boundary tags "bndry-0..n" (D:490), paver2d.fill_interior's two writeln
//               lines, the resulting triangle cells, and the two bare Error messages
//               (D:466-469 first-vs-last point, D:492-496 segment-to-segment).
//   us.paved.*  Heather Muir's PavedGrid constructor (D:514) and its use of
//               POINT_LIST/FACE_LIST/CELL_LIST (D:522-548). DEPENDS ON paver.d (G2).
//   us.idx.*    opIndex (D:857, :868) and its four in-contract assert texts;
//               number_of_vertices_for_cell / vtk_element_type_for_cell / get_cell_type
//               (D:993-1009) and their out-of-range ArrayIndexError.
//   us.cid.*    get_vtx_id_list_for_cell both spellings (D:1011, :1028) with the
//               text(...) assert messages, and copy_vtx_id_list_for_cell (D:1021, :1037)
//               into a pre-filled size_t[8] so the unwritten tail is visible.
//   us.conn.*   get_connected_cell_ids_for_cell (D:1045) — including the associative-
//               array iteration order that D:1054 `.keys.dup` returns.
//   us.sort.*   sort_faces_in_boundary_order (D:888) driven directly on grids built by
//               hand, with the interior/boundary split, ninteriorfaces, and the assert
//               at D:963.
//   us.bnd.*    get_boundary_grid (D:1057): 2D -> 1D line grids, 3D -> 2D surface grids
//               for all six faces, the GeometryException for new_dimensions outside
//               1..2 (D:1066), the three "Assumed 2D/3D" asserts, the 5-vertex face
//               throw (D:1094), and the empty-tag naming of the returned grid.
//   us.blc.*    get_list_of_boundary_cells (D:1106) in 2D and 3D, and the one plain
//               Exception in the file (D:1120-1126) when the inside cell is null.
//   us.vaf.*    vectorAreaOfFacet (D:1133) for 2/3/4/5/6 vertices, degenerate facets,
//               the bare assert at D:1136 for a 1-element list, and vectorAreaOfCell
//               (D:1167) on closed and deliberately open cells.
//   us.split.*  splitTriangleCell (D:1179): the new vertex/faces/cells, the rewritten
//               original cell, the face links, the bare assert at D:1181, and the two
//               GeometryException texts with their embedded newlines (D:1183, :1188).
//   us.link.*   link_faces_back_to_cells (D:1258) and its assert (D:1263).
//   us.gz.*     write_to_gzip_file (D:1800) payload + read_from_gzip_file (D:1275)
//               round trip. Compared as the DECOMPRESSED payload: the container carries
//               an mtime and differs between two runs of D itself (grid brief 2.1).
//               Also the version guard (D:1286), a missing/short file, and a corrupt
//               line.
//   us.raw.*    write_to_raw_binary_file (D:1823) as a hex body + round trip through
//               read_from_raw_binary_file (D:1330), the bad-header Error (D:1342) and
//               its to!string(char[]) of the wrong bytes.
//   us.su2.*    write_to_su2_file (D:1876) full body for 2D triangles, 2D quads, 3D
//               hexes, and a hand-made wedge grid in both orders
//               (use_gmsh_order_for_wedges true/false), plus scaled output.
//   us.su2rd.*  read_from_su2_file (D:1382) on synthesised files: the three stdout dots
//               and the " (done)" line, header parsing, both token-count spellings,
//               GMSH-ordered wedges, cell indices out of natural order, markers, the
//               2D orientation reversal (D:1519-1546), orphan faces (D:1762), and every
//               failure path (wrong item count D:1437, unknown element D:1443, invalid
//               2D/3D cell type D:1544/:1690, duplicate cell D:1615, interior face in a
//               marker D:1734, unknown marker face D:1739, unclosed boundary D:1776).
//   us.vtk.*    write_to_vtk_file (D:1850) full body in 2D and 3D, and
//               read_from_vtk_text_file (D:1785) up to its unconditional Error (D:1795).
//   us.stl.*    the inherited Grid.write_to_stl_file (grid.d:389) driven through a 2D
//               unstructured grid, because it consumes usgrid's
//               copy_vtx_id_list_for_cell; its 3D Exception and body are rows.
//   us.foam.*   write_openFoam_polyMesh (D:1930): points/faces/owner/neighbour/boundary
//               and the sets files as bodies, the internal-face-first ordering, the
//               reversal table, the empty-tag branch that OVERWRITES the grid label
//               (D:2148, a shadowed local name) and then opens "<setsDir>/" as a file,
//               and the two asserts (D:1977, :1983, :2001, :2008).
//   us.join.*   joinGrid (D:2164): the same-object early return, the six asserts
//               (D:2174-2179), both checkBoundariesOnly paths, openFoam true/false
//               boundary retention, the unattached-face warning, merged counts, and the
//               outsign GeometryException (D:2342).
//   us.stats.*  writeStats (D:2416) on a good grid and on grids with an open cell and
//               an open boundary, which is where to!string(Vector3) (%.6g) shows up.
//   us.vtxb.*   getVertexIdsOnBoundaries (D:2450) in 2D and 3D and on an empty grid.
//   us.bin.*    the inherited bin sort and enclosure search on an unstructured grid
//               (grid.d:277, :320, :355): nbinz forced to 1 in 2D (grid.d:281), the
//               nbinx<1 GeometryException, and finds on cell centres and outside the
//               bounding box.
//   us.imp.*    the import constructor (D:830): format dispatch, "vtkxml" (D:840),
//               an unknown format (D:841), a file that does not exist for each format,
//               and new_label propagation (D:843).
//   us.unit.*   usgrid.d's own unittest (D:2484-2524): its five writeln lines verbatim,
//               all six asserts replayed as observations, the sort_cells_into_bins(10,10)
//               + find_enclosing_cell result (cell 45, barycentre (0.55, 0.325)), and the
//               two canFind rows. DEPENDS ON sgrid.d (CoonsPatch + StructuredGrid 11x21).
//
// Throw-identity rows (`|class=...|file=...|line=...|toString1=...`) follow the format
// established by probes/substrate/probe_throw_identity.d and
// baselines/e587b235/probes/substrate_throw_identity.txt, which measured that a bare
// `new Error(...)` reaches the caller with .file == "" and .line == 0 while
// `new GeometryException(...)` and `new Exception(...)` carry the throw site (T-922).
// usgrid.d has all three: 8 bare Error sites, 19 GeometryException sites, 1 Exception
// site. For a throw inside usgrid.d the recorded file is the module path as the compiler
// saw it (/tmp/gdtk_shadow/geom/grid/usgrid.d); the portable part of the site is the
// LINE NUMBER, so every such row also states the D source line it is expected at.
// Throwable.toString() is printed as ONLY its first line: D appends
// "\n----------------\n" and a backtrace of "??:? [0x...]" frames whose addresses change
// every run, which no port can reproduce (T-903). Messages from inside Phobos keep their
// own file/line in toString1; those are informative only, .msg is the contract.
import std.algorithm;
import std.array;
import std.conv;
import std.file;
import std.format;
import std.math;
import std.range;
import std.stdio;
import std.string;

import gzip;
import nm.number;

import geom;
import geom.grid.paver: PavedGrid, POINT_LIST, FACE_LIST, CELL_LIST;

//-----------------------------------------------------------------
// helpers

string TD = "/tmp/probe_usg_dir";

string nl_escape(string s)
{
    return replace(s, "\n", "<newline>");
}

/// Bit pattern of a double, so two values that print the same can still be told apart.
string bits(double x)
{
    ulong u = *cast(ulong*)&x;
    return format("%016x", u);
}

/// The D spelling and the %.17g spelling side by side, with the bits.
string dsp(double x)
{
    return format("[%s]/%s/%.17g", to!string(x), bits(x), x);
}

/// -1 if the artefact was never created, so a failed writer shows up as a row instead
/// of killing the capture.
int len_of(string p)
{
    if (!std.file.exists(p)) return -1;
    return cast(int)(cast(ubyte[])read(p)).length;
}

/// Whole file body, newlines marked, for text artefacts.
string body_of(string p)
{
    if (!std.file.exists(p)) return "<absent>";
    return join(split(cast(string)read(p), "\n"), "<newline>");
}

/// Decompressed payload of a .gz artefact — the container bytes carry an mtime and
/// differ between two runs of D itself, so only the payload is a contract.
string gunzip_body_of(string p)
{
    if (!std.file.exists(p)) return "<absent>";
    auto r = new GzipByLine(p);
    string[] L;
    while (!r.empty) { L ~= r.front; r.popFront(); }
    return join(L, "<newline>");
}

/// Byte-exact rendering of a string that may contain unprintable bytes.
string hexb(string s)
{
    string o;
    foreach (b; cast(ubyte[])s) { o ~= format("%02x", b); }
    return o;
}

string hex_of(string p)
{
    if (!std.file.exists(p)) return "<absent>";
    auto b = cast(ubyte[])read(p);
    string s;
    foreach (x; b) { s ~= format("%02x", x); }
    return s;
}

/// Write a synthesised input file verbatim (real newlines inside).
void mkr(string p, string body)
{
    std.file.write(p, body);
}

/// Number of bytes of s outside printable ASCII, and s with each such byte replaced by
/// `?`. Three rows in this corpus (the raw-binary bad-header ones) print `to!string` of
/// a char[] that the short rawRead left holding stale buffer bytes; their non-printable
/// tail is not a D value any port can reproduce, so the tail is replaced by a count and
/// the printable prefix stays byte-exact. See the header note.
string upr(string s, out int n)
{
    n = 0;
    string o;
    foreach (c; s) {
        if (c >= 0x20 && c < 0x7f) { o ~= c; }
        else if (c == '\n') { o ~= c; }
        else { o ~= '?'; ++n; }
    }
    return o;
}

/// First line of a Throwable.toString(), with the message escaped.
string tostr1(Throwable t)
{
    string s = t.toString();
    auto i = indexOf(s, "\n");
    if (i < 0) i = s.length;
    return nl_escape(s[0 .. i]);
}

/// Run dg, and print one row describing whatever came out. `no-throw` if nothing did.
void tw(string tag, void delegate() dg)
{
    try {
        dg();
        writefln("%s|no-throw", tag);
    }
    catch (Exception e) {
        report_row(tag, 0, e);
    }
    catch (Error e) {
        report_row(tag, 0, e);
    }
}

void report_row(string tag, int dline, Throwable e)
{
    int n1, n2;
    string m = upr(nl_escape(e.msg), n1);
    string t = upr(tostr1(e), n2);
    if (dline > 0) {
        writefln("%s|dline=%d|class=%s|msg=[%s]|msgunprint=%d|file=[%s]|line=%d|toString1=[%s]|toString1unprint=%d",
                 tag, dline, e.classinfo.name, m, n1, e.file, e.line, t, n2);
    } else {
        writefln("%s|class=%s|msg=[%s]|msgunprint=%d|file=[%s]|line=%d|toString1=[%s]|toString1unprint=%d",
                 tag, e.classinfo.name, m, n1, e.file, e.line, t, n2);
    }
}

/// Like tw, but the tag states the D source line the throw is expected at.
void twd(string tag, int dline, void delegate() dg)
{
    try {
        dg();
        writefln("%s|no-throw|dline=%d", tag, dline);
    }
    catch (Exception e) {
        report_row(tag, dline, e);
    }
    catch (Error e) {
        report_row(tag, dline, e);
    }
}

void dump_verts(string tag, Vector3[] vv)
{
    foreach (i, v; vv) {
        writefln("%s vtx[%d] %.17g %.17g %.17g bits=%s,%s,%s", tag, i,
                 v.x, v.y, v.z, bits(v.x), bits(v.y), bits(v.z));
    }
}

/// Faces as D prints them, in D's own (post-sort) order.
void dump_faces(string tag, USGFace[] fs)
{
    foreach (i, f; fs) {
        writefln("%s face[%d] vtx=%s tag=%s onbnd=%s L=%s R=%s", tag, i, f.vtx_id_list,
                 makeFaceTag(f.vtx_id_list), f.is_on_boundary,
                 f.left_cell ? text(f.left_cell.id) : "null",
                 f.right_cell ? text(f.right_cell.id) : "null");
    }
}

void dump_cells(string tag, USGCell[] cs)
{
    foreach (i, c; cs) {
        writefln("%s cell[%d] %s", tag, i, c.toIOString());
    }
}

void dump_bnd(string tag, BoundaryFaceSet[] bs)
{
    foreach (i, b; bs) {
        writefln("%s bnd[%d] tag=%s faces=%s outsigns=%s", tag, i, b.tag,
                 b.face_id_list, b.outsign_list);
    }
}

void dump_faceidx(string tag, UnstructuredGrid g, string[] keys)
{
    foreach (k; keys) {
        writefln("%s[%s]=%d", tag, k, g.faceIndices[k]);
    }
}

/// A pre-filled 8-slot copy array, so an unwritten tail is observable (D:1022, :1038).
size_t[8] prefilled8()
{
    size_t[8] c;
    foreach (ref e; c) { e = 999; }
    return c;
}

/// A 5x4 structured grid on a Coons patch, the shape usgrid.d's own unittest uses
/// (D:2489-2492) at a size that keeps the corpus readable.
StructuredGrid surf_grid2d(string label="sg2d")
{
    auto p00 = Vector3(0.0, 0.1);
    auto p10 = Vector3(1.0, 0.1);
    auto p11 = Vector3(1.0, 1.1);
    auto p01 = Vector3(0.0, 1.1);
    auto patch = new CoonsPatch(p00, p10, p11, p01);
    UnivariateFunction[] cf = [new LinearFunction(), new LinearFunction(),
                               new LinearFunction(), new LinearFunction()];
    return new StructuredGrid(patch, 5, 4, cf, "simple", label);
}

/// The face dictionary walked in key order (std.algorithm sort), which is what a
/// `std::map` port produces. The raw D iteration order is emitted separately as a
/// `.aaord` row and is a Phobos associative-array artefact, not a contract.
string sorted_faceidx(UnstructuredGrid g)
{
    auto ks = g.faceIndices.byKey.array;
    ks.sort;
    string o;
    foreach (k; ks) { o ~= format("%s=%d ", k, g.faceIndices[k]); }
    return o;
}

void dump_faceidx_sorted(string tag, UnstructuredGrid g)
{
    auto ks = g.faceIndices.byKey.array;
    ks.sort;
    foreach (k; ks) { writefln("%s[%s]=%d", tag, k, g.faceIndices[k]); }
}

/// The observable state of a whole grid, one group of rows.
void dump_grid(string tag, UnstructuredGrid g)
{
    writefln("%s dims=%d nvertices=%d nfaces=%d ncells=%d nboundaries=%d ninteriorfaces=%d label=%s niv=%d njv=%d nkv=%d",
             tag, g.dimensions, g.nvertices, g.nfaces, g.ncells, g.nboundaries,
             g.ninteriorfaces, g.label, g.niv, g.njv, g.nkv);
    writefln("%s lens vertices=%d faces=%d cells=%d boundaries=%d faceIndices=%d", tag,
             g.vertices.length, g.faces.length, g.cells.length, g.boundaries.length,
             g.faceIndices.length);
    dump_verts(tag, g.vertices);
    dump_faces(tag, g.faces);
    dump_cells(tag, g.cells);
    dump_bnd(tag, g.boundaries);
}

//-----------------------------------------------------------------
// grid builders that do not need sgrid.d

/// A 2x1 grid of triangles over the unit square, built by hand through the paving
/// constructor so the face/cell wiring is D's own.
Vector3[][] pave_square()
{
    // D:466 requires bPoints[0][0] to coincide with bPoints[$-1][$-1], so the outer
    // segment must repeat its start point at the end.
    Vector3[] outer = [Vector3(0.0, 0.0), Vector3(0.5, 0.0), Vector3(1.0, 0.0),
                       Vector3(1.0, 0.5), Vector3(1.0, 1.0), Vector3(0.5, 1.0),
                       Vector3(0.0, 1.0), Vector3(0.0, 0.5), Vector3(0.0, 0.0)];
    return [outer];
}

/// An L-shaped region in two boundary segments (drives D:491-498).
Vector3[][] pave_two_segments()
{
    Vector3[] a = [Vector3(0.0, 0.0), Vector3(1.0, 0.0), Vector3(1.0, 1.0)];
    Vector3[] b = [Vector3(1.0, 1.0), Vector3(0.0, 1.0), Vector3(0.0, 0.0)];
    return [a, b];
}

/// One quad cell, four faces, one boundary holding two of them, built directly from
/// the classes so a specific topology can be examined without any reader.
UnstructuredGrid hand_quad2d(string label="hq")
{
    auto g = new UnstructuredGrid(2, label);
    g.vertices = [Vector3(0.0, 0.0), Vector3(1.0, 0.0), Vector3(1.0, 1.0), Vector3(0.0, 1.0)];
    g.nvertices = 4;
    size_t[][4] fl = [[0,1],[1,2],[2,3],[3,0]];
    foreach (i; 0 .. 4) {
        g.faceIndices[makeFaceTag(fl[i])] = g.faces.length;
        g.faces ~= new USGFace(fl[i]);
    }
    g.nfaces = 4;
    g.cells ~= new USGCell(USGCell_type.quad, 0, [0,1,2,3], [0,1,2,3], [1,1,-1,-1]);
    g.ncells = 1;
    g.boundaries ~= new BoundaryFaceSet("two-of-four", [0,1], [1,1]);
    g.boundaries ~= new BoundaryFaceSet("rest", [2,3], [-1,-1]);
    g.nboundaries = 2;
    g.niv = 4; g.njv = 1; g.nkv = 1;
    g.link_faces_back_to_cells();
    return g;
}

/// Every permutation of [0,1,2] and of [0,1,2,3], in a fixed written-out order, so the
/// sweep does not depend on std.algorithm.permutations range semantics.
size_t[][] perms3()
{
    size_t[][] o;
    foreach (a; 0 .. 3) foreach (b; 0 .. 3) foreach (c; 0 .. 3) {
        if (a == b || a == c || b == c) continue;
        o ~= [cast(size_t)a, cast(size_t)b, cast(size_t)c];
    }
    return o;   // 6 lists, lexicographic
}

size_t[][] perms4()
{
    size_t[][] o;
    foreach (a; 0 .. 4) foreach (b; 0 .. 4) foreach (c; 0 .. 4) foreach (d; 0 .. 4) {
        if (a == b || a == c || a == d || b == c || b == d || c == d) continue;
        o ~= [cast(size_t)a, cast(size_t)b, cast(size_t)c, cast(size_t)d];
    }
    return o;   // 24 lists, lexicographic
}

/// A 3x3x2 structured grid over a trilinear volume, for the 3D writer rows.
StructuredGrid vol_grid3d(string label="sg3d")
{
    Vector3[8] p;
    p[0] = Vector3(0.0, 0.1, 0.0);
    p[1] = Vector3(1.0, 0.1, 0.0);
    p[2] = Vector3(1.0, 1.1, 0.0);
    p[3] = Vector3(0.0, 1.1, 0.0);
    p[4] = Vector3(0.0, 0.1, 3.0);
    p[5] = Vector3(1.0, 0.1, 3.0);
    p[6] = Vector3(1.0, 1.1, 3.0);
    p[7] = Vector3(1.0, 1.1, 3.0);
    auto vol = new TFIVolume(p);
    UnivariateFunction[] cf;
    return new StructuredGrid(vol, 3, 3, 2, cf, "simple", label);
}

//-----------------------------------------------------------------
// Synthesised SU2 inputs, written verbatim to /tmp by the probe so the port can
// regenerate byte-identical files. Format: https://su2code.github.io/docs_v7/Mesh-File/

string tri_su2()
{
    return join([
        "NDIME= 2", "", "NELEM= 8",
        "5 0 1 3 0", "5 1 4 3 1", "5 1 2 4 2", "5 2 5 4 3",
        "5 3 4 6 4", "5 4 7 6 5", "5 4 5 7 6", "5 5 8 7 7",
        "", "NPOIN= 9",
        "0.0 0.0 0", "0.5 0.0 1", "1.0 0.0 2", "0.0 0.5 3", "0.5 0.5 4", "1.0 0.5 5",
        "0.0 1.0 6", "0.5 1.0 7", "1.0 1.0 8",
        "", "NMARK= 4",
        "MARKER_TAG= lower", "MARKER_ELEMS= 2", "3 0 1", "3 1 2",
        "MARKER_TAG= right", "MARKER_ELEMS= 2", "3 2 5", "3 5 8",
        "MARKER_TAG= upper", "MARKER_ELEMS= 2", "3 8 7", "3 7 6",
        "MARKER_TAG= left", "MARKER_ELEMS= 2", "3 6 3", "3 3 0", ""], "\n");
}

string quad_su2()
{
    return join([
        "NDIME= 2", "", "NELEM= 4",
        "9 0 1 4 3 0", "9 1 2 5 4 1", "9 3 4 7 6 2", "9 4 5 8 7 3",
        "", "NPOIN= 9",
        "0.0 0.0 0", "0.5 0.0 1", "1.0 0.0 2", "0.0 0.5 3", "0.5 0.5 4", "1.0 0.5 5",
        "0.0 1.0 6", "0.5 1.0 7", "1.0 1.0 8",
        "", "NMARK= 4",
        "MARKER_TAG= south", "MARKER_ELEMS= 2", "3 0 1", "3 1 2",
        "MARKER_TAG= east", "MARKER_ELEMS= 2", "3 2 5", "3 5 8",
        "MARKER_TAG= north", "MARKER_ELEMS= 2", "3 8 7", "3 7 6",
        "MARKER_TAG= west", "MARKER_ELEMS= 2", "3 6 3", "3 3 0", ""], "\n");
}

string hex_su2()
{
    return join([
        "NDIME= 3", "", "NELEM= 2",
        "12 0 1 4 3 6 7 10 9 0", "12 1 2 5 4 7 8 11 10 1",
        "", "NPOIN= 12",
        "0.0 0.0 0.0 0", "0.5 0.0 0.0 1", "1.0 0.0 0.0 2",
        "0.0 1.0 0.0 3", "0.5 1.0 0.0 4", "1.0 1.0 0.0 5",
        "0.0 0.0 1.0 6", "0.5 0.0 1.0 7", "1.0 0.0 1.0 8",
        "0.0 1.0 1.0 9", "0.5 1.0 1.0 10", "1.0 1.0 1.0 11",
        "", "NMARK= 6",
        "MARKER_TAG= front", "MARKER_ELEMS= 2",
        "9 0 1 4 3", "9 1 2 5 4",
        "MARKER_TAG= back", "MARKER_ELEMS= 2",
        "9 6 7 10 9", "9 7 8 11 10",
        "MARKER_TAG= bottom", "MARKER_ELEMS= 1",
        "9 0 1 7 6",
        "MARKER_TAG= top", "MARKER_ELEMS= 1",
        "9 3 4 10 9",
        "MARKER_TAG= left", "MARKER_ELEMS= 1",
        "9 0 3 9 6",
        "MARKER_TAG= right", "MARKER_ELEMS= 1",
        "9 2 5 11 8", ""], "\n");
}

/// One wedge, VTK vertex order (0,1,2 bottom; 3,4,5 top).
string wedge_su2()
{
    return join([
        "NDIME= 3", "", "NELEM= 1",
        "13 0 1 2 3 4 5 0",
        "", "NPOIN= 6",
        "0.0 0.0 0.0 0", "1.0 0.0 0.0 1", "0.0 1.0 0.0 2",
        "0.0 0.0 1.0 3", "1.0 0.0 1.0 4", "0.0 1.0 1.0 5",
        "", "NMARK= 5",
        "MARKER_TAG= bottom", "MARKER_ELEMS= 1", "5 0 1 2",
        "MARKER_TAG= top", "MARKER_ELEMS= 1", "5 3 4 5",
        "MARKER_TAG= a", "MARKER_ELEMS= 1", "9 0 2 5 3",
        "MARKER_TAG= b", "MARKER_ELEMS= 1", "9 0 3 4 1",
        "MARKER_TAG= c", "MARKER_ELEMS= 1", "9 1 4 5 2", ""], "\n");
}

/// The same wedge with the node list in GMSH order, for expect_gmsh_order_for_wedges.
string su2_wedge_gmsh()
{
    return join([
        "NDIME= 3", "", "NELEM= 1",
        "13 0 2 1 3 5 4 0",
        "", "NPOIN= 6",
        "0.0 0.0 0.0 0", "1.0 0.0 0.0 1", "0.0 1.0 0.0 2",
        "0.0 0.0 1.0 3", "1.0 0.0 1.0 4", "0.0 1.0 1.0 5",
        "", "NMARK= 5",
        "MARKER_TAG= bottom", "MARKER_ELEMS= 1", "5 0 1 2",
        "MARKER_TAG= top", "MARKER_ELEMS= 1", "5 3 4 5",
        "MARKER_TAG= a", "MARKER_ELEMS= 1", "9 0 2 5 3",
        "MARKER_TAG= b", "MARKER_ELEMS= 1", "9 0 3 4 1",
        "MARKER_TAG= c", "MARKER_ELEMS= 1", "9 1 4 5 2", ""], "\n");
}

string su2_pyramid()
{
    return join([
        "NDIME= 3", "", "NELEM= 1",
        "14 0 1 2 3 4 0",
        "", "NPOIN= 5",
        "0.0 0.0 0.0 0", "1.0 0.0 0.0 1", "1.0 1.0 0.0 2", "0.0 1.0 0.0 3",
        "0.5 0.5 1.0 4",
        "", "NMARK= 5",
        "MARKER_TAG= base", "MARKER_ELEMS= 1", "9 0 3 2 1",
        "MARKER_TAG= f1", "MARKER_ELEMS= 1", "5 0 1 4",
        "MARKER_TAG= f2", "MARKER_ELEMS= 1", "5 1 2 4",
        "MARKER_TAG= f3", "MARKER_ELEMS= 1", "5 2 3 4",
        "MARKER_TAG= f4", "MARKER_ELEMS= 1", "5 3 0 4", ""], "\n");
}

/// Elements written without the trailing cell index (D:1433 branch).
string su2_noindex()
{
    // A unit square split into two triangles, with the trailing cell index omitted from
    // every element line, and the shared diagonal 1-3 left out of the markers.
    return join([
        "NDIME= 2", "", "NELEM= 2",
        "5 0 1 3", "5 1 2 3",
        "", "NPOIN= 4", "0.0 0.0", "1.0 0.0", "1.0 1.0", "0.0 1.0",
        "", "NMARK= 4",
        "MARKER_TAG= south", "MARKER_ELEMS= 1", "3 0 1",
        "MARKER_TAG= east", "MARKER_ELEMS= 1", "3 1 2",
        "MARKER_TAG= north", "MARKER_ELEMS= 1", "3 2 3",
        "MARKER_TAG= west", "MARKER_ELEMS= 1", "3 3 0", ""], "\n");
}

/// Cell indices given out of natural order, so cells[indx] is scattered (D:1458).
string su2_shuffled()
{
    // The tri_su2 mesh with the trailing cell indices permuted, so cells[indx] is
    // scattered across the array (D:1458) while the geometry stays valid.
    return join([
        "NDIME= 2", "", "NELEM= 8",
        "5 0 1 3 4", "5 1 4 3 7", "5 1 2 4 1", "5 2 5 4 6",
        "5 3 4 6 0", "5 4 7 6 3", "5 4 5 7 2", "5 5 8 7 5",
        "", "NPOIN= 9",
        "0.0 0.0 0", "0.5 0.0 1", "1.0 0.0 2", "0.0 0.5 3", "0.5 0.5 4", "1.0 0.5 5",
        "0.0 1.0 6", "0.5 1.0 7", "1.0 1.0 8",
        "", "NMARK= 4",
        "MARKER_TAG= lower", "MARKER_ELEMS= 2", "3 0 1", "3 1 2",
        "MARKER_TAG= right", "MARKER_ELEMS= 2", "3 2 5", "3 5 8",
        "MARKER_TAG= upper", "MARKER_ELEMS= 2", "3 8 7", "3 7 6",
        "MARKER_TAG= left", "MARKER_ELEMS= 2", "3 6 3", "3 3 0", ""], "\n");
}

/// Two quads whose vertex cycles are clockwise, to drive the reversal at D:1528/:1541.
string su2_cw2d()
{
    return join([
        "NDIME= 2", "", "NELEM= 2",
        "9 0 3 4 1 0", "9 1 4 5 2 1",
        "", "NPOIN= 6",
        "0.0 0.0 0", "0.5 0.0 1", "1.0 0.0 2", "0.0 0.5 3", "0.5 0.5 4", "1.0 0.5 5",
        "", "NMARK= 4",
        "MARKER_TAG= south", "MARKER_ELEMS= 2", "3 0 1", "3 1 2",
        "MARKER_TAG= east", "MARKER_ELEMS= 1", "3 2 5",
        "MARKER_TAG= north", "MARKER_ELEMS= 2", "3 5 4", "3 4 3",
        "MARKER_TAG= west", "MARKER_ELEMS= 1", "3 3 0", ""], "\n");
}

/// A closed 2-cell mesh missing one external edge from its markers: the leftover face
/// becomes an ORPHAN_FACES boundary set (D:1762) and closure still holds.
string su2_orphan()
{
    return join([
        "NDIME= 2", "", "NELEM= 2",
        "9 0 1 4 3 0", "9 1 2 5 4 1",
        "", "NPOIN= 6",
        "0.0 0.0 0", "0.5 0.0 1", "1.0 0.0 2", "0.0 0.5 3", "0.5 0.5 4", "1.0 0.5 5",
        "", "NMARK= 3",
        "MARKER_TAG= south", "MARKER_ELEMS= 2", "3 0 1", "3 1 2",
        "MARKER_TAG= east", "MARKER_ELEMS= 1", "3 2 5",
        "MARKER_TAG= west", "MARKER_ELEMS= 1", "3 3 0", ""], "\n");
}

/// A single triangle with only two of its three edges marked, so the boundary vector
/// area is not zero and D:1776 builds its message with a format() that has no
/// conversion for the argument -- a FormatException, not the GeometryException the
/// source appears to throw.
string su2_orphan2()
{
    return join([
        "NDIME= 2", "", "NELEM= 1",
        "5 0 1 2 0",
        "", "NPOIN= 3", "0.0 0.0 0", "1.0 0.0 1", "0.0 1.0 2",
        "", "NMARK= 2",
        "MARKER_TAG= a", "MARKER_ELEMS= 1", "3 0 1",
        "MARKER_TAG= b", "MARKER_ELEMS= 1", "3 1 2", ""], "\n");
}

/// Same file, with the third edge marked, used for the failure rows below.
string su2_baditemcount()
{
    return join([
        "NDIME= 2", "", "NELEM= 1",
        "5 0 1 2 3 4 0",
        "", "NPOIN= 3", "0.0 0.0 0", "1.0 0.0 1", "0.0 1.0 2",
        "", "NMARK= 1", "MARKER_TAG= a", "MARKER_ELEMS= 1", "3 0 1", ""], "\n");
}

string su2_badtype()
{
    return join([
        "NDIME= 2", "", "NELEM= 1",
        "99 0 1 2 0",
        "", "NPOIN= 3", "0.0 0.0 0", "1.0 0.0 1", "0.0 1.0 2",
        "", "NMARK= 1", "MARKER_TAG= a", "MARKER_ELEMS= 1", "3 0 1", ""], "\n");
}

/// VTK 7 is a polygon, which convert_cell_type refuses, so D:1442 fires.
string su2_polygon()
{
    return join([
        "NDIME= 2", "", "NELEM= 1",
        "7 0 1 2 0",
        "", "NPOIN= 3", "0.0 0.0 0", "1.0 0.0 1", "0.0 1.0 2",
        "", "NMARK= 1", "MARKER_TAG= a", "MARKER_ELEMS= 1", "3 0 1", ""], "\n");
}

/// A triangle in a 3D mesh: convert_cell_type gives USGCell_type.triangle, which the
/// 3D switch at D:1660 does not handle.
string su2_triin3d()
{
    return join([
        "NDIME= 3", "", "NELEM= 1",
        "5 0 1 2 0",
        "", "NPOIN= 3", "0.0 0.0 0.0 0", "1.0 0.0 0.0 1", "0.0 1.0 0.0 2",
        "", "NMARK= 1", "MARKER_TAG= a", "MARKER_ELEMS= 1", "3 0 1", ""], "\n");
}

/// A tetra in a 2D mesh: the orientation loop at D:1520 has no case for it.
string su2_tetin2d()
{
    return join([
        "NDIME= 2", "", "NELEM= 1",
        "10 0 1 2 3 0",
        "", "NPOIN= 4", "0.0 0.0 0", "1.0 0.0 1", "0.0 1.0 2", "0.5 0.5 3",
        "", "NMARK= 1", "MARKER_TAG= a", "MARKER_ELEMS= 1", "3 0 1", ""], "\n");
}

/// Two identical quad cells: the second one lands on the left side of faces that already
/// have a left cell, so D:1614 fires.
string su2_duplicate_cell()
{
    return join([
        "NDIME= 2", "", "NELEM= 2",
        "9 0 1 2 3 0", "9 0 1 2 3 1",
        "", "NPOIN= 4", "0.0 0.0 0", "1.0 0.0 1", "1.0 1.0 2", "0.0 1.0 3",
        "", "NMARK= 4",
        "MARKER_TAG= a", "MARKER_ELEMS= 1", "3 0 1",
        "MARKER_TAG= b", "MARKER_ELEMS= 1", "3 1 2",
        "MARKER_TAG= c", "MARKER_ELEMS= 1", "3 2 3",
        "MARKER_TAG= d", "MARKER_ELEMS= 1", "3 3 0", ""], "\n");
}

/// A marker that names the shared internal edge of a two-cell strip (D:1734).
string su2_marker_interior()
{
    return join([
        "NDIME= 2", "", "NELEM= 2",
        "9 0 1 4 3 0", "9 1 2 5 4 1",
        "", "NPOIN= 6",
        "0.0 0.0 0", "0.5 0.0 1", "1.0 0.0 2", "0.0 0.5 3", "0.5 0.5 4", "1.0 0.5 5",
        "", "NMARK= 1",
        "MARKER_TAG= inside", "MARKER_ELEMS= 1", "3 1 4", ""], "\n");
}

/// A marker naming a vertex pair that is not a face of any cell (D:1739).
string su2_marker_unknown()
{
    // A quad whose marker names the 0-2 diagonal, which is not a face of the cell.
    return join([
        "NDIME= 2", "", "NELEM= 1",
        "9 0 1 2 3 0",
        "", "NPOIN= 4", "0.0 0.0 0", "1.0 0.0 1", "1.0 1.0 2", "0.0 1.0 3",
        "", "NMARK= 5",
        "MARKER_TAG= a", "MARKER_ELEMS= 1", "3 0 1",
        "MARKER_TAG= b", "MARKER_ELEMS= 1", "3 1 2",
        "MARKER_TAG= c", "MARKER_ELEMS= 1", "3 2 3",
        "MARKER_TAG= d", "MARKER_ELEMS= 1", "3 3 0",
        "MARKER_TAG= bogus", "MARKER_ELEMS= 1", "3 0 2", ""], "\n");
}

/// A closed triangle mesh with one marker edge missing, so the sum of boundary vector
/// areas is not zero (D:1775).
string su2_not_closed()
{
    // The same edge named by two different markers, so its vector area is counted twice
    // and the bounding vector area of the region is not zero (D:1769-1777). Missing the
    // conversion for the argument, D:1776 raises std.format.FormatException before the
    // GeometryException of D:1777 can ever be constructed.
    return join([
        "NDIME= 2", "", "NELEM= 1",
        "5 0 1 2 0",
        "", "NPOIN= 3", "0.0 0.0 0", "1.0 0.0 1", "0.0 1.0 2",
        "", "NMARK= 4",
        "MARKER_TAG= a", "MARKER_ELEMS= 1", "3 0 1",
        "MARKER_TAG= b", "MARKER_ELEMS= 1", "3 1 2",
        "MARKER_TAG= c", "MARKER_ELEMS= 1", "3 2 0",
        "MARKER_TAG= again", "MARKER_ELEMS= 1", "3 0 1", ""], "\n");
}

/// A self-intersecting (butterfly) quad: the cell closure test at D:1695 should not be
/// able to balance, so D:1702 fires with the vector area and the cell line.
string su2_selfintersecting_quad()
{
    return join([
        "NDIME= 2", "", "NELEM= 1",
        "9 0 1 2 3 0",
        "", "NPOIN= 4",
        "0.0 0.0 0", "1.0 1.0 1", "1.0 0.0 2", "0.0 1.0 3",
        "", "NMARK= 4",
        "MARKER_TAG= a", "MARKER_ELEMS= 1", "3 2 3",
        "MARKER_TAG= b", "MARKER_ELEMS= 1", "3 1 2",
        "MARKER_TAG= c", "MARKER_ELEMS= 1", "3 0 1",
        "MARKER_TAG= d", "MARKER_ELEMS= 1", "3 3 0", ""], "\n");
}

/// A 1 x 2 column of quads over x in [0, 0.5], y in [0, 1]. Its east edge (vertices 1-3
/// and 3-5) is the face the right block joins to.
string su2_left_block()
{
    return join([
        "NDIME= 2", "", "NELEM= 2",
        "9 0 1 3 2 0", "9 2 3 5 4 1",
        "", "NPOIN= 6",
        "0.0 0.0 0", "0.5 0.0 1", "0.0 0.5 2", "0.5 0.5 3", "0.0 1.0 4", "0.5 1.0 5",
        "", "NMARK= 4",
        "MARKER_TAG= south", "MARKER_ELEMS= 1", "3 0 1",
        "MARKER_TAG= east", "MARKER_ELEMS= 2", "3 1 3", "3 3 5",
        "MARKER_TAG= north", "MARKER_ELEMS= 1", "3 5 4",
        "MARKER_TAG= west", "MARKER_ELEMS= 2", "3 4 2", "3 2 0", ""], "\n");
}

/// The matching column over x in [0.5, 1]; its west edge is the join.
string su2_right_block()
{
    return join([
        "NDIME= 2", "", "NELEM= 2",
        "9 0 1 3 2 0", "9 2 3 5 4 1",
        "", "NPOIN= 6",
        "0.5 0.0 0", "1.0 0.0 1", "0.5 0.5 2", "1.0 0.5 3", "0.5 1.0 4", "1.0 1.0 5",
        "", "NMARK= 4",
        "MARKER_TAG= south", "MARKER_ELEMS= 1", "3 0 1",
        "MARKER_TAG= east", "MARKER_ELEMS= 2", "3 1 3", "3 3 5",
        "MARKER_TAG= north", "MARKER_ELEMS= 1", "3 5 4",
        "MARKER_TAG= west", "MARKER_ELEMS= 2", "3 4 2", "3 2 0", ""], "\n");
}

/// The same column moved to x in [5, 5.5], so nothing coincides.
string su2_far_block()
{
    return join([
        "NDIME= 2", "", "NELEM= 2",
        "9 0 1 3 2 0", "9 2 3 5 4 1",
        "", "NPOIN= 6",
        "5.0 0.0 0", "5.5 0.0 1", "5.0 0.5 2", "5.5 0.5 3", "5.0 1.0 4", "5.5 1.0 5",
        "", "NMARK= 4",
        "MARKER_TAG= south", "MARKER_ELEMS= 1", "3 0 1",
        "MARKER_TAG= east", "MARKER_ELEMS= 2", "3 1 3", "3 3 5",
        "MARKER_TAG= north", "MARKER_ELEMS= 1", "3 5 4",
        "MARKER_TAG= west", "MARKER_ELEMS= 2", "3 4 2", "3 2 0", ""], "\n");
}

/// A single hexahedron, x in [0,1] (left) or [1,2] (right), y and z in [0,1], points in
/// the VTK order the face cycles of D:1667-1673 assume: 0..3 counter-clockwise around the
/// z=0 face and 4..7 the same cycle on z=1. Six markers, one per face, so the import
/// closes. The two blocks share the x=1 plane: the left block's iplus face (1,2,6,5) and
/// the right block's iminus face (0,3,7,4) have the same four coordinates.
string su2_hex_block(bool right)
{
    double x0 = right ? 1.0 : 0.0;
    double x1 = x0 + 1.0;
    string[] L;
    L ~= "NDIME= 3";
    L ~= "";
    L ~= "NELEM= 1";
    L ~= "12 0 1 2 3 4 5 6 7 0";
    L ~= "";
    L ~= "NPOIN= 8";
    L ~= format("%.17g %.17g %.17g 0", x0, 0.0, 0.0);
    L ~= format("%.17g %.17g %.17g 1", x1, 0.0, 0.0);
    L ~= format("%.17g %.17g %.17g 2", x1, 1.0, 0.0);
    L ~= format("%.17g %.17g %.17g 3", x0, 1.0, 0.0);
    L ~= format("%.17g %.17g %.17g 4", x0, 0.0, 1.0);
    L ~= format("%.17g %.17g %.17g 5", x1, 0.0, 1.0);
    L ~= format("%.17g %.17g %.17g 6", x1, 1.0, 1.0);
    L ~= format("%.17g %.17g %.17g 7", x0, 1.0, 1.0);
    L ~= "";
    L ~= "NMARK= 6";
    L ~= "MARKER_TAG= north";
    L ~= "MARKER_ELEMS= 1";
    L ~= "9 2 3 7 6";
    L ~= "MARKER_TAG= east";
    L ~= "MARKER_ELEMS= 1";
    L ~= "9 1 2 6 5";
    L ~= "MARKER_TAG= south";
    L ~= "MARKER_ELEMS= 1";
    L ~= "9 1 0 4 5";
    L ~= "MARKER_TAG= west";
    L ~= "MARKER_ELEMS= 1";
    L ~= "9 0 3 7 4";
    L ~= "MARKER_TAG= top";
    L ~= "MARKER_ELEMS= 1";
    L ~= "9 4 5 6 7";
    L ~= "MARKER_TAG= bottom";
    L ~= "MARKER_ELEMS= 1";
    L ~= "9 0 1 2 3";
    L ~= "";
    return join(L, "\n");
}

string su2_scrambled_hex()
{
    // One hexahedron, 8 points of a box, listed as 0,2,1,3 on the bottom and 4,6,5,7 on
    // top, so the corner cycles add_face_to_cell builds are crossed.
    return join([
        "NDIME= 3", "", "NELEM= 1",
        "12 0 2 1 3 4 6 5 7 0",
        "", "NPOIN= 8",
        "0.0 0.0 0.0 0", "1.0 0.0 0.0 1", "0.0 1.0 0.0 2", "1.0 1.0 0.0 3",
        "0.0 0.0 1.0 4", "1.0 0.0 1.0 5", "0.0 1.0 1.0 6", "1.0 1.0 1.0 7",
        "", "NMARK= 6",
        "MARKER_TAG= front", "MARKER_ELEMS= 1", "9 0 1 3 2",
        "MARKER_TAG= back", "MARKER_ELEMS= 1", "9 4 5 7 6",
        "MARKER_TAG= bottom", "MARKER_ELEMS= 1", "9 0 1 5 4",
        "MARKER_TAG= top", "MARKER_ELEMS= 1", "9 2 3 7 6",
        "MARKER_TAG= left", "MARKER_ELEMS= 1", "9 0 2 6 4",
        "MARKER_TAG= right", "MARKER_ELEMS= 1", "9 1 3 7 5", ""], "\n");
}

/// The whole mesh with one header line removed, to drive the "didn't find the target"
/// return of "" (D:1405) into to!int/to!size_t.
string su2_missing(string drop)
{
    string[] keep;
    foreach (L; split(tri_su2(), "\n")) {
        if (L.length >= 5 && L[0 .. 5] == "NELEM") {
            if (drop == "NELEM") continue;
        }
        if (L.length >= 5 && L[0 .. 5] == "NPOIN") {
            if (drop == "NPOIN") continue;
        }
        if (L.length >= 5 && L[0 .. 5] == "NDIME") {
            if (drop == "NDIME") continue;
        }
        if (L.length >= 5 && L[0 .. 5] == "NMARK") {
            if (drop == "NMARK") continue;
        }
        keep ~= L;
    }
    return join(keep, "\n");
}

/// A header line with a non-numeric value.
string su2_bad_value(string key, string val)
{
    string[] keep;
    foreach (L; split(tri_su2(), "\n")) {
        if (L.length >= 5 && L[0 .. 5] == key) { keep ~= key ~ "= " ~ val; continue; }
        keep ~= L;
    }
    return join(keep, "\n");
}

/// A header line with a count that disagrees with the number of lines that follow.
string su2_bad_count(string key, string val)
{
    return su2_bad_value(key, val);
}

string su2_bad_coord_line()
{
    string[] keep;
    foreach (L; split(tri_su2(), "\n")) {
        if (L == "0.5 0.0 1") { keep ~= "zero zero one"; continue; }
        keep ~= L;
    }
    return join(keep, "\n");
}

string su2_nan_coord_line()
{
    string[] keep;
    foreach (L; split(tri_su2(), "\n")) {
        if (L == "0.5 0.0 1") { keep ~= "nan 0.0 1"; continue; }
        keep ~= L;
    }
    return join(keep, "\n");
}

string su2_big_coord_line()
{
    string[] keep;
    foreach (L; split(tri_su2(), "\n")) {
        if (L == "0.5 0.0 1") { keep ~= "1.23456789012345e+300 0.0 1"; continue; }
        keep ~= L;
    }
    return join(keep, "\n");
}

/// A vertex line whose trailing index is out of range for the declared NPOIN.
string su2_oob_vertex_index()
{
    string[] keep;
    foreach (L; split(tri_su2(), "\n")) {
        if (L == "0.5 0.0 1") { keep ~= "0.5 0.0 99"; continue; }
        keep ~= L;
    }
    return join(keep, "\n");
}

/// An element line whose trailing cell index is out of range for the declared NELEM.
string su2_oob_cell_index()
{
    string[] keep;
    foreach (L; split(tri_su2(), "\n")) {
        if (L == "5 0 1 3 0") { keep ~= "5 0 1 3 99"; continue; }
        keep ~= L;
    }
    return join(keep, "\n");
}

/// A marker with no tag at all: "MARKER_TAG=" gives tokens[1] == "".
string su2_empty_marker_tag()
{
    string[] keep;
    foreach (L; split(tri_su2(), "\n")) {
        if (L == "MARKER_TAG= lower") { keep ~= "MARKER_TAG="; continue; }
        keep ~= L;
    }
    return join(keep, "\n");
}

//-----------------------------------------------------------------

void main()
{
    if (!std.file.exists(TD)) std.file.mkdir(TD);

    //================ us.name.* — cell-type tables (D:40-110, :278-280)
    {
        writefln("us.name.enum none=%d triangle=%d quad=%d polygon=%d tetra=%d wedge=%d hexahedron=%d pyramid=%d line=%d",
                 cast(int)USGCell_type.none, cast(int)USGCell_type.triangle,
                 cast(int)USGCell_type.quad, cast(int)USGCell_type.polygon,
                 cast(int)USGCell_type.tetra, cast(int)USGCell_type.wedge,
                 cast(int)USGCell_type.hexahedron, cast(int)USGCell_type.pyramid,
                 cast(int)USGCell_type.line);
        foreach (i; 0 .. 9) {
            writefln("us.name.cell_names[%d]=[%s]", i, cell_names[i]);
        }
        tw("us.name.cell_names.oob", () { writefln("us.name.cell_names[9]=[%s]", cell_names[9]); });
        foreach (i; 0 .. 9) {
            writefln("us.name.vtk_element_types[%d]=%d", i, vtk_element_types[i]);
        }
        tw("us.name.vtk_element_types.oob", () { writefln("us.name.vtk_element_types[9]=%d", vtk_element_types[9]); });
        writefln("us.name.vtklen=%d", vtk_element_types.length);
        writefln("us.name.cellnameslen=%d", cell_names.length);
        // The VTK numbers themselves (geom/elements/nomenclature.d:39-55).
        writefln("us.name.vtkenum vertex=%d polyvertex=%d line=%d polyline=%d triangle=%d strip=%d polygon=%d pixel=%d quad=%d tetra=%d voxel=%d hex=%d wedge=%d pyramid=%d",
                 VTKElement.vertex, VTKElement.polyvertex, VTKElement.line,
                 VTKElement.polyline, VTKElement.triangle, VTKElement.triangle_strip,
                 VTKElement.polygon, VTKElement.pixel, VTKElement.quad, VTKElement.tetra,
                 VTKElement.voxel, VTKElement.hexahedron, VTKElement.wedge, VTKElement.pyramid);
        foreach (v; [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 99, -1]) {
            writefln("us.name.convert %d->%d name=[%s] nv=%d", v, cast(int)convert_cell_type(v),
                     to!string(convert_cell_type(v)), n_vertices_for_vtk_element(v));
        }
        // cell_type_from_name: "polygon" is deliberately absent (D:101 commented out).
        foreach (s; ["none", "triangle", "quad", "polygon", "tetra", "wedge",
                     "hexahedron", "pyramid", "line", "", "None", "QUAD", "quad ", " cube"]) {
            writefln("us.name.fromname [%s]->%d name=[%s]", s, cast(int)cell_type_from_name(s),
                     to!string(cell_type_from_name(s)));
        }
        // to!string of the enum, which is what D:280's message shows.
        foreach (t; [USGCell_type.none, USGCell_type.triangle, USGCell_type.quad,
                     USGCell_type.polygon, USGCell_type.tetra, USGCell_type.wedge,
                     USGCell_type.hexahedron, USGCell_type.pyramid, USGCell_type.line]) {
            writefln("us.name.tostr %d=[%s]", cast(int)t, to!string(t));
        }
        // to!USGCell_type of raw integers (D:278, from a binary file).
        foreach (v; [0, 1, 2, 3, 4, 5, 6, 7, 8]) {
            auto t = to!USGCell_type(v);
            writefln("us.name.tenum[%d] value=%d name=[%s]", v, cast(int)t, to!string(t));
        }
        foreach (v; [9, 42, -1, int.min, int.max]) {
            twd(format("us.name.tenum[%d]", v), 278, () {
                auto t = to!USGCell_type(v);
                writefln("us.name.tenum[%d]|value=%d", v, cast(int)t);
            });
        }
        writefln("us.name.gridtype=[%s] typename=[%s]", to!string(Grid_t.unstructured_grid),
                 gridTypeName(Grid_t.unstructured_grid));
        twd("us.name.gridtype.unknown", 37, () { gridTypeFromName("bogus_grid"); });
    }

    //================ us.tag.* — makeFaceTag (D:112-132)
    {
        size_t[] e = [];
        writefln("us.tag.empty=[%s] len=%d", makeFaceTag(e), makeFaceTag(e).length);
        writefln("us.tag.one=[%s]", makeFaceTag([cast(size_t)7]));
        writefln("us.tag.2=[%s]", makeFaceTag([cast(size_t)3, cast(size_t)1]));
        writefln("us.tag.2dup=[%s]", makeFaceTag([cast(size_t)2, cast(size_t)2]));
        writefln("us.tag.3=[%s]", makeFaceTag([cast(size_t)9, cast(size_t)1, cast(size_t)5]));
        writefln("us.tag.4=[%s]", makeFaceTag([cast(size_t)11, cast(size_t)2, cast(size_t)7, cast(size_t)0]));
        writefln("us.tag.5=[%s]", makeFaceTag([cast(size_t)30, cast(size_t)4, cast(size_t)4, cast(size_t)12, cast(size_t)1]));
        writefln("us.tag.max=[%s]", makeFaceTag([size_t.max, cast(size_t)0]));
        writefln("us.tag.max2=[%s]", makeFaceTag([size_t.max, size_t.max - 1, cast(size_t)1]));
        // makeFaceTag dup()s before sorting, so the caller's array keeps its order.
        size_t[] orig = [cast(size_t)9, cast(size_t)4, cast(size_t)7];
        auto t = makeFaceTag(orig);
        writefln("us.tag.noreorder tag=[%s] caller=%s", t, orig);
        // A tag is not injective: the same vertex set in a wrong cycle gives the same tag.
        writefln("us.tag.collide [%s]==[%s] : %s", makeFaceTag([cast(size_t)0, cast(size_t)1, cast(size_t)2]),
                 makeFaceTag([cast(size_t)0, cast(size_t)2, cast(size_t)1]),
                 makeFaceTag([cast(size_t)0, cast(size_t)1, cast(size_t)2]) == makeFaceTag([cast(size_t)0, cast(size_t)2, cast(size_t)1]));
    }

    //================ us.orient.* — sameOrientation (D:134-152)
    {
        writefln("us.orient.2.same=%s", sameOrientation([cast(size_t)0, cast(size_t)1], [cast(size_t)0, cast(size_t)1]));
        writefln("us.orient.2.rev=%s", sameOrientation([cast(size_t)0, cast(size_t)1], [cast(size_t)1, cast(size_t)0]));
        writefln("us.orient.3.cyc0=%s", sameOrientation([cast(size_t)0, cast(size_t)1, cast(size_t)2], [cast(size_t)0, cast(size_t)1, cast(size_t)2]));
        writefln("us.orient.3.cyc1=%s", sameOrientation([cast(size_t)0, cast(size_t)1, cast(size_t)2], [cast(size_t)1, cast(size_t)2, cast(size_t)0]));
        writefln("us.orient.3.cyc2=%s", sameOrientation([cast(size_t)0, cast(size_t)1, cast(size_t)2], [cast(size_t)2, cast(size_t)0, cast(size_t)1]));
        writefln("us.orient.3.swap=%s", sameOrientation([cast(size_t)0, cast(size_t)1, cast(size_t)2], [cast(size_t)0, cast(size_t)2, cast(size_t)1]));
        writefln("us.orient.4.cyc=%s", sameOrientation([cast(size_t)1, cast(size_t)2, cast(size_t)3, cast(size_t)0], [cast(size_t)2, cast(size_t)3, cast(size_t)0, cast(size_t)1]));
        writefln("us.orient.4.rev=%s", sameOrientation([cast(size_t)1, cast(size_t)2, cast(size_t)3, cast(size_t)0], [cast(size_t)1, cast(size_t)0, cast(size_t)3, cast(size_t)2]));
        // The empty pair: lengths match, isPermutation of two empty ranges is true, and
        // the cycle loop runs zero times, so the answer is false.
        writefln("us.orient.empty=%s", sameOrientation([], []));
        writefln("us.orient.1pt=%s", sameOrientation([cast(size_t)4], [cast(size_t)4]));
        twd("us.orient.lenmismatch", 136, () { sameOrientation([cast(size_t)0, cast(size_t)1], [cast(size_t)0, cast(size_t)1, cast(size_t)2]); });
        twd("us.orient.notpermu", 137, () { sameOrientation([cast(size_t)0, cast(size_t)1, cast(size_t)2], [cast(size_t)0, cast(size_t)1, cast(size_t)3]); });
        // FULL SWEEP. D's comment (D:142-143) says the cycle may start anywhere and the
        // loop with bringToFront (D:147) is meant to try every cycle. Measured: it does
        // not. Every cyclic rotation of a 3-point face returns false and one
        // non-cyclic permutation returns true, so the port must reproduce D's actual
        // tested sequence, not the intended one.
        foreach (p; perms3()) {
            writefln("us.orient.perm3 [%s] vs [0, 1, 2] = %s", p, sameOrientation([cast(size_t)0,1,2], p));
        }
        foreach (p; perms4()) {
            writefln("us.orient.perm4 [%s] vs [1, 2, 3, 0] = %s", p,
                     sameOrientation([cast(size_t)1,2,3,0], p));
        }
        // The lists D actually compares, printed by walking the same bringToFront loop.
        {
            size_t[] test_list = [cast(size_t)2, cast(size_t)0, cast(size_t)1];
            size_t[] a = [cast(size_t)0, cast(size_t)1, cast(size_t)2];
            writefln("us.orient.seq3 start=%s", test_list);
            foreach (j; 0 .. test_list.length) {
                writefln("us.orient.seq3 j=%d test=%s equal=%s", j, test_list, equal(a, test_list));
                bringToFront(test_list[1..$], test_list[$-1..$]);
            }
            size_t[] tl4 = [cast(size_t)2, cast(size_t)3, cast(size_t)0, cast(size_t)1];
            writefln("us.orient.seq4 start=%s", tl4);
            foreach (j; 0 .. tl4.length) {
                writefln("us.orient.seq4 j=%d test=%s", j, tl4);
                bringToFront(tl4[1..$], tl4[$-1..$]);
            }
        }
        // sameOrientation dup()s its second argument, so the caller's lists survive.
        size_t[] A = [cast(size_t)0, cast(size_t)1, cast(size_t)2];
        size_t[] B = [cast(size_t)2, cast(size_t)0, cast(size_t)1];
        auto r = sameOrientation(A, B);
        writefln("us.orient.noreorder r=%s A=%s B=%s", r, A, B);
    }

    //================ us.tstr.* — the to!string spellings usgrid uses (29 sites)
    {
        // Integers (D:204, :205, :305-310, :403-406, :2060, :2065, and the i of D:490/493).
        foreach (v; [cast(size_t)0, cast(size_t)1, cast(size_t)12, cast(size_t)100000]) {
            writefln("us.tstr.size_t[%d]=[%s]", v, to!string(v));
        }
        foreach (v; [0, 1, -1, 100000, int.min, int.max]) {
            writefln("us.tstr.int[%d]=[%s]", v, to!string(v));
        }
        // char[] (D:374, :1343, :1350) — element-wise, not a reinterpretation.
        foreach (s; ["", "x", "unstructured_grid 1.0", "abc"]) {
            writefln("us.tstr.char[]=[%s] len=%d", to!string(cast(char[])s), to!string(cast(char[])s).length);
        }
        auto zn = to!string(cast(char[])[0x61, 0x00, 0x62]);
        writefln("us.tstr.char[] nul hex=%s len=%d printable=%s", hexb(zn), zn.length,
                 replace(zn, "\0", "<nul>"));
        // Doubles: D uses %.6g. These are the values that actually reach messages in
        // usgrid.d: the tol of D:458, the scale of D:1781/:1901, and Vector3 components.
        double[] vals = [0.0, -0.0, 1.0, 2.0, 0.1, 1.1, 0.5, 0.55, 0.325, 0.2, 1.0/3.0,
                         1e-9, 1.0e-5, 1.0e-4, 123456.789, 1e10, 1e-10, 1.0e20,
                         0.000001, 0.0000001, 1.0/7.0, 2.0/3.0, -1.0/3.0, 1e100];
        foreach (d; vals) {
            writefln("us.tstr.double bits=%s to!string=[%s] %%.6g=[%s] %%.17g=[%s] differ=%s",
                     bits(d), to!string(d), format("%g", d), format("%.17g", d),
                     to!string(d) != format("%.17g", d));
        }
        // The scale=1.0 case of D:1781's writefln("... scale=%s ...") and D:1782.
        foreach (s; [1.0, 25.4, 0.0254, 1e-3]) {
            writefln("us.tstr.scale to!string=[%s] fmts=[%s]", to!string(s), format("%s", s));
        }
        // Vector3 goes through its own toString, which is to!string per component.
        foreach (v; [Vector3(0.0, 0.1), Vector3(1.0, 1.1), Vector3(0.55, 0.325),
                     Vector3(1.0/3.0, 2.0/3.0, 1e-9), Vector3(123456.789, -0.0000001, 0)]) {
            writefln("us.tstr.vector3 to!string=[%s] %%.17g=[(%s, %s, %s)]", to!string(v),
                     format("%.17g", v.x), format("%.17g", v.y), format("%.17g", v.z));
        }
        // A Vector3[] array in a format("%s") — D puts the elements in brackets.
        writefln("us.tstr.varr=[%s]", to!string([Vector3(0.0, 0.1), Vector3(1.0, 1.1)]));
        writefln("us.tstr.varr.empty=[%s]", to!string(new Vector3[0]));
    }

    //================ us.face.* — USGFace (D:154-216)
    {
        auto f = new USGFace(cast(size_t[])[3, 1, 2]);
        writefln("us.face.ctor toString=[%s]", to!string(f));
        writefln("us.face.ctor toIOString=[%s] len=%d", f.toIOString(), f.toIOString().length);
        writefln("us.face.ctor vtx=%s tag=%s onbnd=%s L=%s R=%s", f.vtx_id_list, f.tag,
                 f.is_on_boundary, (f.left_cell ? "set" : "null"), (f.right_cell ? "set" : "null"));
        // .dup: the constructor copies the caller's list (D:169).
        size_t[] src = [cast(size_t)5, cast(size_t)6];
        auto f2 = new USGFace(src);
        src[0] = 99;
        writefln("us.face.dup src=%s face=%s", src, f2.vtx_id_list);
        // Empty face.
        auto f3 = new USGFace(cast(size_t[])[]);
        writefln("us.face.empty toString=[%s] toIOString=[%s]", to!string(f3), f3.toIOString());
        // The tag field is never filled by any constructor or method.
        writefln("us.face.tagfield=[%s] len=%d", f.tag, f.tag.length);

        // string constructor (D:180): "n id0 .. idn-1", stripped and split.
        auto fs = new USGFace("2 7 9");
        writefln("us.face.str vtx=%s io=[%s]", fs.vtx_id_list, fs.toIOString());
        auto fs2 = new USGFace("   3   10   20   30  ");
        writefln("us.face.str2 vtx=%s io=[%s]", fs2.vtx_id_list, fs2.toIOString());
        auto fs3 = new USGFace("0");
        writefln("us.face.str0 vtx=%d io=[%s]", fs3.vtx_id_list.length, fs3.toIOString());
        twd("us.face.str.empty", 184, () { auto q = new USGFace(""); });
        twd("us.face.str.countonly", 185, () { auto q = new USGFace("2 5"); });
        twd("us.face.str.nonint", 185, () { auto q = new USGFace("2 5 x"); });
        twd("us.face.str.negcount", 184, () { auto q = new USGFace("-1 5"); });
        twd("us.face.str.bigcount", 184, () { auto q = new USGFace("9999999999999999999 1"); });
        twd("us.face.str.plus", 184, () { auto q = new USGFace("2 +1 +2"); });
        twd("us.face.str.hex", 184, () { auto q = new USGFace("2 0x1 2"); });

        // copy constructor (D:172): list copied, cell links dropped (D:174-177 comment).
        auto fh = new USGFace(cast(size_t[])[1, 2]);
        auto cc = new USGCell(USGCell_type.quad, 3, [0,1,2,3], [0], [1]);
        fh.left_cell = cc;
        fh.right_cell = cc;
        auto fc = new USGFace(fh);
        writefln("us.face.copy vtx=%s L=%s R=%s onbnd=%s tag=[%s]", fc.vtx_id_list,
                 (fc.left_cell ? "set" : "null"), (fc.right_cell ? "set" : "null"),
                 fc.is_on_boundary, fc.tag);
        fh.vtx_id_list[0] = 77;
        writefln("us.face.copy.indep orig=%s copy=%s", fh.vtx_id_list, fc.vtx_id_list);

        // rawWrite/rawRead round trip (D:188-194, :209-215).
        string p = TD ~ "/face.raw";
        {
            auto fout = File(p, "wb");
            auto fr = new USGFace(cast(size_t[])[4, 5, 6]);
            fr.rawWrite(fout);
            new USGFace(cast(size_t[])[]).rawWrite(fout);
            fout.close();
        }
        writefln("us.face.raw len=%d hex=%s", len_of(p), hex_of(p));
        {
            auto fin = File(p, "rb");
            auto r1 = new USGFace(fin);
            auto r2 = new USGFace(fin);
            writefln("us.face.roundtrip r1=%s r2=%s io1=[%s] io2=[%s]", to!string(r1),
                     to!string(r2), r1.toIOString(), r2.toIOString());
            twd("us.face.roundtrip.eof", 193, () { auto r3 = new USGFace(fin); });
        }
        // A face record whose declared count exceeds the bytes present.
        string p2 = TD ~ "/face.trunc";
        {
            auto fout = File(p2, "wb");
            int[1] b = [3];
            fout.rawWrite(b);
            int[1] c = [7];
            fout.rawWrite(c);
            fout.close();
        }
        {
            auto fin = File(p2, "rb");
            twd("us.face.truncated", 193, () { auto r = new USGFace(fin); });
        }
        // A negative declared count in a raw record.
        string p3 = TD ~ "/face.neg";
        {
            auto fout = File(p3, "wb");
            int[1] b = [-2];
            fout.rawWrite(b);
            fout.close();
        }
        {
            auto fin = File(p3, "rb");
            twd("us.face.negcount", 192, () { auto r = new USGFace(fin); });
        }
    }

    //================ us.cell.* — USGCell (D:218-326)
    {
        auto c = new USGCell(USGCell_type.quad, 11, [0,1,2,3], [4,5,6,7], [1,1,-1,-1]);
        writefln("us.cell.ctor toString=[%s]", to!string(c));
        writefln("us.cell.ctor toIOString=[%s] len=%d", c.toIOString(), c.toIOString().length);
        // The three lists are .dup'd (D:231-233).
        size_t[] vs = [cast(size_t)1, cast(size_t)2];
        size_t[] fl = [cast(size_t)3];
        int[] ol = [1];
        auto c2 = new USGCell(USGCell_type.triangle, 0, vs, fl, ol);
        vs[0] = 98; fl[0] = 99; ol[0] = -9;
        writefln("us.cell.dup vtx=%s faces=%s outsigns=%s", c2.vtx_id_list, c2.face_id_list,
                 c2.outsign_list);
        // THE SHADOWED PARAMETER (D:236-238): `id = other.id` writes the parameter, so
        // this.id keeps size_t.init (0). Both the passed id and other.id are lost.
        auto c3 = new USGCell(c, 77);
        writefln("us.cell.copyid orig=%d passed=77 copy=%d", c.id, c3.id);
        writefln("us.cell.copy lists=%s %s %s", c3.vtx_id_list, c3.face_id_list, c3.outsign_list);
        c.vtx_id_list[0] = 900;
        writefln("us.cell.copy.indep orig=%s copy=%s", c.vtx_id_list, c3.vtx_id_list);
        // A cell with no faces at all (as get_boundary_grid makes them, D:1096-1100).
        auto c4 = new USGCell(USGCell_type.line, 0, [0,1], cast(size_t[])[], cast(int[])[]);
        writefln("us.cell.bare toString=[%s] io=[%s]", to!string(c4), c4.toIOString());
        // A cell of type none (the constructors do not validate the cell_type argument).
        auto c5 = new USGCell(USGCell_type.none, 0, [0], [0], [1]);
        writefln("us.cell.none io=[%s] tostr=[%s]", c5.toIOString(), to!string(c5));
        tw("us.cell.none.cell_names", () { writefln("us.cell.none.name=%s", cell_names[c5.cell_type]); });
        tw("us.cell.none.vtktypes", () { writefln("us.cell.none.vtk=%d", vtk_element_types[c5.cell_type]); });

        // string constructor (D:245).
        auto cs = new USGCell("quad vtx 4 0 1 2 3 faces 4 5 6 7 8 outsigns 4 1 1 -1 -1", 3);
        writefln("us.cell.str id=%d io=[%s]", cs.id, cs.toIOString());
        writefln("us.cell.str lists=%s %s %s", cs.vtx_id_list, cs.face_id_list, cs.outsign_list);
        auto cs0 = new USGCell("triangle vtx 3 0 1 2 faces 0 outsigns 0", 0);
        writefln("us.cell.str.emptylists id=%d io=[%s] faces=%d outsigns=%d", cs0.id,
                 cs0.toIOString(), cs0.face_id_list.length, cs0.outsign_list.length);
        twd("us.cell.str.badtype", 253, () { auto q = new USGCell("cube vtx 4 0 1 2 3 faces 0 outsigns 0", 0); });
        twd("us.cell.str.polygon", 253, () { auto q = new USGCell("polygon vtx 4 0 1 2 3 faces 0 outsigns 0", 0); });
        twd("us.cell.str.empty", 251, () { auto q = new USGCell("", 0); });
        twd("us.cell.str.short", 256, () { auto q = new USGCell("quad vtx 4 0 1", 0); });
        twd("us.cell.str.badint", 258, () { auto q = new USGCell("quad vtx 4 0 x 2 3 faces 0 outsigns 0", 0); });
        twd("us.cell.str.negcount", 256, () { auto q = new USGCell("quad vtx -1 faces 0 outsigns 0", 0); });
        twd("us.cell.str.nosigns", 266, () { auto q = new USGCell("quad vtx 2 0 1 faces 1 3", 0); });
        twd("us.cell.str.junkword", 256, () { auto q = new USGCell("quad VTX 2 0 1 faces 0 outsigns 0", 0); });

        // File constructor (D:272) and the cell_type==none throw (D:279-281).
        string p = TD ~ "/cell.raw";
        {
            auto fout = File(p, "wb");
            int[1] z = [0];
            fout.rawWrite(z);
            fout.close();
        }
        {
            auto fin = File(p, "rb");
            twd("us.cell.raw.none", 280, () { auto q = new USGCell(fin, 0); });
        }
        string p4 = TD ~ "/cell.raw2";
        {
            auto fout = File(p4, "wb");
            auto w = new USGCell(USGCell_type.wedge, 0, [0,1,2,3,4,5], [1,2,3,4,5], [1,1,1,1,-1]);
            w.rawWrite(fout);
            fout.close();
        }
        {
            auto fin = File(p4, "rb");
            auto r = new USGCell(fin, 42);
            writefln("us.cell.roundtrip id=%d io=[%s] len=%d hex=%s", r.id, r.toIOString(),
                     len_of(p4), hex_of(p4));
            twd("us.cell.roundtrip.eof", 287, () { auto r2 = new USGCell(fin, 43); });
        }
    }

    //================ us.bfs.* — BoundaryFaceSet (D:328-421)
    {
        auto b = new BoundaryFaceSet("wall", [1,2,3], [1,-1,1]);
        writefln("us.bfs.ctor toString=[%s]", to!string(b));
        writefln("us.bfs.ctor toIOString=[%s] len=%d", b.toIOString(), b.toIOString().length);
        size_t[] fl = [cast(size_t)9];
        int[] ol = [1];
        auto b2 = new BoundaryFaceSet("t", fl, ol);
        fl[0] = 99; ol[0] = -9;
        writefln("us.bfs.dup faces=%s outsigns=%s", b2.face_id_list, b2.outsign_list);
        // single-token string form (D:345-347): tag only, empty lists.
        auto b3 = new BoundaryFaceSet("justatag");
        writefln("us.bfs.tagonly toIOString=[%s] faces=%d outsigns=%d", b3.toIOString(),
                 b3.face_id_list.length, b3.outsign_list.length);
        twd("us.bfs.tagonly.tostring", 393, () { writefln("us.bfs.tagonly.repr=[%s]", to!string(b3)); });
        // full string form (D:348-364).
        auto b4 = new BoundaryFaceSet("wall faces 3 1 2 3 outsigns 3 1 -1 1");
        writefln("us.bfs.str toIOString=[%s]", b4.toIOString());
        twd("us.bfs.str.assert", 362, () { auto q = new BoundaryFaceSet("wall faces 2 1 2 outsigns 3 1 -1 1"); });
        twd("us.bfs.str.empty", 351, () { auto q = new BoundaryFaceSet(""); });
        twd("us.bfs.str.short", 353, () { auto q = new BoundaryFaceSet("wall faces 2 1"); });
        twd("us.bfs.str.badint", 353, () { auto q = new BoundaryFaceSet("wall faces z 1 2 outsigns 0"); });
        twd("us.bfs.str.negcount", 353, () { auto q = new BoundaryFaceSet("wall faces -1 outsigns 0"); });
        twd("us.bfs.str.zeros", 362, () { auto q = new BoundaryFaceSet("wall faces 0 outsigns 0"); });
        // Spaces inside the tag: split() takes the first token as the tag and the
        // second as the "faces" keyword, so a 3-token head throws in to!int.
        twd("us.bfs.spacedtag", 353, () {
            auto q = new BoundaryFaceSet("my tag faces 1 0 outsigns 1 1");
            writefln("us.bfs.spacedtag tag=[%s] io=[%s]", q.tag, q.toIOString());
        });
        auto b5 = new BoundaryFaceSet("my_tag faces 1 0 outsigns 1 1");
        writefln("us.bfs.underscoretag tag=[%s] toIOString=[%s]", b5.tag, b5.toIOString());
        auto b6 = new BoundaryFaceSet("", cast(size_t[])[], cast(int[])[]);
        writefln("us.bfs.emptytag tag=[%s] io=[%s]", b6.tag, b6.toIOString());
        twd("us.bfs.emptytag.tostring", 393, () { writefln("us.bfs.emptytag.repr=[%s]", to!string(b6)); });
        // copy constructor (D:382-387).
        auto b7 = new BoundaryFaceSet(b);
        b.face_id_list[0] = 500;
        writefln("us.bfs.copy indep orig=%s copy=%s tag=[%s]", b.face_id_list, b7.face_id_list, b7.tag);
        // rawWrite/rawRead (D:367-380, :410-420), including the zero-length tag branch.
        string p = TD ~ "/bfs.raw";
        {
            auto fout = File(p, "wb");
            new BoundaryFaceSet("inlet", [1,2], [1,-1]).rawWrite(fout);
            new BoundaryFaceSet("", [3], [1]).rawWrite(fout);
            fout.close();
        }
        writefln("us.bfs.raw len=%d hex=%s", len_of(p), hex_of(p));
        {
            auto fin = File(p, "rb");
            auto r1 = new BoundaryFaceSet(fin);
            auto r2 = new BoundaryFaceSet(fin);
            writefln("us.bfs.roundtrip r1io=[%s] r2io=[%s] r2tag=[%s]", r1.toIOString(),
                     r2.toIOString(), r2.tag);
            twd("us.bfs.roundtrip.eof", 377, () { auto r3 = new BoundaryFaceSet(fin); });
        }
        // A negative tag length in a raw record (D:371 branch is `> 0`).
        string p2 = TD ~ "/bfs.negtag";
        {
            auto fout = File(p2, "wb");
            int[1] n = [-3]; fout.rawWrite(n);
            int[1] z = [0]; fout.rawWrite(z); fout.rawWrite(z);
            fout.close();
        }
        {
            auto fin = File(p2, "rb");
            twd("us.bfs.negtaglen", 376, () { auto r = new BoundaryFaceSet(fin); });
        }
    }

    //================ us.ctor.* — the blank grid (D:434-438), no validation at all
    {
        foreach (d; [1, 2, 3, 0, -1, 7]) {
            auto g = new UnstructuredGrid(d, format("blank%02d", d));
            writefln("us.ctor.d%d dims=%d nvertices=%d nfaces=%d ncells=%d nboundaries=%d ninteriorfaces=%d lens v=%d f=%d c=%d b=%d niv=%d njv=%d nkv=%d gridtype=%s sorted=%s",
                     d, g.dimensions, g.nvertices, g.nfaces, g.ncells, g.nboundaries,
                     g.ninteriorfaces, g.vertices.length, g.faces.length, g.cells.length,
                     g.boundaries.length, g.niv, g.njv, g.nkv, to!string(g.grid_type),
                     g.cells_are_sorted_into_bins);
            writefln("us.ctor.d%d faceIndices=%d bb0=%s bb1=%s bins=%d nbx=%d nby=%d nbz=%d deltax=%s",
                     d, g.faceIndices.length, to!string(g.bb0), to!string(g.bb1),
                     g.bins.length, g.nbx, g.nby, g.nbz, to!string(g.deltax));
            twd(format("us.ctor.d%d.tostring", d), 880, () {
                writefln("us.ctor.d%d.repr=[%s]", d, to!string(g));
            });
        }
        // The label default argument (D:434) is "".
        auto g0 = new UnstructuredGrid(2);
        writefln("us.ctor.nolabel label=[%s] len=%d", g0.label, g0.label.length);
        // A blank grid is not useless: the abstract table still answers.
        twd("us.ctor.blank.nvfc", 996, () { writefln("us.ctor.blank.nvfc=%d", g0.number_of_vertices_for_cell(0)); });
        twd("us.ctor.blank.vtktype", 1002, () { writefln("us.ctor.blank.vtk=%d", g0.vtk_element_type_for_cell(0)); });
        twd("us.ctor.blank.gettype", 1008, () { writefln("us.ctor.blank.type=%d", g0.get_cell_type(0)); });
        twd("us.ctor.blank.opidx", 860, () { auto v = g0[0, 0]; });
        twd("us.ctor.blank.opidx1", 870, () { auto v = g0[0]; });
        twd("us.ctor.blank.vtxlist", 1013, () { auto v = g0.get_vtx_id_list_for_cell(0, 0, 0); });
        twd("us.ctor.blank.bndgrid", 1069, () { auto b = g0.get_boundary_grid(0); });
        twd("us.ctor.blank.blc", 1118, () { auto b = g0.get_list_of_boundary_cells(0); });
        twd("us.ctor.blank.vtxb", 2450, () { writefln("us.ctor.blank.vtxb=%s", g0.getVertexIdsOnBoundaries()); });
        twd("us.ctor.blank.split", 1181, () { g0.splitTriangleCell(0); });
        twd("us.ctor.blank.join", 2174, () { g0.dup().joinGrid(g0); });
        g0.writeStats();
        twd("us.ctor.blank.su2", 1876, () { g0.write_to_su2_file(TD ~ "/blank.su2", 1.0); });
        writefln("us.ctor.blank.su2 len=%d body=[%s]", len_of(TD ~ "/blank.su2"), body_of(TD ~ "/blank.su2"));
        twd("us.ctor.blank.vtk", 1850, () { g0.write_to_vtk_file(TD ~ "/blank.vtk"); });
        writefln("us.ctor.blank.vtk len=%d body=[%s]", len_of(TD ~ "/blank.vtk"), body_of(TD ~ "/blank.vtk"));
        twd("us.ctor.blank.gz", 1800, () { g0.write_to_gzip_file(TD ~ "/blank.gz"); });
        writefln("us.ctor.blank.gz body=[%s]", gunzip_body_of(TD ~ "/blank.gz"));
        twd("us.ctor.blank.raw", 1823, () { g0.write_to_raw_binary_file(TD ~ "/blank.bin"); });
        writefln("us.ctor.blank.raw len=%d hex=%s", len_of(TD ~ "/blank.bin"), hex_of(TD ~ "/blank.bin"));
        twd("us.ctor.blank.foam", 1930, () { g0.write_openFoam_polyMesh(TD ~ "/blankfoam"); });
        twd("us.ctor.blank.bins", 277, () { g0.sort_cells_into_bins(5, 5); });
    }

    //================ us.str.* — toString (D:875-886) on a real grid
    {
        auto g = hand_quad2d("hs");
        writefln("us.str.hand=[%s]", to!string(g));
        writefln("us.str.hand fmts=[%s]", format("%s", g));
        auto b = hand_quad2d("hb");
        b.cells[0].cell_type = USGCell_type.none;
        twd("us.str.cellnone", 882, () { writefln("us.str.cellnone=[%s]", to!string(b)); });
    }

    //================ us.dup.* — dup() (D:848) and the copy constructor (D:440-456)
    {
        auto g = hand_quad2d("orig");
        g.sort_faces_in_boundary_order();
        auto d = g.dup();
        writefln("us.dup.counts dims=%d nvertices=%d nfaces=%d ncells=%d nboundaries=%d ninteriorfaces=%d label=[%s] niv=%d njv=%d nkv=%d sorted=%s",
                 d.dimensions, d.nvertices, d.nfaces, d.ncells, d.nboundaries,
                 d.ninteriorfaces, d.label, d.niv, d.njv, d.nkv, d.cells_are_sorted_into_bins);
        writefln("us.dup.types origgrid=%s dupgrid=%s", to!string(g.grid_type), to!string(d.grid_type));
        // New objects all the way down: D class handles, so identity is observable only
        // through mutation. Mutate the original and re-read the copy.
        g.vertices[0].x = 11.0;
        g.faces[0].vtx_id_list[0] = 90;
        g.faces[1].is_on_boundary = true;
        g.cells[0].vtx_id_list[0] = 91;
        g.cells[0].face_id_list[0] = 92;
        g.cells[0].outsign_list[0] = -7;
        g.boundaries[0].face_id_list[0] = 93;
        g.boundaries[0].outsign_list[0] = -8;
        g.boundaries[0].tag = "renamed";
        g.nvertices = 777;
        writefln("us.dup.after mutate orig vtx=%s face0=%s cell0=%s bnd0=%s %s nvertices=%d",
                 g.vertices[0].x, g.faces[0].vtx_id_list, g.cells[0].vtx_id_list,
                 g.boundaries[0].face_id_list, g.boundaries[0].outsign_list, g.nvertices);
        writefln("us.dup.after mutate dup  vtx=%s face0=%s cell0=%s bnd0=%s %s nvertices=%d",
                 d.vertices[0].x, d.faces[0].vtx_id_list, d.cells[0].vtx_id_list,
                 d.boundaries[0].face_id_list, d.boundaries[0].outsign_list, d.nvertices);
        writefln("us.dup.after mutate dup  tag0=[%s] onbnd=%s L=%s R=%s ninteriorfaces=%d",
                 d.boundaries[0].tag, d.faces[0].is_on_boundary,
                 d.faces[0].left_cell ? text("nonnull") : "null",
                 d.faces[0].right_cell ? text("nonnull") : "null", d.ninteriorfaces);
        // The face cell links are dropped by USGFace's copy constructor (D:172-178), so a
        // duplicated grid has cells but no face->cell back links.
        writefln("us.dup.backlinks orig=%s/%s dup=%s/%s",
                 g.faces[0].left_cell ? "L" : "-", g.faces[0].right_cell ? "R" : "-",
                 d.faces[0].left_cell ? "L" : "-", d.faces[0].right_cell ? "R" : "-");
        // Cell ids: USGCell(other,id) loses the id (D:236), so every copied cell has id 0.
        writefln("us.dup.cellids orig=%s dup=%s",
                 map!(c => c.id)(g.cells), map!(c => c.id)(d.cells));
        // faceIndices is rebuilt (D:450) from the new faces.
        auto ks = g.faceIndices.byKey.array;
        ks.sort;
        foreach (k; ks) {
            writefln("us.dup.faceidx [%s] orig=%d dup=%d", k, g.faceIndices[k], d.faceIndices[k]);
        }
        writefln("us.dup.faceidx.aaord orig=%s dup=%s", g.faceIndices.byKey.array,
                 d.faceIndices.byKey.array);
        // The label logic of D:442-443.
        auto d2 = new UnstructuredGrid(hand_quad2d("lab"), "explicit");
        writefln("us.dup.label default=[%s] explicit=[%s]", new UnstructuredGrid(hand_quad2d("lab")).label, d2.label);
        // Dup of an empty grid, and dup of a dup.
        auto e = new UnstructuredGrid(3, "e");
        writefln("us.dup.empty dims=%d lens=%d %d %d %d label=[%s]", e.dup().dimensions,
                 e.dup().vertices.length, e.dup().faces.length, e.dup().cells.length,
                 e.dup().boundaries.length, e.dup().label);
        writefln("us.dup.ddl same=%s", e.dup().label == e.label);
        // grid.d has no virtual dup, so the handle type differs between the two spellings.
        writefln("us.dup.typeinfo label=[%s]", to!string(typeid(d).name));
    }

    //================ us.sg2d.* — the StructuredGrid constructor, 2D branch (D:553-661)
    // DEPENDS ON sgrid.d: usgrid.d:553 takes a StructuredGrid and the module unittest at
    // D:2492 builds one, so these rows are D-side reference for that constructor.
    {
        auto sg = surf_grid2d("sg2d");
        writefln("us.sg2d.src dims=%d niv=%d njv=%d nkv=%d ncells=%d nvertices=%d bcTags=%s",
                 sg.dimensions, sg.niv, sg.njv, sg.nkv, sg.ncells, sg.nvertices, sg.bcTags);
        auto usg = new UnstructuredGrid(sg);
        dump_grid("us.sg2d.default", usg);
        dump_faceidx_sorted("us.sg2d.fidx", usg);
        writefln("us.sg2d.fidx.aaord=%s", usg.faceIndices.byKey.array);
        writefln("us.sg2d.repr=[%s]", to!string(usg));
        foreach (i, c; usg.cells) {
            size_t[8] cp = prefilled8();
            size_t nvtx;
            usg.copy_vtx_id_list_for_cell(cp, nvtx, i);
            writefln("us.sg2d.cellcopy[%d] nvtx=%d copy=%s faces=%s outsigns=%s id=%d type=%d",
                     i, nvtx, cp, c.face_id_list, c.outsign_list, c.id, cast(int)c.cell_type);
        }
        foreach (i; 0 .. usg.nvertices) {
            writefln("us.sg2d.opidx[%d] %.17g %.17g %.17g", i, (*usg[i]).x, (*usg[i]).y, (*usg[i]).z);
        }
        // Tag propagation (D:555, D:568-574) with and without a name and a block label.
        sg.set_tag(Face.north, "my-special-boundary");
        sg.set_tag(Face.west, "inlet");
        auto usg2 = new UnstructuredGrid(sg, "renamed-block");
        writefln("us.sg2d.renamed label=[%s] dims=%d nboundaries=%d", usg2.label,
                 usg2.dimensions, usg2.nboundaries);
        dump_bnd("us.sg2d.renamed", usg2.boundaries);
        writefln("us.sg2d.renamed.repr=[%s]", to!string(usg2));
        auto usg3 = new UnstructuredGrid(sg);
        dump_bnd("us.sg2d.inheritedtags", usg3.boundaries);
        writefln("us.sg2d.inheritedtags label=[%s]", usg3.label);
        // The single-cell 2x2 lattice.
        auto sg_small = new StructuredGrid(2, 2, 1, "s2");
        auto usg_small = new UnstructuredGrid(sg_small, "small");
        dump_grid("us.sg2d.2x2", usg_small);
        writefln("us.sg2d.2x2.fidx=%s", sorted_faceidx(usg_small));
        // A 1-wide lattice: i==0 and i==niv-1 are the same face, so it lands in the west
        // AND the east boundary set. nboundaryfaces then exceeds nfaces and
        // ninteriorfaces = nfaces - nboundaryfaces underflows (D:921), the loop at D:926
        // runs essentially forever, and faces[j] is indexed with the underflowed size_t.
        auto sg_thin = new StructuredGrid(1, 4, 1, "thin");
        twd("us.sg2d.thin", 553, () { auto q = new UnstructuredGrid(sg_thin, "thin"); });
        // Same thing from the other side: a 1-tall lattice is a 1D grid, see us.sgbad.
    }

    //================ us.sg3d.* — the StructuredGrid constructor, 3D branch (D:662-820)
    // DEPENDS ON sgrid.d.
    {
        Vector3[8] p;
        p[0] = Vector3(0.0, 0.1, 0.0);
        p[1] = Vector3(1.0, 0.1, 0.0);
        p[2] = Vector3(1.0, 1.1, 0.0);
        p[3] = Vector3(0.0, 1.1, 0.0);
        p[4] = Vector3(0.0, 0.1, 3.0);
        p[5] = Vector3(1.0, 0.1, 3.0);
        p[6] = Vector3(1.0, 1.1, 3.0);
        p[7] = Vector3(1.0, 1.1, 3.0);
        auto vol = new TFIVolume(p);
        UnivariateFunction[] cf = [new LinearFunction(), new LinearFunction(),
                                   new LinearFunction(), new LinearFunction(),
                                   new LinearFunction(), new LinearFunction(),
                                   new LinearFunction(), new LinearFunction(),
                                   new LinearFunction(), new LinearFunction(),
                                   new LinearFunction(), new LinearFunction()];
        auto sg3 = new StructuredGrid(vol, 3, 3, 2, cf, "simple", "box3d");
        writefln("us.sg3d.src dims=%d niv=%d njv=%d nkv=%d ncells=%d nvertices=%d bcTags=%s",
                 sg3.dimensions, sg3.niv, sg3.njv, sg3.nkv, sg3.ncells, sg3.nvertices, sg3.bcTags);
        auto usg = new UnstructuredGrid(sg3);
        dump_grid("us.sg3d.default", usg);
        dump_faceidx_sorted("us.sg3d.fidx", usg);
        writefln("us.sg3d.repr=[%s]", to!string(usg));
        foreach (i, c; usg.cells) {
            size_t[8] cp = prefilled8();
            size_t nvtx;
            usg.copy_vtx_id_list_for_cell(cp, nvtx, i);
            writefln("us.sg3d.cellcopy[%d] nvtx=%d copy=%s faces=%s outsigns=%s id=%d vtk=%d",
                     i, nvtx, cp, c.face_id_list, c.outsign_list, c.id,
                     usg.vtk_element_type_for_cell(i));
        }
        // A 2x2x2 lattice is the smallest legal 3D case.
        auto sg8 = new StructuredGrid(vol, 2, 2, 2, cf, "simple", "cube");
        auto u8 = new UnstructuredGrid(sg8, "cubeu");
        dump_grid("us.sg3d.cube", u8);
        writefln("us.sg3d.cube.ninterior=%d faceidxcount=%d", u8.ninteriorfaces, u8.faceIndices.length);
        // One cell thick in one direction: 2x2x2 with nkv=2 is that already; a 1-tall
        // lattice makes (sg.njv-1) == 0 and the j/k face arrays empty.
        auto sg_flat = new StructuredGrid(vol, 3, 1, 2, cf, "simple", "flat");
        twd("us.sg3d.flat", 662, () {
            auto q = new UnstructuredGrid(sg_flat, "flatu");
            writefln("us.sg3d.flat.nv=%d nf=%d nc=%d nb=%d ninterior=%d", q.nvertices,
                     q.nfaces, q.ncells, q.nboundaries, q.ninteriorfaces);
        });
    }

    //================ us.sgbad.* — StructuredGrid shapes the constructor is not for
    // DEPENDS ON sgrid.d.
    {
        // A 1D lattice has dimensions == 1, so D:561 is false and the code takes the
        // "Assume dimensions == 3" branch (D:662) and reads bcTags[2] of a 2-element
        // array (sgrid.d:45 sets bcTags = ["", ""] for dim 1).
        auto sg1 = new StructuredGrid(5, 1, 1, "line1d");
        writefln("us.sgbad.1d.src dims=%d niv=%d njv=%d nkv=%d bcTags=%d", sg1.dimensions,
                 sg1.niv, sg1.njv, sg1.nkv, sg1.bcTags.length);
        twd("us.sgbad.1d", 568, () { auto q = new UnstructuredGrid(sg1, "u1d"); });
        // A 1x1x1 lattice is 1D by the same inference.
        auto pt = new StructuredGrid(1, 1, 1, "point");
        twd("us.sgbad.point", 568, () { auto q = new UnstructuredGrid(pt, "upt"); });
        // A lattice with zero vertices in one direction: niv == 0 makes sg.ncells wrap in
        // sgrid.d:44, and the usgrid counts come out as size_t products.
        auto sgz = new StructuredGrid(0, 1, 1, "zero");
        writefln("us.sgbad.zero dims=%d niv=%d njv=%d nkv=%d ncells=%d nvertices=%d bcTags=%d",
                 sgz.dimensions, sgz.niv, sgz.njv, sgz.nkv, sgz.ncells, sgz.nvertices,
                 sgz.bcTags.length);
        twd("us.sgbad.zero", 568, () { auto q = new UnstructuredGrid(sgz, "uzero"); });
        auto sgz2 = new StructuredGrid(3, 0, 1, "zeroj");
        writefln("us.sgbad.zeroj dims=%d niv=%d njv=%d nkv=%d ncells=%d", sgz2.dimensions,
                 sgz2.niv, sgz2.njv, sgz2.nkv, sgz2.ncells);
        twd("us.sgbad.zeroj", 561, () { auto q = new UnstructuredGrid(sgz2, "uzeroj"); });
        // An empty label on the structured grid: D:573 skips the "-" separator, so the
        // boundary tag is the bare face name.
        auto sg_lbl = new StructuredGrid(3, 3, 1, "");
        auto u_lbl = new UnstructuredGrid(sg_lbl);
        dump_bnd("us.sgbad.nolabel", u_lbl.boundaries);
        writefln("us.sgbad.nolabel label=[%s]", u_lbl.label);
        auto sg_lbl2 = new StructuredGrid(3, 3, 1, "blk");
        dump_bnd("us.sgbad.withlabel", (new UnstructuredGrid(sg_lbl2)).boundaries);
        // All four/six tags set by name.
        auto sg_t = new StructuredGrid(3, 3, 1, "tagged");
        sg_t.set_tags(["west": "W", "east": "E", "south": "S", "north": "N"]);
        dump_bnd("us.sgbad.alltags", (new UnstructuredGrid(sg_t)).boundaries);
    }

    //================ us.pave.* — the Vector3[][] paving constructor (D:458-512)
    {
        auto g = new UnstructuredGrid(pave_square(), "paved");
        dump_grid("us.pave.sq", g);
        writefln("us.pave.sq repr=[%s]", to!string(g));
        writefln("us.pave.sq fidx=%s", sorted_faceidx(g));
        // The mid-point vertex and the triangle cells come from paver2d.fill_interior,
        // whose two writeln lines appear above these rows on stdout.
        foreach (i; 0 .. g.ncells) {
            Vector3 c; number vol;
            g.compute_cell_properties(i, false, c, vol);
            writefln("us.pave.sq ccp[%d] centroid=[%.17g %.17g %.17g] vol=%.17g bary=%s", i,
                     c.x, c.y, c.z, vol, to!string(g.cell_barycentre(i)));
        }
        // Two boundary segments, joined end to end.
        auto g2 = new UnstructuredGrid(pave_two_segments(), "twoseg");
        dump_grid("us.pave.seg", g2);
        // D:466 first point of segment 0 against last point of the last segment.
        Vector3[] bad = [Vector3(0.0, 0.0), Vector3(1.0, 0.0), Vector3(1.0, 1.0), Vector3(0.0, 1.0)];
        twd("us.pave.mismatch.firstlast", 469, () {
            auto q = new UnstructuredGrid([bad], "bad1");
        });
        // The tolerance is a real parameter: a gap of 1e-9 is not > 1e-9, a gap of 2e-9 is.
        Vector3[] near = [Vector3(0.0, 0.0), Vector3(1.0, 0.0), Vector3(1.0, 1.0), Vector3(1e-12, 1.0)];
        twd("us.pave.gap1e-12", 469, () { auto q = new UnstructuredGrid([near], "near"); });
        Vector3[] far = [Vector3(0.0, 0.0), Vector3(1.0, 0.0), Vector3(1.0, 1.0), Vector3(2e-9, 1.0)];
        twd("us.pave.gap2e-9", 469, () { auto q = new UnstructuredGrid([far], "far"); });
        // D:491 segment-to-segment mismatch (the first check still passes).
        Vector3[] a = [Vector3(0.0, 0.0), Vector3(1.0, 0.0), Vector3(1.0, 1.0)];
        Vector3[] b = [Vector3(2.0, 1.0), Vector3(0.0, 1.0), Vector3(0.0, 0.0)];
        twd("us.pave.mismatch.segment", 496, () { auto q = new UnstructuredGrid([a, b], "bad2"); });
        // Empty boundary lists.
        twd("us.pave.empty", 466, () { auto q = new UnstructuredGrid(new Vector3[][0], "bad3"); });
        twd("us.pave.emptyinner", 466, () {
            Vector3[][] v = [[Vector3(0.0, 0.0)], cast(Vector3[])[]];
            auto q = new UnstructuredGrid(v, "bad4");
        });
        twd("us.pave.emptyfirst", 466, () {
            Vector3[][] v = [cast(Vector3[])[], [Vector3(0.0, 0.0)]];
            auto q = new UnstructuredGrid(v, "bad5");
        });
        // A one-point segment: no face is built for it, so its boundary set is empty and
        // BoundaryFaceSet.toString (D:393) fails inside the grid's own toString (D:883).
        twd("us.pave.onepointseg", 496, () {
            Vector3[] c = [Vector3(1.0, 1.0)];
            auto q = new UnstructuredGrid([a, c], "bad6");
            writefln("us.pave.onepointseg.bnd=%s", q.boundaries[1].toIOString());
            writefln("us.pave.onepointseg.repr=%s", to!string(q));
        });
        // A two-point segment is the smallest thing that makes a face.
        Vector3[] d1 = [Vector3(0.0, 0.0), Vector3(1.0, 1.0)];
        Vector3[] d2 = [Vector3(1.0, 1.0), Vector3(0.0, 0.0)];
        auto g3 = new UnstructuredGrid([d1, d2], "degenerate");
        dump_grid("us.pave.degenerate", g3);
    }

    //================ us.paved.* — the PavedGrid constructor (D:514-551)
    // DEPENDS ON paver.d (stage G2). Uses the module-level POINT_LIST/FACE_LIST/CELL_LIST.
    {
        Vector3[] bd = [Vector3(0.0, 0.0), Vector3(0.5, 0.0), Vector3(1.0, 0.0),
                        Vector3(1.0, 0.5), Vector3(1.0, 1.0), Vector3(0.5, 1.0),
                        Vector3(0.0, 1.0), Vector3(0.0, 0.5)];
        BoundaryFaceSet[] ib = [new BoundaryFaceSet("muir-wall", cast(size_t[])[], cast(int[])[])];
        twd("us.paved.ctor", 514, () {
            auto g = new UnstructuredGrid(bd, ib, "muir");
            dump_grid("us.paved", g);
            writefln("us.paved repr=[%s]", to!string(g));
            writefln("us.paved nboundaries=%d boundaries=%d cells=%d faces=%d", g.nboundaries,
                     g.boundaries.length, g.cells.length, g.faces.length);
        });
        // The globals paver leaves behind are visible here: CELL_LIST is what D:540 walks.
        writefln("us.paved.lists POINT_LIST=%d FACE_LIST=%d CELL_LIST=%d", POINT_LIST.length,
                 FACE_LIST.length, CELL_LIST.length);
        twd("us.paved.badboundary", 522, () {
            Vector3[] z = [Vector3(0.0, 0.0)];
            auto g = new UnstructuredGrid(z, cast(BoundaryFaceSet[])[], "z");
            writefln("us.paved.badboundary cells=%d", g.cells.length);
        });
        // SHARED MUTABLE GLOBAL STATE across two successive constructions. paver.d's
        // POINT_LIST/FACE_LIST/CELL_LIST (paver.d:158-160) are module-level mutable
        // arrays and the PavedGrid constructor does NOT reset them: it appends. So a
        // SECOND PavedGrid/Heather-Muir construction sees the FIRST grid's lists still
        // present and the globals accumulate. Re-printing after a second construction of
        // the same boundary is the discriminating row: doubled => accumulate, same =>
        // reset. (In the port these become inline eil::Array<> globals — paver.hpp:195.)
        twd("us.paved.ctor2", 514, () {
            auto g2 = new UnstructuredGrid(bd, ib, "muir2");
            writefln("us.paved.ctor2 nv=%d nf=%d nc=%d", g2.nvertices, g2.faces.length,
                     g2.cells.length);
        });
        writefln("us.paved.lists.after2 POINT_LIST=%d FACE_LIST=%d CELL_LIST=%d",
                 POINT_LIST.length, FACE_LIST.length, CELL_LIST.length);
    }

    //================ us.gz.* — the native gzipped text format (D:1800, D:1275)
    {
        // Round trip a 2D paving grid and a 3D structured-derived grid.
        auto g2 = new UnstructuredGrid(pave_square(), "gz2d");
        string p = TD ~ "/gz2d.gz";
        twd("us.gz.write2d", 1800, () { g2.write_to_gzip_file(p); });
        writefln("us.gz.write2d len=%d payload=[%s]", len_of(p), gunzip_body_of(p));
        twd("us.gz.read2d", 1275, () {
            auto r = new UnstructuredGrid(p, "gziptext", true);
            dump_grid("us.gz.read2d", r);
            writefln("us.gz.read2d repr=[%s]", to!string(r));
            string p2 = TD ~ "/gz2d_again.gz";
            r.write_to_gzip_file(p2);
            writefln("us.gz.read2d reserialize same=%s", gunzip_body_of(p) == gunzip_body_of(p2));
        });
        // With a scale factor the vertices are multiplied on the way in (D:1302).
        twd("us.gz.read2scalet", 1275, () {
            auto r = new UnstructuredGrid(p, "gziptext", true, 25.4);
            writefln("us.gz.scaled vtx[1] bits=%s %s %s", bits(r.vertices[1].x),
                     bits(r.vertices[3].x), bits(r.vertices[4].y));
            writefln("us.gz.scaled vtx[1]=%.17g %.17g", r.vertices[1].x, r.vertices[4].y);
        });
        auto g3 = new UnstructuredGrid(vol_grid3d("gz3d"), "gz3d");
        string p3 = TD ~ "/gz3d.gz";
        twd("us.gz.write3d", 1800, () { g3.write_to_gzip_file(p3); });
        writefln("us.gz.write3d len=%d payload=[%s]", len_of(p3), gunzip_body_of(p3));
        twd("us.gz.read3d", 1275, () {
            auto r = new UnstructuredGrid(p3, "gziptext", true);
            writefln("us.gz.read3d dims=%d nvertices=%d nfaces=%d ncells=%d nboundaries=%d",
                     r.dimensions, r.nvertices, r.nfaces, r.ncells, r.nboundaries);
            r.link_faces_back_to_cells();
            dump_faces("us.gz.read3d", r.faces);
        });
        // A label with spaces survives "label: %s" (D:1291).
        auto gl = new UnstructuredGrid(2, "my label with spaces");
        string pl = TD ~ "/label.gz";
        gl.write_to_gzip_file(pl);
        writefln("us.gz.label payload=[%s]", gunzip_body_of(pl));
        twd("us.gz.label-read", 1291, () {
            auto r = new UnstructuredGrid(pl, "gziptext", true);
            writefln("us.gz.label read label=[%s] dims=%d", r.label, r.dimensions);
        });
        // Wrong format version (D:1286) and a garbage header.
        string pv = TD ~ "/ver.gz";
        {
            auto fo = new GzipOut(pv);
            fo.compress("unstructured_grid 2.0\n");
            fo.compress("label: x\n");
            fo.compress("dimensions: 2\n");
            fo.finish();
        }
        twd("us.gz.version", 1287, () { new UnstructuredGrid(pv, "gziptext", true); });
        string pv2 = TD ~ "/ver1.gz";
        {
            auto fo = new GzipOut(pv2);
            fo.compress("structured_grid 1.0\n");
            fo.compress("label: x\n");
            fo.compress("dimensions: 2\n");
            fo.finish();
        }
        twd("us.gz.wronghead", 1285, () { new UnstructuredGrid(pv2, "gziptext", true); });
        // An empty file, a non-gzip file, and a missing file.
        string pe = TD ~ "/empty.gz";
        mkr(pe, "");
        twd("us.gz.emptyfile", 1283, () { new UnstructuredGrid(pe, "gziptext", true); });
        string pnp = TD ~ "/notgz.gz";
        mkr(pnp, "hello world\n");
        twd("us.gz.notgzip", 1283, () { new UnstructuredGrid(pnp, "gziptext", true); });
        twd("us.gz.missing", 1282, () { new UnstructuredGrid(TD ~ "/no_such_file.gz", "gziptext", true); });
        // Truncated at each header line in turn.
        foreach (n; 1 .. 8) {
            string pt = format("%s/trunc%d.gz", TD, n);
            string[] lines = split(gunzip_body_of(p), "<newline>");
            string body;
            foreach (i, L; lines) { if (i < n) { if (i > 0) body ~= "\n"; body ~= L; } }
            {
                auto fo = new GzipOut(pt);
                fo.compress(body ~ "\n");
                fo.finish();
            }
            twd(format("us.gz.truncated%d", n), 1283, () { new UnstructuredGrid(pt, "gziptext", true); });
        }
        // A cell line whose declared counts do not agree with the tokens.
        string pcb = TD ~ "/badcell.gz";
        {
            auto fo = new GzipOut(pcb);
            fo.compress("unstructured_grid 1.0\nlabel: bc\ndimensions: 2\n");
            fo.compress("vertices: 4\n 0 0 0\n 1 0 0\n 1 1 0\n 0 1 0\n");
            fo.compress("faces: 4\n2 0 1\n2 1 2\n2 2 3\n2 3 0\n");
            fo.compress("cells: 1\nquad vtx 4 0 1 2 3 faces 5 0 1 2 3 outsigns 4 1 1 -1 -1\n");
            fo.compress("boundaries: 1\nwall faces 4 0 1 2 3 outsigns 4 1 1 -1 -1\n");
            fo.finish();
        }
        twd("us.gz.badcell.count", 1318, () { new UnstructuredGrid(pcb, "gziptext", true); });
        string pcb2 = TD ~ "/badbnd.gz";
        {
            auto fo = new GzipOut(pcb2);
            fo.compress("unstructured_grid 1.0\nlabel: bb\ndimensions: 2\n");
            fo.compress("vertices: 4\n 0 0 0\n 1 0 0\n 1 1 0\n 0 1 0\n");
            fo.compress("faces: 4\n2 0 1\n2 1 2\n2 2 3\n2 3 0\n");
            fo.compress("cells: 1\nquad vtx 4 0 1 2 3 faces 4 0 1 2 3 outsigns 4 1 1 -1 -1\n");
            fo.compress("boundaries: 1\nwall faces 2 0 1 outsigns 3 1 -1 1\n");
            fo.finish();
        }
        twd("us.gz.badbnd.assert", 1325, () { new UnstructuredGrid(pcb2, "gziptext", true); });
        // link_faces_back_to_cells outsign mismatch through the reader (D:1263).
        string pcb3 = TD ~ "/badoutsign.gz";
        {
            auto fo = new GzipOut(pcb3);
            fo.compress("unstructured_grid 1.0\nlabel: bo\ndimensions: 2\n");
            fo.compress("vertices: 4\n 0 0 0\n 1 0 0\n 1 1 0\n 0 1 0\n");
            fo.compress("faces: 4\n2 0 1\n2 1 2\n2 2 3\n2 3 0\n");
            fo.compress("cells: 1\nquad vtx 4 0 1 2 3 faces 4 0 1 2 3 outsigns 3 1 1 -1\n");
            fo.compress("boundaries: 1\nwall faces 4 0 1 2 3 outsigns 4 1 1 -1 -1\n");
            fo.finish();
        }
        twd("us.gz.badoutsign", 1327, () { new UnstructuredGrid(pcb3, "gziptext", true); });
    }

    //================ us.raw.* — the native raw binary format (D:1823, D:1330)
    {
        auto g2 = new UnstructuredGrid(pave_square(), "raw2d");
        string p = TD ~ "/raw2d.bin";
        twd("us.raw.write", 1823, () { g2.write_to_raw_binary_file(p); });
        writefln("us.raw.write len=%d hex=%s", len_of(p), hex_of(p));
        twd("us.raw.read", 1330, () {
            auto r = new UnstructuredGrid(p, "rawbinary", true);
            dump_grid("us.raw.read", r);
            string p2 = TD ~ "/raw2d_again.bin";
            r.write_to_raw_binary_file(p2);
            writefln("us.raw.read reserialize same=%s len=%d",
                     hex_of(p) == hex_of(p2), len_of(p2));
        });
        twd("us.raw.read-scale", 1330, () {
            auto r = new UnstructuredGrid(p, "rawbinary", true, 0.3048);
            writefln("us.raw.scaled vtx[2] bits=%s vtx[4] bits=%s", bits(r.vertices[2].x),
                     bits(r.vertices[4].y));
        });
        // A grid with an empty label takes the label_length == 0 branch (D:1347).
        auto g0 = new UnstructuredGrid(2, "");
        string p0 = TD ~ "/raw0.bin";
        g0.write_to_raw_binary_file(p0);
        writefln("us.raw.nolabel len=%d hex=%s", len_of(p0), hex_of(p0));
        twd("us.raw.nolabel-read", 1330, () {
            auto r = new UnstructuredGrid(p0, "rawbinary", true);
            writefln("us.raw.nolabel label=[%s] dims=%d", r.label, r.dimensions);
        });
        // Wrong header, truncated header, empty file, missing file.
        string pw = TD ~ "/badhead.bin";
        {
            auto fo = File(pw, "wb");
            fo.rawWrite(to!(char[])("structured_grid 1.0"));
            fo.close();
        }
        twd("us.raw.badheader", 1340, () { new UnstructuredGrid(pw, "rawbinary", true); });
        string pt2 = TD ~ "/shorthead.bin";
        {
            auto fo = File(pt2, "wb");
            fo.rawWrite(to!(char[])("unstructured_g"));
            fo.close();
        }
        twd("us.raw.sheader", 1340, () { new UnstructuredGrid(pt2, "rawbinary", true); });
        string pe = TD ~ "/empty.bin";
        mkr(pe, "");
        twd("us.raw.emptyfile", 1337, () { new UnstructuredGrid(pe, "rawbinary", true); });
        twd("us.raw.missing", 1337, () { new UnstructuredGrid(TD ~ "/no_such_file.bin", "rawbinary", true); });
        // A header followed by nothing: the label length read (D:1345) hits end of file.
        string ph = TD ~ "/headonly.bin";
        {
            auto fo = File(ph, "wb");
            fo.rawWrite(to!(char[])("unstructured_grid 1.0"));
            fo.close();
        }
        twd("us.raw.headonly", 1345, () { new UnstructuredGrid(ph, "rawbinary", true); });
        // Declared counts that exceed what is present.
        string pc = TD ~ "/badc.bin";
        {
            auto fo = File(pc, "wb");
            fo.rawWrite(to!(char[])("unstructured_grid 1.0"));
            int[1] zl = [0]; fo.rawWrite(zl);
            int[5] c = [2, 1, 1, 4, 1];   // 4 cells declared, none written
            fo.rawWrite(c);
            double[3] v = [0.0, 0.0, 0.0]; fo.rawWrite(v);
            int[1] one = [1]; fo.rawWrite(one); int[1] f0 = [0]; fo.rawWrite(f0);
            fo.close();
        }
        twd("us.raw.toofewcells", 1373, () { new UnstructuredGrid(pc, "rawbinary", true); });
    }

    //================ us.su2.* — write_to_su2_file (D:1876)
    {
        // 2D triangles from the paving constructor.
        auto gt = new UnstructuredGrid(pave_square(), "su2tri");
        string p = TD ~ "/tri.su2";
        twd("us.su2.tri", 1876, () { gt.write_to_su2_file(p); });
        writefln("us.su2.tri len=%d body=[%s]", len_of(p), body_of(p));
        // 2D quads from a structured grid.
        auto gq = new UnstructuredGrid(surf_grid2d("su2quad"), "su2quad");
        string p2 = TD ~ "/quad.su2";
        twd("us.su2.quad", 1876, () { gq.write_to_su2_file(p2); });
        writefln("us.su2.quad len=%d body=[%s]", len_of(p2), body_of(p2));
        // 3D hexes; note the NPOIN block carries three coordinates (D:1900-1904).
        auto gh = new UnstructuredGrid(vol_grid3d("su2hex"), "su2hex");
        string p3 = TD ~ "/hex.su2";
        twd("us.su2.hex", 1876, () { gh.write_to_su2_file(p3); });
        writefln("us.su2.hex len=%d body=[%s]", len_of(p3), body_of(p3));
        // Scaled output: coordinates are divided by scale (D:1901).
        string p4 = TD ~ "/hex_scaled.su2";
        twd("us.su2.hex-scaled", 1876, () { gh.write_to_su2_file(p4, 25.4); });
        writefln("us.su2.hex scaled len=%d body=[%s]", len_of(p4), body_of(p4));
        string p5 = TD ~ "/quad_scaled.su2";
        gq.write_to_su2_file(p5, 0.0254);
        writefln("us.su2.quad scaled len=%d body=[%s]", len_of(p5), body_of(p5));
        // A wedge cell, written in both vertex orders.
        mkr(TD ~ "/wedge.su2", wedge_su2());
        auto gw = new UnstructuredGrid(TD ~ "/wedge.su2", "su2text", true);
        string p6 = TD ~ "/wedge_vtk.su2";
        gw.write_to_su2_file(p6, 1.0, false);
        writefln("us.su2.wedge vtkorder len=%d body=[%s]", len_of(p6), body_of(p6));
        string p7 = TD ~ "/wedge_gmsh.su2";
        gw.write_to_su2_file(p7, 1.0, true);
        writefln("us.su2.wedge gmshorder len=%d body=[%s]", len_of(p7), body_of(p7));
        // An empty-tag boundary still prints "MARKER_TAG= " (D:1909).
        auto ge = new UnstructuredGrid(2, "emptytag");
        ge.vertices = [Vector3(0,0), Vector3(1,0), Vector3(1,1), Vector3(0,1)];
        ge.nvertices = 4;
        size_t[][4] vls = [[cast(size_t)0, cast(size_t)1], [cast(size_t)2, cast(size_t)3],
                           [cast(size_t)1, cast(size_t)2], [cast(size_t)0, cast(size_t)3]];
        foreach (vl; vls) {
            ge.faceIndices[makeFaceTag(vl)] = ge.faces.length;
            ge.faces ~= new USGFace(vl);
        }
        ge.nfaces = 4;
        ge.cells ~= new USGCell(USGCell_type.quad, 0, [0,1,2,3], [0,2,1,3], [1,1,-1,-1]);
        ge.ncells = 1;
        ge.boundaries ~= new BoundaryFaceSet("", [cast(size_t)0], [1]);
        ge.nboundaries = 1;
        ge.niv = 4; ge.njv = 1; ge.nkv = 1;
        string p8 = TD ~ "/emptytag.su2";
        ge.write_to_su2_file(p8);
        writefln("us.su2.emptytag body=[%s]", body_of(p8));
        // The two directions of the sgrid delegation, both written through usgrid
        // (sgrid.d:1313-1318 just forwards here, so the bodies must be identical).
        auto sg = surf_grid2d("delegate");
        string pA = TD ~ "/deleg_usg.su2";
        (new UnstructuredGrid(sg, "delegate")).write_to_su2_file(pA);
        string pB = TD ~ "/deleg_sgrid.su2";
        sg.write_to_su2_file(pB);
        writefln("us.su2.delegate direct len=%d sgrid len=%d same=%s", len_of(pA), len_of(pB),
                 body_of(pA) == body_of(pB));
        string pC = TD ~ "/deleg_sgrid_scaled.su2";
        sg.write_to_su2_file(pC, 2.0);
        string pD = TD ~ "/deleg_usg_scaled.su2";
        (new UnstructuredGrid(sg, "delegate")).write_to_su2_file(pD, 2.0);
        writefln("us.su2.delegate scaled same=%s body=[%s]", body_of(pC) == body_of(pD),
                 body_of(pC));
    }

    //================ us.su2rd.* — read_from_su2_file (D:1382)
    {
        mkr(TD ~ "/tri_rd.su2", tri_su2());
        twd("us.su2rd.tri", 1382, () {
            auto g = new UnstructuredGrid(TD ~ "/tri_rd.su2", "su2text", true);
            dump_grid("us.su2rd.tri", g);
            writefln("us.su2rd.tri repr=[%s]", to!string(g));
            writefln("us.su2rd.tri fidx=%s", sorted_faceidx(g));
        });
        mkr(TD ~ "/quad_rd.su2", quad_su2());
        twd("us.su2rd.quad", 1382, () {
            auto g = new UnstructuredGrid(TD ~ "/quad_rd.su2", "su2text", true);
            dump_grid("us.su2rd.quad", g);
            string p = TD ~ "/quad_rt.su2";
            g.write_to_su2_file(p);
            writefln("us.su2rd.quad roundtrip len=%d body=[%s]", len_of(p), body_of(p));
            twd("us.su2rd.quad-reread", 1382, () {
                auto g2 = new UnstructuredGrid(p, "su2text", true);
                writefln("us.su2rd.quad reread nv=%d nf=%d nc=%d nb=%d", g2.nvertices,
                         g2.nfaces, g2.ncells, g2.nboundaries);
            });
        });
        mkr(TD ~ "/hex_rd.su2", hex_su2());
        twd("us.su2rd.hex", 1382, () {
            auto g = new UnstructuredGrid(TD ~ "/hex_rd.su2", "su2text", true);
            dump_grid("us.su2rd.hex", g);
            writefln("us.su2rd.hex fidx=%s", sorted_faceidx(g));
        });
        // The same hex mesh with a scale: coordinates are multiplied by scale (D:1500-1502).
        twd("us.su2rd.hex-scale", 1382, () {
            auto g = new UnstructuredGrid(TD ~ "/hex_rd.su2", "su2text", true, 25.4, false, "scaled");
            writefln("us.su2rd.hex scale label=[%s] vtx[1] bits=%s vtx[9] bits=%s", g.label,
                     bits(g.vertices[1].x), bits(g.vertices[9].y));
        });
        // Elements with no trailing cell index (tokens.length == 1+nv_expected, D:1433).
        mkr(TD ~ "/noidx.su2", su2_noindex());
        twd("us.su2rd.noidx", 1382, () {
            auto g = new UnstructuredGrid(TD ~ "/noidx.su2", "su2text", true);
            writefln("us.su2rd.noidx nv=%d nf=%d nc=%d nb=%d cellids=%s", g.nvertices,
                     g.nfaces, g.ncells, g.nboundaries, map!(c => c.id)(g.cells));
        });
        // Vertex indices out of natural order, so cells[indx] is written out of sequence.
        mkr(TD ~ "/shufidx.su2", su2_shuffled());
        twd("us.su2rd.shuffled", 1382, () {
            auto g = new UnstructuredGrid(TD ~ "/shufidx.su2", "su2text", true);
            writefln("us.su2rd.shuffled cellids=%s cell0io=[%s]", map!(c => c.id)(g.cells),
                     g.cells[0].toIOString());
            dump_cells("us.su2rd.shuffled", g.cells);
        });
        // Wedge read in GMSH order and reordered (D:1447-1452).
        mkr(TD ~ "/wedge_gmsh.su2", su2_wedge_gmsh());
        twd("us.su2rd.wedge-nomsh", 1382, () {
            auto g = new UnstructuredGrid(TD ~ "/wedge_gmsh.su2", "su2text", true, 1.0, false);
            writefln("us.su2rd.wedge nomsh cell0=%s", g.cells[0].toIOString());
        });
        twd("us.su2rd.wedge-gmsh", 1382, () {
            auto g = new UnstructuredGrid(TD ~ "/wedge_gmsh.su2", "su2text", true, 1.0, true);
            writefln("us.su2rd.wedge gmsh cell0=%s", g.cells[0].toIOString());
            dump_faces("us.su2rd.wedge gmsh", g.faces);
            dump_bnd("us.su2rd.wedge gmsh", g.boundaries);
        });
        // A pyramid cell in 3D.
        mkr(TD ~ "/pyr.su2", su2_pyramid());
        twd("us.su2rd.pyramid", 1382, () {
            auto g = new UnstructuredGrid(TD ~ "/pyr.su2", "su2text", true);
            dump_grid("us.su2rd.pyramid", g);
        });
        // 2D cells given clockwise must be reversed (D:1519-1542).
        mkr(TD ~ "/cw2d.su2", su2_cw2d());
        twd("us.su2rd.cw2d", 1382, () {
            auto g = new UnstructuredGrid(TD ~ "/cw2d.su2", "su2text", true);
            dump_grid("us.su2rd.cw2d", g);
        });
        // An orphan face: a boundary marker that is missing one external edge, so the
        // face is left with one cell and no marker (D:1750-1766) and the closure test at
        // D:1775 then reports a non-zero bounding vector area.
        mkr(TD ~ "/orphan.su2", su2_orphan());
        twd("us.su2rd.orphan", 1777, () {
            auto g = new UnstructuredGrid(TD ~ "/orphan.su2", "su2text", true);
            writefln("us.su2rd.orphan nb=%d tags=%s", g.nboundaries,
                     map!(b => b.tag)(g.boundaries));
            dump_bnd("us.su2rd.orphan", g.boundaries);
        });
        // A marker that omits an edge but the mesh is otherwise closed: ORPHAN_FACES set.
        mkr(TD ~ "/orphan2.su2", su2_orphan2());
        twd("us.su2rd.orphan2", 1763, () {
            auto g = new UnstructuredGrid(TD ~ "/orphan2.su2", "su2text", true);
            dump_bnd("us.su2rd.orphan2", g.boundaries);
            writefln("us.su2rd.orphan2 nb=%d nfaces=%d ninterior=%d", g.nboundaries,
                     g.nfaces, g.ninteriorfaces);
        });
        // ---- failure paths
        mkr(TD ~ "/bad_items.su2", su2_baditemcount());
        twd("us.su2rd.baditems", 1439, () { new UnstructuredGrid(TD ~ "/bad_items.su2", "su2text", true); });
        mkr(TD ~ "/bad_type.su2", su2_badtype());
        twd("us.su2rd.badtype", 1445, () { new UnstructuredGrid(TD ~ "/bad_type.su2", "su2text", true); });
        mkr(TD ~ "/poly.su2", su2_polygon());
        twd("us.su2rd.polygon", 1445, () { new UnstructuredGrid(TD ~ "/poly.su2", "su2text", true); });
        // An unknown VTK type whose expected vertex count is 0, so the item-count test at
        // D:1429/:1433 passes and the reach the "Unknown element type." message (D:1443).
        mkr(TD ~ "/poly_bare.su2", "NDIME= 2\n\nNELEM= 1\n7 0\n\nNPOIN= 3\n0.0 0.0 0\n1.0 0.0 1\n0.0 1.0 2\n\nNMARK= 1\nMARKER_TAG= a\nMARKER_ELEMS= 1\n3 0 1\n");
        twd("us.su2rd.bare", 1445, () { new UnstructuredGrid(TD ~ "/poly_bare.su2", "su2text", true); });
        mkr(TD ~ "/unknown_bare.su2", "NDIME= 2\n\nNELEM= 1\n99 0\n\nNPOIN= 3\n0.0 0.0 0\n1.0 0.0 1\n0.0 1.0 2\n\nNMARK= 1\nMARKER_TAG= a\nMARKER_ELEMS= 1\n3 0 1\n");
        twd("us.su2rd.unknownbare", 1445, () { new UnstructuredGrid(TD ~ "/unknown_bare.su2", "su2text", true); });
        // A single 1-token element line: tokens.length == 1 == 1 + nv_expected.
        mkr(TD ~ "/onetok.su2", "NDIME= 2\n\nNELEM= 1\n99\n\nNPOIN= 3\n0.0 0.0 0\n1.0 0.0 1\n0.0 1.0 2\n\nNMARK= 1\nMARKER_TAG= a\nMARKER_ELEMS= 1\n3 0 1\n");
        twd("us.su2rd.onetok", 1445, () { new UnstructuredGrid(TD ~ "/onetok.su2", "su2text", true); });
        mkr(TD ~ "/tri_in3d.su2", su2_triin3d());
        twd("us.su2rd.triin3d", 1690, () { new UnstructuredGrid(TD ~ "/tri_in3d.su2", "su2text", true); });
        mkr(TD ~ "/tet_in2d.su2", su2_tetin2d());
        twd("us.su2rd.tetin2d", 1544, () { new UnstructuredGrid(TD ~ "/tet_in2d.su2", "su2text", true); });
        mkr(TD ~ "/dupcell.su2", su2_duplicate_cell());
        twd("us.su2rd.dupcell", 1615, () { new UnstructuredGrid(TD ~ "/dupcell.su2", "su2text", true); });
        mkr(TD ~ "/markerint.su2", su2_marker_interior());
        twd("us.su2rd.markerinterior", 1734, () { new UnstructuredGrid(TD ~ "/markerint.su2", "su2text", true); });
        mkr(TD ~ "/markerunk.su2", su2_marker_unknown());
        twd("us.su2rd.markerunknown", 1739, () { new UnstructuredGrid(TD ~ "/markerunk.su2", "su2text", true); });
        mkr(TD ~ "/noclose.su2", su2_not_closed());
        twd("us.su2rd.noclose", 1777, () { new UnstructuredGrid(TD ~ "/noclose.su2", "su2text", true); });
        mkr(TD ~ "/selfint.su2", su2_selfintersecting_quad());
        twd("us.su2rd.selfint", 1702, () {
            auto g = new UnstructuredGrid(TD ~ "/selfint.su2", "su2text", true);
            writefln("us.su2rd.selfint cell0=%s varea=%s", g.cells[0].toIOString(),
                     to!string(g.vectorAreaOfCell(g.cells[0])));
        });
        // A hex whose element line lists the vertices in the wrong order: the six faces
        // this makes are not the faces of a closed cell, so D:1695 should not balance.
        mkr(TD ~ "/scrambled.su2", su2_scrambled_hex());
        twd("us.su2rd.scrambled", 1702, () { new UnstructuredGrid(TD ~ "/scrambled.su2", "su2text", true); });
        // Header problems.
        mkr(TD ~ "/no_ndime.su2", su2_missing("NDIME"));
        twd("us.su2rd.no-ndime", 1407, () { new UnstructuredGrid(TD ~ "/no_ndime.su2", "su2text", true); });
        mkr(TD ~ "/no_nelem.su2", su2_missing("NELEM"));
        twd("us.su2rd.no-nelem", 1412, () { new UnstructuredGrid(TD ~ "/no_nelem.su2", "su2text", true); });
        mkr(TD ~ "/no_npoin.su2", su2_missing("NPOIN"));
        twd("us.su2rd.no-npoin", 1478, () { new UnstructuredGrid(TD ~ "/no_npoin.su2", "su2text", true); });
        mkr(TD ~ "/no_nmark.su2", su2_missing("NMARK"));
        twd("us.su2rd.no-nmark", 1713, () { new UnstructuredGrid(TD ~ "/no_nmark.su2", "su2text", true); });
        mkr(TD ~ "/bad_ndime.su2", su2_bad_value("NDIME", "two"));
        twd("us.su2rd-bad-ndime", 1407, () { new UnstructuredGrid(TD ~ "/bad_ndime.su2", "su2text", true); });
        mkr(TD ~ "/neg_ndime.su2", su2_bad_value("NDIME", "-1"));
        twd("us.su2rd-neg-ndime", 1407, () { new UnstructuredGrid(TD ~ "/neg_ndime.su2", "su2text", true); });
        mkr(TD ~ "/dim4.su2", su2_bad_value("NDIME", "4"));
        twd("us.su2rd-dim4", 1499, () { new UnstructuredGrid(TD ~ "/dim4.su2", "su2text", true); });
        mkr(TD ~ "/badcoord.su2", su2_bad_coord_line());
        twd("us.su2rd-badcoord", 1491, () { new UnstructuredGrid(TD ~ "/badcoord.su2", "su2text", true); });
        mkr(TD ~ "/nancoord.su2", su2_nan_coord_line());
        twd("us.su2rd-nancoord", 1491, () {
            auto g = new UnstructuredGrid(TD ~ "/nancoord.su2", "su2text", true);
            writefln("us.su2rd nancoord vtx[0]=%.17g bits=%s", g.vertices[0].x, bits(g.vertices[0].x));
        });
        mkr(TD ~ "/bigcoord.su2", su2_big_coord_line());
        twd("us.su2rd-bigcoord", 1491, () {
            auto g = new UnstructuredGrid(TD ~ "/bigcoord.su2", "su2text", true);
            writefln("us.su2rd bigcoord vtx[0]=%.17g bits=%s to!string=[%s]", g.vertices[0].x,
                     bits(g.vertices[0].x), to!string(g.vertices[0].x));
        });
        mkr(TD ~ "/badcount.su2", su2_bad_count("NELEM", "3"));
        twd("us.su2rd-too-many-elems", 1418, () { new UnstructuredGrid(TD ~ "/badcount.su2", "su2text", true); });
        mkr(TD ~ "/oobvid.su2", su2_oob_vertex_index());
        twd("us.su2rd-oob-vertex-id", 1509, () { new UnstructuredGrid(TD ~ "/oobvid.su2", "su2text", true); });
        mkr(TD ~ "/oobcid.su2", su2_oob_cell_index());
        twd("us.su2rd-oob-cell-id", 1458, () { new UnstructuredGrid(TD ~ "/oobcid.su2", "su2text", true); });
        mkr(TD ~ "/emptytag.su2in", su2_empty_marker_tag());
        twd("us.su2rd-empty-tag", 1715, () {
            auto g = new UnstructuredGrid(TD ~ "/emptytag.su2in", "su2text", true);
            dump_bnd("us.su2rd empty tag", g.boundaries);
        });
        twd("us.su2rd-missing-file", 1391, () {
            new UnstructuredGrid(TD ~ "/no_such_file.su2", "su2text", true);
        });
        mkr(TD ~ "/empty.su2", "");
        twd("us.su2rd-empty-file", 1407, () { new UnstructuredGrid(TD ~ "/empty.su2", "su2text", true); });
    }

    //================ us.vtk.* — write_to_vtk_file (D:1850) and the unfinished reader
    {
        auto gt = new UnstructuredGrid(pave_square(), "vtk2d");
        string p = TD ~ "/vtk2d.vtk";
        twd("us.vtk.2d", 1850, () { gt.write_to_vtk_file(p); });
        writefln("us.vtk.2d len=%d body=[%s]", len_of(p), body_of(p));
        auto gh = new UnstructuredGrid(vol_grid3d("vtk3d"), "vtk3d");
        string p2 = TD ~ "/vtk3d.vtk";
        twd("us.vtk.3d", 1850, () { gh.write_to_vtk_file(p2); });
        writefln("us.vtk.3d len=%d body=[%s]", len_of(p2), body_of(p2));
        // A cell of type none lands in vtk_element_types[0] == 0 (D:1871).
        auto gn = hand_quad2d("vtknone");
        gn.cells[0].cell_type = USGCell_type.none;
        string p3 = TD ~ "/vtknone.vtk";
        twd("us.vtk.cellnone", 1871, () { gn.write_to_vtk_file(p3); });
        writefln("us.vtk.cellnone body=[%s]", body_of(p3));
        // An empty label prints as an empty line where the title goes (D:1854).
        auto gl = new UnstructuredGrid(2, "");
        gl.vertices = [Vector3(0,0), Vector3(1,0), Vector3(1,1)];
        gl.nvertices = 3;
        gl.cells ~= new USGCell(USGCell_type.triangle, 0, [0,1,2], cast(size_t[])[], cast(int[])[]);
        gl.ncells = 1;
        gl.nfaces = 0; gl.nboundaries = 0; gl.niv = 3; gl.njv = 1; gl.nkv = 1;
        string p4 = TD ~ "/vtkemptylabel.vtk";
        gl.write_to_vtk_file(p4);
        writefln("us.vtk.emptylabel body=[%s]", body_of(p4));
        // read_from_vtk_text_file (D:1785) walks three header lines and then throws.
        string rv = TD ~ "/read.vtk";
        mkr(rv, "# vtk DataFile Version 2.0\nmy label\nASCII\nDATASET UNSTRUCTURED_GRID\nPOINTS 3 float\n");
        twd("us.vtk.rd-ok", 1795, () {
            auto g = new UnstructuredGrid(2, "rd");
            g.read_from_vtk_text_file(rv);
        });
        twd("us.vtk.rd-nobom", 1790, () {
            auto g = new UnstructuredGrid(2, "rd");
            g.read_from_vtk_text_file(TD ~ "/no_such.vtk");
        });
        string rv2 = TD ~ "/bad1.vtk";
        mkr(rv2, "# vtk DataFile Version 3.0\nlabel\nASCII\nDATASET UNSTRUCTURED_GRID\n");
        twd("us.vtk.rd-version", 1790, () {
            auto g = new UnstructuredGrid(2, "rd");
            g.read_from_vtk_text_file(rv2);
        });
        string rv3 = TD ~ "/bad2.vtk";
        mkr(rv3, "# vtk DataFile Version 2.0\nlabel\nBINARY\nDATASET UNSTRUCTURED_GRID\n");
        twd("us.vtk.rd-binary", 1792, () {
            auto g = new UnstructuredGrid(2, "rd");
            g.read_from_vtk_text_file(rv3);
        });
        string rv4 = TD ~ "/bad3.vtk";
        mkr(rv4, "# vtk DataFile Version 2.0\nlabel\nASCII\nDATASET STRUCTURED_GRID\n");
        twd("us.vtk.rd-dataset", 1793, () {
            auto g = new UnstructuredGrid(2, "rd");
            g.read_from_vtk_text_file(rv4);
        });
        string rv5 = TD ~ "/short.vtk";
        mkr(rv5, "# vtk DataFile Version 2.0\n");
        twd("us.vtk.rd-short", 1791, () {
            auto g = new UnstructuredGrid(2, "rd");
            g.read_from_vtk_text_file(rv5);
        });
        // The two formats the import constructor refuses.
        twd("us.vtk.xml-fmt", 840, () { new UnstructuredGrid(rv, "vtkxml", true); });
    }

    //================ us.stl.* — the inherited STL writer on a 2D unstructured grid
    {
        auto gt = new UnstructuredGrid(pave_square(), "stl2d");
        string p = TD ~ "/stl2d.stl";
        twd("us.stl.tri", 389, () { gt.write_to_stl_file(p); });
        writefln("us.stl.tri len=%d body=[%s]", len_of(p), body_of(p));
        writefln("us.stl.tri scale25 body=[%s]", (() { gt.write_to_stl_file(p, 25.4); return body_of(p); })());
        auto gq = new UnstructuredGrid(surf_grid2d("stlquad"), "stlquad");
        string p2 = TD ~ "/stlquad.stl";
        twd("us.stl.quad", 389, () { gq.write_to_stl_file(p2, 0.5); });
        writefln("us.stl.quad len=%d body=[%s]", len_of(p2), body_of(p2));
        twd("us.stl.3d", 391, () {
            auto g3 = new UnstructuredGrid(vol_grid3d("stl3d"), "stl3d");
            g3.write_to_stl_file(TD ~ "/stl3d.stl");
        });
        // A polygonal cell (5 vertices) reaches the default branch of the switch.
        auto gp = hand_quad2d("stlpenta");
        gp.cells[0].vtx_id_list = [0,1,2,3,0];
        twd("us.stl.penta", 427, () { gp.write_to_stl_file(TD ~ "/stlpenta.stl"); });
    }

    //================ us.foam.* — write_openFoam_polyMesh (D:1930-2162)
    {
        // The paving grid's faces carry no cell links (paver2d.fill_interior does not set
        // them), so the first thing it does through this writer is trip the owner assert
        // at D:1977. link_faces_back_to_cells (D:1258) is what repairs that, and it is
        // what read_from_gzip_file and read_from_su2_file call for themselves.
        auto graw = new UnstructuredGrid(pave_square(), "foam2draw");
        string topr = TD ~ "/foam2draw";
        twd("us.foam.unlinked", 1977, () { graw.write_openFoam_polyMesh(topr); });
        writefln("us.foam.unlinked points len=%d", len_of(topr ~ "/polyMesh/points"));
        graw.link_faces_back_to_cells();
        twd("us.foam.relinked", 1930, () { graw.write_openFoam_polyMesh(topr); });
        writefln("us.foam.relinked points len=%d faces len=%d", len_of(topr ~ "/polyMesh/points"),
                 len_of(topr ~ "/polyMesh/faces"));
        auto g = new UnstructuredGrid(TD ~ "/quad_rd.su2", "su2text", true);
        string top = TD ~ "/foam2d";
        twd("us.foam.2d", 1930, () { g.write_openFoam_polyMesh(top); });
        foreach (nm; ["points", "faces", "owner", "neighbour", "boundary"]) {
            string fp = top ~ "/polyMesh/" ~ nm;
            writefln("us.foam.2d %s len=%d body=[%s]", nm, len_of(fp), body_of(fp));
        }
        foreach (b; g.boundaries) {
            string fp = top ~ "/polyMesh/sets/" ~ b.tag;
            if (std.file.exists(fp)) {
                writefln("us.foam.2d set[%s] body=[%s]", b.tag, body_of(fp));
            } else {
                writefln("us.foam.2d set[%s] absent", b.tag);
            }
        }
        writefln("us.foam.2d label after write=[%s]", g.label);
        // The 3D hex grid, with internal faces and six boundary sets.
        auto g3 = new UnstructuredGrid(TD ~ "/hex_rd.su2", "su2text", true);
        string top3 = TD ~ "/foam3d";
        twd("us.foam.3d", 1930, () { g3.write_openFoam_polyMesh(top3); });
        foreach (nm; ["points", "faces", "owner", "neighbour", "boundary"]) {
            string fp = top3 ~ "/polyMesh/" ~ nm;
            writefln("us.foam.3d %s len=%d body=[%s]", nm, len_of(fp), body_of(fp));
        }
        dump_bnd("us.foam.3d", g3.boundaries);
        // An empty-tag boundary: D:2148 assigns to the grid's own `label` member (the
        // local `label` of D:2125 is out of scope here), then D:2149 opens
        // "<setsDir>/" as a file name.
        auto ge = new UnstructuredGrid(TD ~ "/quad_rd.su2", "su2text", true);
        ge.boundaries[0].tag = "";
        string topE = TD ~ "/foamempty";
        twd("us.foam.emptytag", 2149, () { ge.write_openFoam_polyMesh(topE); });
        writefln("us.foam.emptytag label after write=[%s]", ge.label);
        writefln("us.foam.emptytag boundary body=[%s]", body_of(topE ~ "/polyMesh/boundary"));
        // A face attached to no cell at all: the owner assert (D:1977).
        auto go = hand_quad2d("foamorphan");
        go.faces ~= new USGFace(cast(size_t[])[0, 2]);
        go.nfaces = go.faces.length;
        go.boundaries.length = 0;
        go.nboundaries = 0;
        twd("us.foam.unowned-face", 1977, () { go.write_openFoam_polyMesh(TD ~ "/foamorphan"); });
        // A boundary set that repeats a face: the face tally assert (D:1983).
        auto gr = hand_quad2d("foamdup");
        gr.boundaries[1].face_id_list ~= 0;
        gr.boundaries[1].outsign_list ~= 1;
        twd("us.foam.double-counted", 1983, () { gr.write_openFoam_polyMesh(TD ~ "/foamdup"); });
    }

    //================ us.imp.* — the import constructor (D:830-846)
    {
        // Same file, every format spelling.
        string good = TD ~ "/quad_rd.su2";
        twd("us.imp.vtkxml", 840, () { new UnstructuredGrid(good, "vtkxml", true); });
        twd("us.imp.unknown", 841, () { new UnstructuredGrid(good, "AVS", true); });
        twd("us.imp.empty-fmt", 841, () { new UnstructuredGrid(good, "", true); });
        twd("us.imp.case", 841, () { new UnstructuredGrid(good, "SU2TEXT", true); });
        twd("us.imp.missing-su2", 838, () { new UnstructuredGrid(TD ~ "/absent.su2", "su2text", true); });
        twd("us.imp.missing-gz", 836, () { new UnstructuredGrid(TD ~ "/absent.gz", "gziptext", true); });
        twd("us.imp.missing-raw", 837, () { new UnstructuredGrid(TD ~ "/absent.bin", "rawbinary", true); });
        twd("us.imp.missing-vtk", 839, () { new UnstructuredGrid(TD ~ "/absent.vtk", "vtktext", true); });
        // new_label wins over the label read from the file (D:843).
        twd("us.imp-label-kept", 843, () {
            auto g = new UnstructuredGrid(good, "su2text", true);
            writefln("us.imp label kept fromfile=[%s]", g.label);
        });
        twd("us.imp-label-given", 843, () {
            auto g = new UnstructuredGrid(good, "su2text", true, 1.0, false, "override");
            writefln("us.imp label given=[%s] niv=%d njv=%d nkv=%d", g.label, g.niv, g.njv, g.nkv);
        });
        // Directory as the file name.
        twd("us.imp-directory", 838, () { new UnstructuredGrid(TD, "su2text", true); });
    }

    //================ remaining reachable throws / asserts / the unittest
    // Nested helpers for grids built by hand (no reader, no paving dependency).
    UnstructuredGrid manGrid(int dims, int[] vtxPerFace)
    {
        auto g = new UnstructuredGrid(dims, "man");
        size_t idx = 0;
        foreach (nv; vtxPerFace) {
            size_t[] ids;
            foreach (kk; 0 .. nv) { ids ~= idx; idx += 1; }
            g.faces ~= new USGFace(ids);
        }
        g.nfaces = g.faces.length; g.nvertices = idx; g.niv = idx; g.njv = 1; g.nkv = 1;
        // get_boundary_grid (D:1074-1075) copies nvertices before the face switch, so
        // vertices must be populated for the face-count asserts (D:1082/1086/1090) to
        // be reached instead of the D:1075 copy throwing.
        foreach (kk; 0 .. idx) g.vertices ~= Vector3(cast(double)kk, cast(double)kk, 0.0);
        size_t[] fids; int[] os;
        foreach (i; 0 .. g.faces.length) { fids ~= i; os ~= 1; }
        g.boundaries ~= new BoundaryFaceSet("b", fids, os);
        g.nboundaries = 1;
        return g;
    }

    // A single triangle cell grid, wired by hand through link_faces_back_to_cells.
    UnstructuredGrid triGrid(string label="tri")
    {
        auto g = new UnstructuredGrid(2, label);
        g.vertices = [Vector3(0.0, 0.0), Vector3(1.0, 0.0), Vector3(0.5, 1.0)];
        g.nvertices = 3;
        size_t[][3] fl = [[0,1],[1,2],[2,0]];
        foreach (i; 0 .. 3) {
            g.faceIndices[makeFaceTag(fl[i])] = g.faces.length;
            g.faces ~= new USGFace(fl[i]);
        }
        g.nfaces = 3;
        g.cells ~= new USGCell(USGCell_type.triangle, 0, [0,1,2], [0,1,2], [1,1,1]);
        g.ncells = 1; g.nboundaries = 0; g.niv = 3; g.njv = 1; g.nkv = 1;
        g.link_faces_back_to_cells();
        return g;
    }

    UnstructuredGrid usg3d(string label="u3d") { return new UnstructuredGrid(vol_grid3d(label)); }

    //================ us.idx.* — abstract-table indices on a real grid, text() asserts
    {
        auto g = triGrid();
        writefln("us.idx.nvfc=%d vtk=%d ctype=%d", g.number_of_vertices_for_cell(0),
                 g.vtk_element_type_for_cell(0), g.get_cell_type(0));
        twd("us.idx.nvfc.oob", 996, () { g.number_of_vertices_for_cell(99); });
        twd("us.idx.gettype.oob", 1008, () { g.get_cell_type(99); });
        // get_vtx_id_list_for_cell has text()-message asserts (D:1013/1014/1015, :1031)
        writefln("us.idx.vtxlist=[%s]", g.get_vtx_id_list_for_cell(0));
        twd("us.idx.vtx.badi", 1013, () { g.get_vtx_id_list_for_cell(0, 1); });
        twd("us.idx.vtx.badj", 1014, () { g.get_vtx_id_list_for_cell(0, 1, 0); });
        twd("us.idx.vtx.badk", 1015, () { g.get_vtx_id_list_for_cell(0, 0, 1); });
        twd("us.idx.vtx.oob", 1031, () { g.get_vtx_id_list_for_cell(99); });
        // copy_vtx_id_list_for_cell into a pre-filled size_t[8]: the tail stays 999.
        size_t[8] cp = prefilled8();
        size_t nvtx;
        g.copy_vtx_id_list_for_cell(cp, nvtx, 0);
        writefln("us.idx.copy nvtx=%d copy=%s", nvtx, cp);
        // opIndex asserts (D:860/861/862, :870) on a real grid
        writefln("us.idx.op0 %s", (*g[0]).toString());
        twd("us.idx.op.i", 870, () { auto v = (*g[99]); });
        twd("us.idx.op.j", 861, () { auto v = (*g[0, 1]); });
        twd("us.idx.op.k", 862, () { auto v = (*g[0, 0, 1]); });
    }

    //================ us.conn.* — get_connected_cell_ids_for_cell (D:1045) [dep sgrid]
    {
        auto g = new UnstructuredGrid(surf_grid2d("conn"));
        writefln("us.conn.cell0=%s", g.get_connected_cell_ids_for_cell(0));
        writefln("us.conn.cellast=%s", g.get_connected_cell_ids_for_cell(g.ncells-1));
    }

    //================ us.sort.* — sort_faces_in_boundary_order (D:888) on a real grid
    {
        auto g = new UnstructuredGrid(surf_grid2d("sortg"));
        g.sort_faces_in_boundary_order();
        string ob;
        foreach (f; g.faces) ob ~= (f.is_on_boundary ? "B" : "I");
        writefln("us.sort nfaces=%d ninterior=%d pattern=%s", g.faces.length,
                 g.ninteriorfaces, ob);
        // D:914 reads faces[faceid] for each boundary entry; a dangling id throws there.
        auto h = manGrid(2, [2, 2]);
        h.boundaries[0].face_id_list[0] = 99;
        twd("us.sort.dangling", 914, () { h.sort_faces_in_boundary_order(); });
    }

    //================ us.bnd.* — get_boundary_grid (D:1057) asserts + GeometryException
    {
        // valid 2D -> 1D line boundary grid
        twd("us.bnd.2d2line", 1057, () {
            auto bg = cast(UnstructuredGrid)manGrid(2, [2]).get_boundary_grid(0);
            writefln("us.bnd.2d2line dims=%d nc=%d type=%s", bg.dimensions, bg.cells.length,
                     bg.cells[0].cell_type);
        });
        // valid 3D -> 2D quad boundary grid (4-vtx face)
        twd("us.bnd.3d2quad", 1057, () {
            auto bg = cast(UnstructuredGrid)manGrid(3, [4]).get_boundary_grid(0);
            writefln("us.bnd.3d2quad dims=%d nc=%d type=%s", bg.dimensions, bg.cells.length,
                     bg.cells[0].cell_type);
        });
        // valid 3D -> 2D triangle boundary grid (3-vtx face)
        twd("us.bnd.3d2tri", 1057, () {
            auto bg = cast(UnstructuredGrid)manGrid(3, [3]).get_boundary_grid(0);
            writefln("us.bnd.3d2tri type=%s", bg.cells[0].cell_type);
        });
        // dims==2, 3-vtx face -> assert D:1086 "Assumed 3D but it was not so."
        twd("us.bnd.d2face3", 1086, () { manGrid(2, [3]).get_boundary_grid(0); });
        // dims==2, 4-vtx face -> assert D:1090 "Assumed 3D but it was not so."
        twd("us.bnd.d2face4", 1090, () { manGrid(2, [4]).get_boundary_grid(0); });
        // dims==3, 2-vtx face -> assert D:1082 "Assumed 2D but it was not so."
        twd("us.bnd.d3face2", 1082, () { manGrid(3, [2]).get_boundary_grid(0); });
        // dims==3, 5-vtx face -> default -> GeometryException D:1094
        twd("us.bnd.d3face5", 1094, () { manGrid(3, [5]).get_boundary_grid(0); });
        // new_dimensions outside 1..2 -> GeometryException D:1067
        twd("us.bnd.dims1", 1067, () { manGrid(1, [2]).get_boundary_grid(0); });
        twd("us.bnd.dims0", 1067, () { manGrid(0, [2]).get_boundary_grid(0); });
        // boundary index out of range on a real grid (D:1069) [dep sgrid]
        twd("us.bnd.idx-oob", 1069, () { new UnstructuredGrid(surf_grid2d("boob")).get_boundary_grid(99); });
    }

    //================ us.blc.* — get_list_of_boundary_cells (D:1106) Exception D:1126
    {
        // boundary face with outsign 1 but no left_cell -> insideCell null -> D:1126
        twd("us.blc.nullcell", 1126, () { manGrid(2, [2]).get_list_of_boundary_cells(0); });
        // well-formed 2D grid returns a real cell list [dep sgrid]
        twd("us.blc.ok", 1106, () {
            auto g = new UnstructuredGrid(surf_grid2d("blcg"));
            writefln("us.blc.ok nbnd=%d bnd0=%s", g.nboundaries, g.get_list_of_boundary_cells(0));
        });
    }

    //================ us.vaf.* — vectorAreaOfFacet (D:1133) assert D:1136 [dep sgrid]
    {
        auto g = new UnstructuredGrid(surf_grid2d("vafg"));
        writefln("us.vaf.f2=[%s]", g.vectorAreaOfFacet([cast(size_t)0, cast(size_t)1]).toString());
        writefln("us.vaf.f3=[%s]", g.vectorAreaOfFacet([cast(size_t)0, cast(size_t)1, cast(size_t)2]).toString());
        writefln("us.vaf.f4=[%s]", g.vectorAreaOfFacet([cast(size_t)0, cast(size_t)1, cast(size_t)2, cast(size_t)3]).toString());
        writefln("us.vaf.f5=[%s]", g.vectorAreaOfFacet([cast(size_t)0, cast(size_t)1, cast(size_t)2, cast(size_t)3, cast(size_t)4]).toString());
        twd("us.vaf.empty", 1136, () { g.vectorAreaOfFacet([]); });
        twd("us.vaf.one", 1136, () { g.vectorAreaOfFacet([cast(size_t)0]); });
        writefln("us.vaf.cell0=[%s]", g.vectorAreaOfCell(g.cells[0]).toString());
    }

    //================ us.split.* — splitTriangleCell (D:1179)
    {
        twd("us.split.ok", 1179, () {
            auto g = triGrid("sp");
            g.splitTriangleCell(0);
            writefln("us.split.ok nv=%d nf=%d nc=%d", g.nvertices, g.faces.length, g.cells.length);
            foreach (i, c; g.cells) writefln("us.split.ok cell[%d] %s", i, c.toIOString());
        });
        // not 2D -> GeometryException D:1185 (message has embedded newlines)
        twd("us.split.not2d", 1185, () { usg3d("sp3").splitTriangleCell(0); });
        // 2D but not a triangle -> GeometryException D:1190 [dep sgrid]
        twd("us.split.notri", 1190, () {
            new UnstructuredGrid(surf_grid2d("spq")).splitTriangleCell(0);
        });
        // id out of range -> bare assert D:1181 "Assertion failure"
        twd("us.split.oobid", 1181, () { triGrid().splitTriangleCell(99); });
    }

    //================ us.link.* — link_faces_back_to_cells (D:1258) assert D:1263
    {
        auto g = new UnstructuredGrid(2, "lnk");
        g.faces ~= new USGFace(cast(size_t[])[0, 1]);
        g.cells ~= new USGCell(USGCell_type.line, 0, [0, 1], [0], [1, -1]); // 1 face, 2 outsigns
        twd("us.link.mismatch", 1263, () { g.link_faces_back_to_cells(); });
    }

    //================ us.join.* — joinGrid (D:2164) six asserts + outsign throw D:2342
    {
        // [dep sgrid] the master asserts D:2174/2176/2178 and the other-side D:2175/2177/2179
        twd("us.join.m_nvtx", 2174, () {
            auto m = new UnstructuredGrid(surf_grid2d("jm")); m.nvertices = 12345;
            m.joinGrid(new UnstructuredGrid(surf_grid2d("jo")));
        });
        twd("us.join.o_nvtx", 2175, () {
            auto m = new UnstructuredGrid(surf_grid2d("jm"));
            auto o = new UnstructuredGrid(surf_grid2d("jo")); o.nvertices = 54321;
            m.joinGrid(o);
        });
        twd("us.join.m_nface", 2176, () {
            auto m = new UnstructuredGrid(surf_grid2d("jm")); m.nfaces = 999;
            m.joinGrid(new UnstructuredGrid(surf_grid2d("jo")));
        });
        twd("us.join.o_nface", 2177, () {
            auto m = new UnstructuredGrid(surf_grid2d("jm"));
            auto o = new UnstructuredGrid(surf_grid2d("jo")); o.nfaces = 888;
            m.joinGrid(o);
        });
        twd("us.join.m_ncell", 2178, () {
            auto m = new UnstructuredGrid(surf_grid2d("jm")); m.ncells = 77;
            m.joinGrid(new UnstructuredGrid(surf_grid2d("jo")));
        });
        twd("us.join.o_ncell", 2179, () {
            auto m = new UnstructuredGrid(surf_grid2d("jm"));
            auto o = new UnstructuredGrid(surf_grid2d("jo")); o.ncells = 66;
            m.joinGrid(o);
        });
        // genuine same-object join short-circuits at D:2172 (asserts not reached)
        twd("us.join.self", 2172, () {
            auto m = new UnstructuredGrid(surf_grid2d("jm"));
            writefln("us.join.self same=%s", (m.joinGrid(m)) is m);
        });
        // an invalid outsign on a cell of the other grid -> GeometryException D:2342
        twd("us.join.badoutsign", 2342, () {
            auto m = new UnstructuredGrid(surf_grid2d("jm"));
            auto o = new UnstructuredGrid(surf_grid2d("jo"));
            o.cells[0].outsign_list[0] = 5;
            m.joinGrid(o);
        });
    }

    //================ us.stats.* — writeStats (D:2416): closed and open-cell cases
    {
        triGrid("stat_ok").writeStats();
        // an open cell: only two of the three edges are wired, both outsign +1, so the
        // vector-area sum is non-zero and D prints the per-cell non-zero area row.
        auto g = new UnstructuredGrid(2, "open");
        g.vertices = [Vector3(0,0), Vector3(1,0), Vector3(0,1)]; g.nvertices = 3;
        g.faces ~= new USGFace(cast(size_t[])[0,1]); g.faces ~= new USGFace(cast(size_t[])[1,2]);
        g.nfaces = 2; g.niv = 3; g.njv = 1; g.nkv = 1;
        g.cells ~= new USGCell(USGCell_type.triangle, 0, [0,1,2], [0,1], [1,1]);
        g.ncells = 1; g.nboundaries = 0;
        g.writeStats();
    }

    //================ us.vtxb.* — getVertexIdsOnBoundaries (D:2450) [dep sgrid]
    {
        writefln("us.vtxb.2d=%s", new UnstructuredGrid(surf_grid2d("vtxb")).getVertexIdsOnBoundaries());
        writefln("us.vtxb.3d=%s", usg3d("vtxb3").getVertexIdsOnBoundaries());
    }

    //================ us.bin.* — bin sort + enclosure search (grid.d) via an usgrid
    {
        // nbinx<1 -> GeometryException grid.d:284
        twd("us.bin.nbin0", 284, () { usg3d("bin0").sort_cells_into_bins(0, 5); });
        twd("us.bin.sort", 277, () {
            auto g = usg3d("bin3");
            g.sort_cells_into_bins(3, 3, 2);
            Vector3 p = g.cell_barycentre(0);
            size_t indx; bool found;
            g.find_enclosing_cell(p, indx, found);
            writefln("us.bin.sort nbx=%d nby=%d nbz=%d found=%s indx=%d",
                     g.nbx, g.nby, g.nbz, found, indx);
        });
    }

    //================ us.edge.* — the D:881 size_t underflow, non-positive dimensions
    {
        // toString (D:881) indexes faces[$-1]; with vertices present but faces empty,
        // $-1 underflows to 18446744073709551615 and D prints exactly that in the error.
        auto g = new UnstructuredGrid(2, "underflow");
        g.vertices = [Vector3(0, 0)]; g.nvertices = 1;   // faces empty
        twd("us.edge.faces_underflow", 881, () { writefln("us.edge.under=[%s]", to!string(g)); });
        // cell id out of range on the single get_vtx_id_list spelling (D:1031)
        auto g2 = new UnstructuredGrid(surf_grid2d("eoob"));
        twd("us.edge.cell_oob", 1031, () { g2.get_vtx_id_list_for_cell(99999); });
        // a USGCell_type outside the enum, printed through toIOString -> cell_names[N] OOB
        auto c = new USGCell(cast(USGCell_type)99, 1, [0,1], [2], [1]);
        twd("us.edge.oobtype.io", 304, () { writefln("us.edge.oobtype.io=[%s]", c.toIOString()); });
        // The PRINT form for a USGCell_type outside the enum, reached by an explicit
        // C-style cast (contrast to!USGCell_type(i) above, which VALIDATES and throws a
        // ConvException). %s / to!string of a cast(USGCell_type)99 yields the literal
        // "cast(USGCell_type)99"; USGCell.toString embeds it the same way.
        writefln("us.edge.oobtype.tostring=[%s]", to!string(cast(USGCell_type)99));
        writefln("us.edge.oobtype.cellrepr=[%s]", c.toString());
    }

    //================ us.unit.* — usgrid.d's own unittest (D:2484-2524) as rows
    // DEPENDS ON sgrid.d + surface.coonspatch. Replayed exactly, one row per assert.
    {
        auto p00 = Vector3(0.0, 0.1);
        auto p10 = Vector3(1.0, 0.1);
        auto p11 = Vector3(1.0, 1.1);
        auto p01 = Vector3(0.0, 1.1);
        auto my_patch = new CoonsPatch(p00, p10, p11, p01);
        UnivariateFunction[] cf = [new LinearFunction(), new LinearFunction(),
                                   new LinearFunction(), new LinearFunction()];
        auto my_grid = new StructuredGrid(my_patch, 11, 21, cf);
        my_grid.set_tag(Face.north, "my-special-boundary");
        string repr = to!string(my_grid);
        writefln("us.unit.writeln my_grid=%s", repr);
        writefln("us.unit.a1.canFind=%s", repr.canFind("my-special-boundary"));
        writefln("us.unit.a2.approx=%s", approxEqualVectors(*my_grid[5,5], Vector3(0.5, 0.35)));
        writefln("us.unit.writeln grid point 5 5 at x=%s y=%s", my_grid[5,5].x, my_grid[5,5].y);

        auto usg = new UnstructuredGrid(my_grid);
        string usg_repr = to!string(usg);
        writefln("us.unit.writeln usg=%s", usg_repr);
        writefln("us.unit.a3.canFind=%s", usg_repr.canFind("my-special-boundary"));

        usg.sort_cells_into_bins(10, 10);
        Vector3 my_point = Vector3(0.55, 0.325);
        size_t cell_indx = 0; bool found = false;
        usg.find_enclosing_cell(my_point, cell_indx, found);
        writefln("us.unit.writeln Search for cell enclosing my_point= %s", my_point);
        if (found) {
            writefln("us.unit.writeln     cell found, index= %d", cell_indx);
            writefln("us.unit.writeln     cell barycentre= %s", usg.cell_barycentre(cell_indx));
        } else {
            writefln("us.unit.writeln     cell not found");
        }
        writefln("us.unit.a4.found=%s", found == true);
        writefln("us.unit.a5.cell_indx=%d", cell_indx);
        writefln("us.unit.a6.approx=%s",
                 approxEqualVectors(usg.cell_barycentre(cell_indx), Vector3(0.55, 0.325)));
    }

    writefln("us.end.ok");

    //================ us.gap.* — the sites the other groups could not reach, measured
    // Each row here either witnesses a throw/assert that the main groups missed, or
    // records `no-throw` together with what actually happened, which is how the port
    // learns that a site is dead code rather than a missing test.
    {
        // --- D:1690 "invalid cell type in 3D". A quad (VTK 9) inside a 3D mesh has
        // nvtx == 4, which grid.d:155 routes to hex_cell_properties, so the 3D face
        // switch at D:1660 is reached with a cell type it does not handle.
        mkr(TD ~ "/quad_in3d.su2", join([
            "NDIME= 3", "", "NELEM= 1",
            "9 0 1 2 3 0",
            "", "NPOIN= 4",
            "0.0 0.0 0.0 0", "1.0 0.0 0.0 1", "1.0 1.0 0.0 2", "0.0 1.0 0.0 3",
            "", "NMARK= 1",
            "MARKER_TAG= a", "MARKER_ELEMS= 1", "9 0 1 2 3", ""], "\n"));
        twd("us.gap.1690 quad in 3d", 1690, () {
            new UnstructuredGrid(TD ~ "/quad_in3d.su2", "su2text", true);
        });
        // --- D:1656 "invalid cell type in 2D". Any 2D cell type that is not triangle or
        // quad is rejected earlier, by the orientation loop at D:1520, so this site is
        // unreachable. A line cell (VTK 3, USGCell_type.line) shows which one fires.
        mkr(TD ~ "/line_in2d.su2", join([
            "NDIME= 2", "", "NELEM= 1",
            "3 0 1 0",
            "", "NPOIN= 2", "0.0 0.0 0", "1.0 0.0 1",
            "", "NMARK= 1", "MARKER_TAG= a", "MARKER_ELEMS= 1", "3 0 1", ""], "\n"));
        twd("us.gap.1656 line in 2d", 1656, () {
            new UnstructuredGrid(TD ~ "/line_in2d.su2", "su2text", true);
        });
        // --- D:1611 "invalid number of corners on face". The corner lists handed to
        // add_face_to_cell are literals of length 2, 3 or 4 (D:1645-1688), so the default
        // branch is unreachable; the pyramid read below covers 3 and 4 corners and throws
        // nothing.
        twd("us.gap.1611 pyramid ok", 1611, () {
            mkr(TD ~ "/pyr.su2", su2_pyramid());
            auto g = new UnstructuredGrid(TD ~ "/pyr.su2", "su2text", true);
            writefln("us.gap.1611 faces=%s", map!(f => f.vtx_id_list.length)(g.faces));
        });
        // --- D:543 "paver generated a non-quad cell". CELL_LIST entries all come back
        // as "quad" from auto_cell_type for these boundaries, so the Error is not
        // reached; the two shapes tried here are the degenerate ones.
        twd("us.gap.543 triangle boundary", 543, () {
            Vector3[] bd = [Vector3(0.0, 0.0), Vector3(1.0, 0.0), Vector3(0.5, 0.5)];
            auto g = new UnstructuredGrid(bd, cast(BoundaryFaceSet[])[], "tri3");
            writefln("us.gap.543 types=%s", map!(c => c.cell_type)(g.cells));
        });
        twd("us.gap.543 five points", 543, () {
            Vector3[] bd = [Vector3(0.0, 0.0), Vector3(1.0, 0.0), Vector3(1.0, 1.0),
                            Vector3(0.5, 1.5), Vector3(0.0, 1.0)];
            auto g = new UnstructuredGrid(bd, cast(BoundaryFaceSet[])[], "penta5");
            writefln("us.gap.543 types=%s", map!(c => c.cell_type)(g.cells));
        });
        // --- D:963 assert in sort_faces_in_boundary_order. A face listed in two boundary
        // sets makes nboundaryfaces exceed nfaces first, so the underflow at D:921 wins.
        twd("us.gap.963 double listed", 963, () {
            auto g = hand_quad2d("gap963");
            g.boundaries[1].face_id_list ~= 0;
            g.boundaries[1].outsign_list ~= 1;
            g.sort_faces_in_boundary_order();
        });
        // --- D:1728 assert in the marker loop. A marker only reaches that assert for a
        // face found in faceIndices, and every such face was made by add_face_to_cell,
        // which attaches a cell on the spot. Measured: the marker path either succeeds or
        // throws D:1739.
        // --- D:2001 / D:2008: the openFoam outsign-consistency asserts, reached by
        // flipping one outsign on a face that sits on the block boundary.
        twd("us.gap.2001 or 2008 flipped", 2008, () {
            auto g = new UnstructuredGrid(TD ~ "/quad_rd.su2", "su2text", true);
            g.boundaries.length = 0;
            g.nboundaries = 0;
            g.sort_faces_in_boundary_order();
            g.write_openFoam_polyMesh(TD ~ "/gap2001");
        });
        auto gsign = new UnstructuredGrid(TD ~ "/quad_rd.su2", "su2text", true);
        gsign.cells[0].outsign_list[0] = -gsign.cells[0].outsign_list[0];
        twd("us.gap.2008 outward", 2008, () { gsign.write_openFoam_polyMesh(TD ~ "/gap2008"); });
        auto gsign2 = new UnstructuredGrid(TD ~ "/quad_rd.su2", "su2text", true);
        foreach (i, f; gsign2.faces) {
            if (f.left_cell !is null && f.right_cell is null) {
                foreach (j, fid; f.left_cell.face_id_list) {
                    if (fid == i) { f.left_cell.outsign_list[j] = -1; }
                }
                break;
            }
        }
        twd("us.gap.2001 inward", 2001, () { gsign2.write_openFoam_polyMesh(TD ~ "/gap2001"); });
        // --- D:2334 / D:2338: joinGrid re-derives the face links from the outsigns and
        // asserts that no face gets two cells on the same side. One flipped outsign in
        // the incoming grid does exactly that.
        mkr(TD ~ "/quad_left.su2", su2_left_block());
        mkr(TD ~ "/quad_right.su2", su2_right_block());
        auto gj1 = new UnstructuredGrid(TD ~ "/quad_left.su2", "su2text", true);
        auto gj2 = new UnstructuredGrid(TD ~ "/quad_right.su2", "su2text", true);
        gj2.cells[0].outsign_list[1] = -gj2.cells[0].outsign_list[1];
        twd("us.gap.2334 left clash", 2334, () { gj1.joinGrid(gj2); });
        auto gj3 = new UnstructuredGrid(TD ~ "/quad_left.su2", "su2text", true);
        auto gj4 = new UnstructuredGrid(TD ~ "/quad_right.su2", "su2text", true);
        gj4.cells[0].outsign_list[0] = -gj4.cells[0].outsign_list[0];
        twd("us.gap.2338 right clash", 2338, () { gj3.joinGrid(gj4); });
        // --- D:821/822/823: the three count asserts at the end of the StructuredGrid
        // constructor. They cannot be reached, because a wrong sg.niv makes D:587 call
        // sg.single_index first -- and single_index (grid.d:71) is NOT an abstract
        // override, so its in-contract does run. That contrast is the point of this row:
        // see the MEASURED FINDINGS block at the foot of this file.
        twd("us.gap.821 wrong niv", 821, () {
            auto sg = surf_grid2d("gap821");
            writefln("us.gap.821 src niv=%d njv=%d vlen=%d", sg.niv, sg.njv, sg.vertices.length);
            sg.niv = 4;
            auto q = new UnstructuredGrid(sg, "gap821u");
            writefln("us.gap.821 built nv=%d vlen=%d nf=%d flen=%d", q.nvertices,
                     q.vertices.length, q.nfaces, q.faces.length);
        });
        twd("us.gap.822 wrong njv", 822, () {
            auto sg = surf_grid2d("gap822");
            sg.njv = 3;
            auto q = new UnstructuredGrid(sg, "gap822u");
            writefln("us.gap.822 built nf=%d flen=%d nc=%d clen=%d", q.nfaces, q.faces.length,
                     q.ncells, q.cells.length);
        });
        // --- the .dup of a returned vertex id list (D:1018, :1034) is a real copy.
        auto gd = new UnstructuredGrid(TD ~ "/quad_rd.su2", "su2text", true);
        auto v1 = gd.get_vtx_id_list_for_cell(0);
        auto v2 = gd.get_vtx_id_list_for_cell(0, 0, 0);
        v1[0] = 9999;
        v2[1] = 8888;
        writefln("us.gap.dup v1=%s v2=%s cell=%s", v1, v2, gd.cells[0].vtx_id_list);
        size_t[8] tail = prefilled8();
        size_t nvt = 3;
        gd.copy_vtx_id_list_for_cell(tail, nvt, 0);
        writefln("us.gap.copy nvtx=%d tail=%s", nvt, tail);
        // j and k are ignored by the 1-argument spelling, so a wrong j is not an error
        // (D:1011 takes i, j, k but the body uses only i; the in-contract is dead).
        writefln("us.gap.ijk j=1 k=7 gives=%s", gd.get_vtx_id_list_for_cell(1, 1, 7));
    }
}

//-----------------------------------------------------------------
// MEASURED FINDINGS (comment only — this block does not change stdout, so the
// baseline stays byte-valid). Everything below was produced by the rows above on
// ldc2 1.40.0 at gdtk e587b235, compiled with -O0 and no -O flag.
//
// 1. `in` CONTRACTS ON AN OVERRIDE OF AN ABSTRACT METHOD ARE NOT ENFORCED. This is the
//    single most consequential thing in this file. usgrid.d declares eight such
//    contracts, all on overrides of grid.d's abstract table:
//      opIndex(size_t,size_t,size_t=0)            D:859-863  assert i<nvertices / j==0 / k==0
//      opIndex(size_t)                            D:870      assert indx<nvertices
//      get_vtx_id_list_for_cell(i,j,k=0) const    D:1012-1016 assert i<ncells / j==0 / k==0
//      get_vtx_id_list_for_cell(indx) const       D:1029-1032 assert indx<ncells
//    Measured, three ways:
//      * us.idx.vtx.badi / badj / badk and us.idx.op.j / op.k are `no-throw` rows: a call
//        with j=1 or k=7 succeeds and returns the list for i alone
//        (us.gap.ijk j=1 k=7 gives=[1, 2, 5, 4]).
//      * us.idx.op.i and us.idx.vtx.oob report core.exception.ArrayIndexError raised in the
//        BODY (usgrid.d:872 and :1034), not AssertError at the contract line (870, 1030).
//      * an isolated 20-line module reproduces the split exactly: a plain method's
//        `in { assert(...) }` throws AssertError on its own line; the same contract on an
//        `override` of an `abstract` method never runs (via the derived handle or the base
//        handle); an override of a CONCRETE virtual does run. So the trigger is
//        "implementation of an abstract member", not `override` alone.
//    Consequence for the port: those 8 asserts must NOT become eil_assert / eil_assert_msg.
//    The messages "index i is invalid", "index indx is invalid", text("index i=",i,
//    " is invalid, ncells=",ncells) are dead text in D, and a port that enforces them
//    produces a different exception class, a different message, and throws where D does
//    not throw at all. Contrast row: grid.d:71 single_index and :81 ijk_indices are plain
//    (non-abstract) methods and their contracts DO fire -- us.gap.821 shows the
//    consequence, that a wrong sg.niv is not caught by D:821 either (the counts stay
//    self-consistent at 16 vertices), so the constructor silently builds a stride-shifted
//    grid instead of asserting.
//
// 2. sameOrientation (D:134) does not test cyclic rotations. The comment at D:142-143 says
//    "the cycle of points may start with any of them, so we have to check them all", and
//    the loop uses bringToFront(test_list[1..$], test_list[$-1..$]) at D:147. Measured by
//    the us.orient.perm3 / perm4 sweep (all 6 and all 24 permutations) plus the
//    us.orient.seq3 / seq4 traces: element 0 is never moved, only the tail is rotated, so
//    D returns true for exactly 4 of the 24 orderings of a 4-point face and, of those, one
//    ([0,2,1] against [0,1,2]) is not a cycle at all. Reproduce D's tested SEQUENCE, not
//    the intended one; joinGrid's flip_new_face (D:2299) depends on it.
//
// 3. USGCell's copy constructor loses the id (D:236-238). `this(const USGCell other,
//    size_t id) { id = other.id; ... }` assigns to the parameter, so this.id is never
//    written and stays size_t.init == 0. us.cell.copyid shows orig=3, passed=77, copy=0.
//    It is not cosmetic: dup() (D:453) routes every cell through that constructor, so all
//    cells of a duplicated grid have id 0, and get_list_of_boundary_cells (D:1115) then
//    collapses its cell_indx dictionary onto one key -- us.blc.dup shows the last cell index
//    returned for every boundary face. Same family as Vector3.add's shadowing in
//    wave-02-brief section 5. Keep it.
//
// 4. D:1777 never throws what it looks like it throws. D:1776 is
//    format("SU2 grid has non-zero bounding vector area=", varea) -- a format string with no
//    conversion and one argument -- so the expression raises std.format.FormatException
//    "Orphan format arguments: args[0..1]" before the GeometryException of D:1777 is
//    constructed. Witnessed by us.su2rd.noclose (a mesh with one edge named by two markers).
//    The other message-building paths that DO work are us.su2rd.scrambled (D:1696-1701: the
//    errMsg ~= format(...) accumulation, with vectorAreaOfFacet rendered through %s and
//    therefore through %.6g) and us.split.not2d / notri (D:1183, :1188, ~= into a message
//    with embedded newlines, shown as <newline>).
//
// 5. write_openFoam_polyMesh mutates the grid when a boundary tag is empty. At D:2148 the
//    `label` of D:2125 is out of scope, so `label = format("boundary%04d", i)` assigns to
//    the Grid.label MEMBER, and D:2149 then opens "<setsDir>/" with an empty object name.
//    us.foam.emptytag shows both: the ErrnoException ("Is a directory") and
//    `label after write=[boundary0000]` on a grid whose label was "foamempty". The sibling
//    block at D:2123-2127 does NOT do this, because there `label` is a local. Keep both.
//
// 6. paver2d.fill_interior (D:502) leaves every face with a null left_cell and right_cell,
//    so a grid built by the paving constructor cannot be written to OpenFOAM:
//    us.foam.unlinked is core.exception.RangeError at D:1964, because D:1963 tests
//    `face.left_cell is null` first and then looks the NULL right_cell up in cellId. The
//    same grids round-trip fine once link_faces_back_to_cells has run (us.gz.*, us.foam.2d).
//
// 7. Associative-array iteration order is recorded, not hidden. us.conn.* pairs each raw
//    D order (`aa_order=`) with the sorted order (`sorted=`), and
//    us.dup.faceidx.aaord / us.sg2d.fidx.aaord give the raw key order of faceIndices next to
//    the sorted dump. The raw order was stable across two runs and across different
//    insertion orders for the same key set, but it is a Phobos bucket-order artefact that a
//    std::map port (eil::aa.hpp, lexicographic) will not reproduce; only D:1054
//    (`connectedCells.keys.dup`) lets it reach an observable value.
//
// 8. Sites that are NOT reachable, each with the row that proves it: D:543 (paver always
//    returns quads; the two degenerate boundaries tried end in paver.d's own AssertError
//    and Error, us.gap.543.*), D:963 (the ninteriorfaces underflow at D:921 wins first,
//    us.gap.963 prints index [18446744073709551615]), D:1611 (corner lists are literals of
//    2, 3, 4), D:1656 (the 2D orientation loop at D:1520 rejects the same cell types
//    earlier -- us.gap.1656 shows D:1544 firing), D:1728, D:2001 (D:2008 fires for the
//    mirror case, us.gap.2001 inward), D:2334 (us.gap.2334 no-throw while its sibling 2338
//    fires), D:821/822/823, D:1115-1118's dictionary collapse is silent (see 3). D:1690 IS
//    reachable but only with a 4-vertex cell in a 3D mesh (us.gap.1690 quad in 3d), because
//    grid.d:155 routes nvtx==4 to hex_cell_properties instead of refusing.
//
// 9. Formatting and byte-handling rules exercised here, all consistent with
//    wave-02-brief section 5: to!string(double) is %.6g (us.tstr.*, with the %.17g
//    spelling and the bit pattern on every row), Vector3 prints as Vector3(0.55, 0.325, 0)
//    through its own toString, arrays print as [1, 2.3, -4], the uninitialised Vector3 and
//    number fields of a blank grid are NaN (us.ctor.d* bb0/bb1/deltax), and to!string of an
//    enum MEMBER is the member's name (us.name.tostr, which is what D:280's message shows).
//    Two `.gz` rules: the payload is compared with GzipByLine and never as container bytes,
//    and read_from_su2_file's own stdout is part of the contract -- three dots for a mesh
//    with at least one cell, one vertex and one cell-face loop pass (D:1417, :1483, :1632),
//    then " (done) scale=%s dim=%s ncells=%d nbfs= %d" with scale through %.6g.
//
// 10. Rows whose message contains a byte outside printable ASCII carry msgunprint>0 and the
//    byte is shown as `?`. Only three rows do: the raw-binary bad-header errors, where
//    D:1339's `new char[expected_header.length]` keeps stale buffer bytes after a short
//    rawRead and D:1343 pastes them into the message with to!string(char[]). The printable
//    prefix and the count are the contract; the bytes themselves are uninitialised memory
//    and are not portable in either direction.
