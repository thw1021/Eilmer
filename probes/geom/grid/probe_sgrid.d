// probes/geom/grid/probe_sgrid.d — what geom/grid/sgrid.d (module geom.grid.sgrid)
// produces, measured against ldc2 1.40.0 at gdtk e587b235.
//
// StructuredGrid is the lattice grid class: 8 constructors, the 17-member abstract
// table of grid.d:93-122 implemented over an (i,j,k) index, five file formats in and
// out, subgrid/boundary/join/slab/wedge surgery, a 2D mesh-quality functional with a
// Nelder-Mead optimiser behind it, and the Gridpro/Plot3D importers. sgrid.d has ONE
// `unittest` block (D:1833-1865, 15 asserts and one writeln), and its writeln of
// `to!string(my_grid)` is replayed here as the sg.unit.* rows.
//
// Build and capture (reproducible; the gdtk tree itself is read-only):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/grid \
//     probes/geom/grid/probe_sgrid.d -of=/tmp/probe_g4 -od/tmp/probe_obj_g4
//   /tmp/probe_g4 > baselines/e587b235/probes/geom_grid_sgrid.txt
// @nogc has to go because ldc2 1.40 rejects `throw new ...` inside sgrid.d's own @nogc
// members (D:217, :235, :246, :257, :308, :356, :1534) — a compile-time attribute with
// no runtime effect, and the C++ port has no @nogc to carry.
//
// Every artefact goes under /tmp/probe_g4_dir/, never the repository working tree.
//
// What each group pins down (row prefixes, all sg.*):
//   sg.ctor.*   the size-lattice constructor (D:35-70) and its dimension inference
//               (D:39), ncells/bcTags per dimension, the untouched .init of the
//               vertex array (NaN, wave-02-brief section 5), and the size_t wrap of
//               ncells for niv==0 (D:44). Default arguments nkv=1 and label="" (D:35).
//   sg.idx.*    single_index/ijk_indices (grid.d:70-91) over real lattices with the
//               k=0 default and every out-of-range contract assert.
//   sg.opidx.*  StructuredGrid.opIndex (D:206-221) — its own in-contract assert texts
//               are the *formatted* ones (text("index i=",i," is invalid, niv=",niv)),
//               which differ from grid.d's plain "index i is invalid".
//   sg.cell.*   get_vtx_id_list_for_cell / copy_vtx_id_list_for_cell, both spellings,
//               over 1D/2D/3D. Includes the D:345 quirk (`nkc = njv-1`, not `nkv-1`)
//               and D:337's missing `nvtx` write in the ijk form, and the two
//               GeometryException wordings, and D:366's "Zero cells" guard.
//   sg.abst.*   the full abstract table reached through a `Grid` handle (D:93-122).
//   sg.str.*    toString (D:223-232) — format("%s") of Vector3 (%.6g) and of bcTags.
//   sg.tag.*    set_tag (D:174-183) in and out of range; set_tags (D:185-204) over all
//               six dictionary keys in 1D/2D/3D and the returned count.
//   sg.dup.*    dup (D:169-172) and the copy constructor (D:162-167): the copy is a
//               new object (D class semantics) and bcTags is .dup'd.
//   sg.sub.*    subgrid (D:376-420) and all six Error wordings (three dimension
//               guards, three overrun guards).
//   sg.bnd.*    get_boundary_grid (D:422-490) for all six faces, with the note that
//               D:451 sizes the new grid with `new_nkv` but fills with `new_njv`, and
//               D:481's Face.bottom loop runs to `niv`/`njv` (the parent's counts)
//               rather than new_niv/new_njv; plus the 2D GeometryException.
//   sg.blc.*    get_list_of_boundary_cells (D:492-578) in 3D (6 faces) and 2D (4).
//   sg.path.*   the Path constructor (D:73-79) and make_grid_from_path (D:580-592).
//   sg.surf.*   the ParametricSurface constructor (D:82-101) with "simple", "linear",
//               "bilinear" and an unknown interpolation; make_grid_from_surface
//               (D:595-617) and _bilinear (D:620-645). clusterf shorter than 4 gets
//               LinearFunction padding (D:89).
//   sg.rgrid.*  the parameter-redistributed constructor (D:104-113) and the cubic
//               Bezier remap inside make_grid_from_surface (D:648-688) — every `^^`
//               in D:664-670 stays a pow(), never a multiplication.
//   sg.vol.*    the ParametricVolume constructor (D:116-136), make_grid_from_volume
//               (D:690-732) and _trilinear (D:734-781), clusterf padded to 12, and the
//               D:724 `(1-r)` integer-vs-float spelling next to D:722 `(1.0-r)`.
//   sg.readtxt.* read_from_text_file (D:783-852) with and without the VTK header, the
//               dimension inference (D:805-819), and every failure path.
//   sg.readgz.* / sg.readraw.* the two override readers, including the version guards
//               (D:862-865, :934-937) and the tag sections.
//   sg.wgz.*    write_to_gzip_file (D:999-1040) then read back — compared as the
//               DECOMPRESSED payload (the container carries an mtime; grid brief §2.1).
//   sg.wraw.*   write_to_raw_binary_file (D:1042-1082) dumped as hex, plus its round
//               trip. Header/label/count/vertex/tag layout is byte contract.
//   sg.vtk.*    write_to_vtk_file (D:1084-1102) full body.
//   sg.su2.*    write_to_su2_file (D:1312-1318) full body. NOTE: the D body delegates
//               to UnstructuredGrid (usgrid.d, stage G5, not in the C++ tree yet), so
//               these rows are captured for that stage and are NOT replayed by
//               test_geom_grid_sgrid.cpp — see the header of that test.
//   sg.join.*   joinGrid (D:1130-1256): "east"/"imax"/"north"/"jmax", the four guard
//               wordings, the vertex-coincidence test with its Vector3 texts, and the
//               reshuffle loops (D:1227-1245) whose `for (auto j = njv-1; j > 0; --j)`
//               on a size_t never reaches j==0.
//   sg.slab.* / sg.wedge.* makeSlabGrid (D:1258-1280) both overloads and both
//               symmetry branches, makeWedgeGrid (D:1282-1310) with its atan2/cos/sin
//               chain (D:1293-1304) and nkv_new=2 and 3, and the two `assert(nkv==1)`.
//   sg.bad.*    measure_of_badness (D:1320-1394) including the D:1337 typo (`b.set`
//               where `c` is meant) which the port must keep, the local `ncells`
//               shadow (D:1376), double.max seeding (D:1327) and 3D "Not implemented.".
//   sg.rs.*     rs_grids_to_list / list_to_rs_grids (D:1472-1522) and their
//               "data array too short" asserts.
//   sg.determ.* determine_rs_grids (D:1396-1423) — a nested objective function driving
//               nm.nelmin.minimize over 16 parameters (brief 4b.5: a capturing lambda).
//               conv_flag/fdata/nfe/nres and the 16 converged values are the rows.
//   sg.rotq.*   rotateGrid (D:1425-1465) — note `abs(norm)-1.0 > 1e-8` (no fabs of the
//               difference) and the Exception message with an embedded newline
//               (D:1439-1440), shown here as <newline>.
//   sg.rot.*    the two inherited Grid.rotate entries (grid.d:443, :481) on a real
//               lattice: identity, 180 degrees, a centre, an axis of length 2.
//   sg.uflowz.* uflowz (D:1534-1540) at the ten unittest values plus the exact
//               threshold values 1e-30/1e29/1e30, -0.0, nan and both infinities.
//   sg.vtkhdr.* read_VTK_header_line (D:1544-1561): case-insensitive search, the
//               empty-line end-of-file break and its Error text.
//   sg.gridpro.* import_gridpro_grid (D:1565-1612) — its own writeln lines to stdout,
//               scale, '#' comment skip, uflowz clamping of mm-scale values.
//   sg.rsgp.*   rotate_gridpro_blocks (D:1756-1798): the three rotations and the
//               fall-through that returns THE SAME object.
//   sg.fswap.*  grid_faceswap (D:1800-1828): the second `if` overwrites the first, and
//               both-false returns a null handle.
//   sg.plot3d.* writeGridsAsPlot3D (D:1711-1754) and importPlot3DGrid (D:1614-1709),
//               single- and multi-block, dim=2 and 3, the gzipped path through
//               pipeShell("gunzip -c ...") (D:1623) and the invalid-dim Error.
//   sg.ccp.* / sg.pic.* / sg.bin.* / sg.fsc.* the inherited Grid services on a real
//               lattice: compute_cell_properties (grid.d:124) on degenerate, normal and
//               inverted cells, point_is_inside_cell (grid.d:178), the bin sort
//               (grid.d:277) and the fast/slow enclosure searches (grid.d:320, :355) at
//               points on and one ULP off the bin boundaries.
//   sg.unit.*   the writeln of sgrid.d's own unittest (D:1845) plus the four tag
//               canFind rows and the two approxEqualVectors rows (D:1847-1852). The 15
//               asserts are ported as executable assertions in
//               tests/test_geom_grid_sgrid.cpp, in source order with their D line
//               numbers; the uflowz ones (D:1854-1864) are also rows here.
//
// Rows whose text comes from D's Throwable.toString() are truncated at the
// "exception=" marker (see the tryit note below): that tail is
// "core.exception.ConvException@/usr/include/d/.../utf.d(187): ..." and names a Phobos
// source line, which no C++ port can produce. The class distinction itself is kept —
// tryit tags each row `exc=` (an Exception) or `err=` (an Error), and D's
// `catch (Exception e)` at D:842 only takes the first, so the tag carries exactly the
// portable part of the text.
import std.algorithm;
import std.array;
import std.conv;
import std.file;
import std.format;
import std.math;
import std.range;
import std.stdio;
import std.string;
import std.process : pipeShell;

import gzip;
import nm.number;
import nm.nelmin;

import geom.elements;
import geom.geometry_exception;
import geom.gpath.line;
import geom.gpath.path;
import geom.grid.grid;
import geom.grid.sgrid;
import geom.misc.univariatefunctions;
import geom.surface.coonspatch;
import geom.surface.parametricsurface;
import geom.volume.parametricvolume;

//-----------------------------------------------------------------
// helpers

string TD = "/tmp/probe_g4_dir";

string nl_escape(string s)
{
    return replace(s, "\n", "<newline>");
}

int len_of(string p)
{
    return cast(int)(cast(ubyte[])read(p)).length;
}

/// Whole file body, newlines marked, for text artefacts.
string body_of(string p)
{
    return join(split(cast(string)read(p), "\n"), "<newline>");
}

/// Decompressed payload of a .gz artefact — the container bytes carry an mtime and
/// differ between two runs of D itself, so only the payload is a contract.
string gunzip_body_of(string p)
{
    auto r = new GzipByLine(p);
    string[] L;
    while (!r.empty) { L ~= r.front; r.popFront(); }
    return join(L, "<newline>");
}

string hex_of(string p)
{
    auto b = cast(ubyte[])read(p);
    string s;
    foreach (x; b) { s ~= format("%02x", x); }
    return s;
}

void mk(string p, string body)
{
    std.file.write(p, body);
}

/// One vertex per row, in D's own order, at %.17g.
void dump_verts(string tag, Vector3[] vv)
{
    foreach (i, v; vv) {
        writefln("%s vtx[%d] %.17g %.17g %.17g", tag, i, v.x, v.y, v.z);
    }
}

void dump_grid(string tag, StructuredGrid g)
{
    if (g is null) { writefln("%s isnull=true", tag); return; }
    writefln("%s dims=%d niv=%d njv=%d nkv=%d ncells=%d nvertices=%d label=%s", tag,
             g.dimensions, g.niv, g.njv, g.nkv, g.ncells, g.nvertices, g.label);
    dump_verts(tag, g.vertices);
    writefln("%s vtx_id=%s", tag, g.vtx_id);
    writefln("%s bcTags=%s", tag, g.bcTags);
}

/// The same for a Grid handle (so the abstract-table rows can print through it).
void dump_grid_base(string tag, Grid g)
{
    writefln("%s dims=%d niv=%d njv=%d nkv=%d ncells=%d nvertices=%d label=%s", tag,
             g.dimensions, g.niv, g.njv, g.nkv, g.ncells, g.nvertices, g.label);
    dump_verts(tag, g.vertices);
}

void dump_ids(string tag, size_t[] ids)
{
    writefln("%s ids=%s", tag, ids);
}

void dump_copy(string tag, size_t[8] c, size_t nvtx)
{
    writefln("%s copy=%s nvtx=%d", tag, c, nvtx);
}

/// A pre-filled 8-slot array, so an unwritten tail is observable.
size_t[8] prefilled()
{
    size_t[8] c;
    foreach (ref e; c) { e = 999; }
    return c;
}

void dump_doubles(string tag, double[] d)
{
    foreach (i, v; d) { writefln("%s d[%d] %.17g", tag, i, v); }
}

void dump_g2d(string tag, double[4][4] a)
{
    foreach (i; 0 .. 4) {
        writefln("%s row[%d] %.17g %.17g %.17g %.17g", tag, i, a[i][0], a[i][1],
                 a[i][2], a[i][3]);
    }
}

void tryit(string tag, void delegate() dg)
{
    try {
        dg();
        writefln("%s done", tag);
    }
    catch (Exception e) {
        writefln("%s exc=%s", tag, nl_escape(e.msg));
    }
    catch (Error e) {
        writefln("%s err=%s", tag, nl_escape(e.msg));
    }
}

/// Like tryit, but truncates the message at the "exception=" marker (see header).
void tryit_cut(string tag, void delegate() dg)
{
    try {
        dg();
        writefln("%s done", tag);
    }
    catch (Exception e) {
        writefln("%s exc=%s", tag, cut_at(e.msg));
    }
    catch (Error e) {
        writefln("%s err=%s", tag, cut_at(e.msg));
    }
}

string cut_at(string m)
{
    auto i = indexOf(m, "exception=");
    if (i < 0) return nl_escape(m);
    return nl_escape(m[0 .. i]) ~ "exception=<truncated>";
}

//-----------------------------------------------------------------
// A concrete trilinear volume, local to the probe: sgrid only needs the abstract base.

class ProbeVolume : ParametricVolume {
    Vector3 lo, dx, dy, dz, c1, c2;

    this(Vector3 lo, Vector3 dx, Vector3 dy, Vector3 dz) {
        this.lo = lo; this.dx = dx; this.dy = dy; this.dz = dz;
        this.c1 = Vector3(0.11, -0.07, 0.05);
        this.c2 = Vector3(0.03, 0.02, -0.04);
    }
    override Vector3 opCall(double r, double s, double t) const {
        Vector3 p = lo + dx*r + dy*s + dz*t;
        p += c1*(r*s);
        p += c2*(r*t);
        return p;
    }
    override ParametricVolume dup() const {
        return new ProbeVolume(lo, dx, dy, dz);
    }
    override string toString() const { return "ProbeVolume"; }
}

//-----------------------------------------------------------------

StructuredGrid mk2d(size_t niv, size_t njv, string label="g2d")
{
    return new StructuredGrid(niv, njv, 1, label);
}

/// A 5x4 grid on a Coons patch, linear clustering, for the reader/writer rows.
StructuredGrid surf_grid2d(string label="sg2d")
{
    auto p00 = Vector3(0.0, 0.1);
    auto p10 = Vector3(1.0, 0.1);
    auto p11 = Vector3(1.0, 1.1);
    auto p01 = Vector3(0.0, 1.1);
    auto patch = new CoonsPatch(p00, p10, p11, p01);
    UnivariateFunction[] cf = [new LinearFunction(0.0, 1.0), new LinearFunction(0.0, 1.0),
                              new LinearFunction(0.0, 1.0), new LinearFunction(0.0, 1.0)];
    return new StructuredGrid(patch, 5, 4, cf, "simple", label);
}

StructuredGrid vol_grid3d(string label="sg3d")
{
    auto vol = new ProbeVolume(Vector3(0.1, 0.2, 0.3), Vector3(1.0, 0.0, 0.0),
                               Vector3(0.0, 1.0, 0.0), Vector3(0.0, 0.0, 1.0));
    UnivariateFunction[] cf;
    return new StructuredGrid(vol, 3, 3, 2, cf, "simple", label);
}

void main()
{
    if (!std.file.exists(TD)) std.file.mkdir(TD);

    //================ sg.ctor.* — size lattice (D:35-70)
    {
        auto g1 = new StructuredGrid(5, 1, 1, "line");
        dump_grid("sg.ctor.1d", g1);
        auto g2 = new StructuredGrid(5, 4);           // nkv defaults to 1 (D:35)
        dump_grid("sg.ctor.2d", g2);
        auto g3 = new StructuredGrid(3, 3, 2, "cube");
        dump_grid("sg.ctor.3d", g3);
        auto g4 = new StructuredGrid(1, 1, 1, "pt");
        dump_grid("sg.ctor.1pt", g4);
        // niv == 0: ncells = niv-1 wraps (D:44), vertices stays empty.
        auto g5 = new StructuredGrid(0, 1, 1, "zero");
        writefln("sg.ctor.n0 dims=%d niv=%d njv=%d nkv=%d ncells=%d nvertices=%d vlen=%d",
                 g5.dimensions, g5.niv, g5.njv, g5.nkv, g5.ncells, g5.nvertices,
                 g5.vertices.length);
        tryit("sg.ctor.n0.tostr", () { writefln("sg.ctor.n0.repr=%s", to!string(g5)); });
        // Untouched vertices are .init (NaN), and vtx_id is filled (D:62-69).
        auto g6 = new StructuredGrid(2, 2, 2, "nan");
        dump_grid("sg.ctor.nan", g6);
        writefln("sg.ctor.gridtype=%s", g1.grid_type);
        writefln("sg.ctor.dims 1d=%d 2d=%d 3d=%d 1pt=%d", g1.dimensions, g2.dimensions,
                 g3.dimensions, g4.dimensions);
    }

    //================ sg.idx.* — single_index / ijk_indices (grid.d:70-91)
    {
        auto g = new StructuredGrid(3, 3, 2, "idx");
        foreach (k; 0 .. 2) {
            foreach (j; 0 .. 3) {
                foreach (i; 0 .. 3) {
                    writefln("sg.idx.3d i=%d j=%d k=%d si=%d", i, j, k, g.single_index(i, j, k));
                }
            }
        }
        writefln("sg.idx.kdefault i=2 j=1 si=%d", g.single_index(2, 1));
        foreach (n; 0 .. 18) {
            auto t = g.ijk_indices(n);
            writefln("sg.idx.inv n=%d ijk=%s", n, t);
        }
        auto g2 = new StructuredGrid(5, 4, 1, "idx2");
        foreach (n; 0 .. 20) {
            auto t = g2.ijk_indices(n);
            writefln("sg.idx.inv2d n=%d ijk=%s si=%d", n, t, g2.single_index(t[0], t[1], t[2]));
        }
        writefln("sg.idx.2d.kdefault si(1,2)=%d", g2.single_index(1, 2));
        tryit("sg.idx.oob.i", () { g.single_index(3, 0, 0); });
        tryit("sg.idx.oob.j", () { g.single_index(0, 3, 0); });
        tryit("sg.idx.oob.k", () { g.single_index(0, 0, 2); });
        tryit("sg.idx.oob.ijk", () { g.ijk_indices(18); });
        // 1D lattice: njv==nkv==1, so j and k must be 0.
        auto g1 = new StructuredGrid(4, 1, 1, "idx1");
        writefln("sg.idx.1d si(2,0,0)=%d", g1.single_index(2, 0, 0));
        foreach (n; 0 .. 4) {
            writefln("sg.idx.1d.inv n=%d ijk=%s", n, g1.ijk_indices(n));
        }
    }

    //================ sg.opidx.* — opIndex (D:206-221)
    {
        auto g = surf_grid2d("opx");
        foreach (j; 0 .. 4) {
            foreach (i; 0 .. 5) {
                writefln("sg.opidx.2d i=%d j=%d %.17g %.17g %.17g", i, j, (*g[i,j]).x,
                         (*g[i,j]).y, (*g[i,j]).z);
            }
        }
        writefln("sg.opidx.kdef=%.17g", (*g[2, 1]).x);
        foreach (n; 0 .. 20) {
            writefln("sg.opidx.1arg n=%d %.17g", n, (*g[n]).x);
        }
        tryit("sg.opidx.oob.i", () { auto q = g[5, 0]; });
        tryit("sg.opidx.oob.j", () { auto q = g[0, 4]; });
        tryit("sg.opidx.oob.k", () { auto q = g[0, 0, 1]; });
        tryit("sg.opidx.oob.indx", () { auto q = g[20]; });
        auto g3 = vol_grid3d("opx3d");
        tryit("sg.opidx.3d.oob", () { auto q = g3[0, 0, 2]; });
        writefln("sg.opidx.3d i=1 j=1 k=1 %.17g %.17g %.17g", (*g3[1,1,1]).x,
                 (*g3[1,1,1]).y, (*g3[1,1,1]).z);
    }

    //================ sg.cell.* — vertex id lists (D:262-374)
    {
        auto g = new StructuredGrid(4, 3, 2, "cell3d");
        foreach (k; 0 .. 1) {
            foreach (j; 0 .. 2) {
                foreach (i; 0 .. 3) {
                    dump_ids(format("sg.cell.3d i=%d j=%d k=%d", i, j, k),
                             g.get_vtx_id_list_for_cell(i, j, k));
                    auto c = prefilled(); size_t nvtx = 0;
                    g.copy_vtx_id_list_for_cell(c, nvtx, i, j, k);
                    dump_copy(format("sg.cell.3dc i=%d j=%d k=%d", i, j, k), c, nvtx);
                }
            }
        }
        writefln("sg.cell.kdef ids=%s", g.get_vtx_id_list_for_cell(0, 0));
        auto g2 = new StructuredGrid(4, 3, 1, "cell2d");
        foreach (j; 0 .. 2) {
            foreach (i; 0 .. 3) {
                dump_ids(format("sg.cell.2d i=%d j=%d", i, j),
                         g2.get_vtx_id_list_for_cell(i, j));
                auto c = prefilled(); size_t nvtx = 0;
                g2.copy_vtx_id_list_for_cell(c, nvtx, i, j);
                dump_copy(format("sg.cell.2dc i=%d j=%d", i, j), c, nvtx);
            }
        }
        auto g1 = new StructuredGrid(5, 1, 1, "cell1d");
        foreach (i; 0 .. 4) {
            dump_ids(format("sg.cell.1d i=%d", i), g1.get_vtx_id_list_for_cell(i));
            auto c = prefilled(); size_t nvtx = 0;
            g1.copy_vtx_id_list_for_cell(c, nvtx, i);
            dump_copy(format("sg.cell.1dc i=%d", i), c, nvtx);
        }
        // The single-index forms. For a 3D lattice with njv != nkv, D:345's
        // `nkc = njv-1` and D:363's `nkc = nkv-1` must diverge: 3x3x2 -> both are 2,
        // so a 4x3x2 lattice is the one that shows it (nic=3, njc=2, nkc(D:345)=2).
        auto gd = new StructuredGrid(4, 3, 5, "diverge");
        writefln("sg.cell.div niv=%d njv=%d nkv=%d ncells=%d", gd.niv, gd.njv, gd.nkv,
                 gd.ncells);
        foreach (n; [0, 1, 2, 3, 6, 9, 12, 18, 23, 24, 30]) {
            tryit(format("sg.cell.div.iindx n=%d", n), () {
                dump_ids(format("sg.cell.div.iindx.out n=%d", n),
                         gd.get_vtx_id_list_for_cell(cast(size_t)n));
            });
            tryit(format("sg.cell.div.copy n=%d", n), () {
                auto c = prefilled(); size_t nvtx = 0;
                gd.copy_vtx_id_list_for_cell(c, nvtx, cast(size_t)n);
                dump_copy(format("sg.cell.div.copy.out n=%d", n), c, nvtx);
            });
        }
        foreach (n; [0, 1, 2, 3, 5, 6, 9, 12]) {
            tryit(format("sg.cell.2dindx n=%d", n), () {
                dump_ids(format("sg.cell.2dindx.out n=%d", n),
                         g2.get_vtx_id_list_for_cell(cast(size_t)n));
            });
        }
        foreach (n; [0, 1, 2, 3, 4, 7]) {
            tryit(format("sg.cell.1dindx n=%d", n), () {
                dump_ids(format("sg.cell.1dindx.out n=%d", n),
                         g1.get_vtx_id_list_for_cell(cast(size_t)n));
            });
        }
        tryit("sg.cell.1d.jnonzero", () { g1.get_vtx_id_list_for_cell(0, 1, 0); });
        tryit("sg.cell.1d.knonzero", () { g1.get_vtx_id_list_for_cell(0, 0, 1); });
        tryit("sg.cell.2d.jbad", () { g2.get_vtx_id_list_for_cell(0, 3); });
        tryit("sg.cell.2d.kbad", () { g2.get_vtx_id_list_for_cell(0, 0, 1); });
        tryit("sg.cell.3d.kbad", () { g.get_vtx_id_list_for_cell(0, 0, 2); });
        tryit("sg.cell.3d.ibad", () { g.get_vtx_id_list_for_cell(3, 0, 0); });
        // Zero cells in a direction (D:366-368).
        auto gz = new StructuredGrid(1, 1, 1, "onept");
        tryit("sg.cell.zero", () {
            auto c = prefilled(); size_t nvtx = 0;
            gz.copy_vtx_id_list_for_cell(c, nvtx, 0);
        });
        tryit("sg.cell.zero.ijk", () { gz.get_vtx_id_list_for_cell(0, 0, 0); });
        // A 1D grid asked for a j!=0 cell through the index form.
        tryit("sg.cell.1dindx.oob", () { g1.get_vtx_id_list_for_cell(4); });
    }

    //================ sg.abst.* — the abstract table through a Grid handle
    {
        Grid g = surf_grid2d("abst2d");
        writefln("sg.abst.2d nvtx=%d vtktype=%d celltype=%d", g.number_of_vertices_for_cell(0),
                 g.vtk_element_type_for_cell(0), g.get_cell_type(0));
        Grid g3 = vol_grid3d("abst3d");
        writefln("sg.abst.3d nvtx=%d vtktype=%d celltype=%d", g3.number_of_vertices_for_cell(0),
                 g3.vtk_element_type_for_cell(0), g3.get_cell_type(0));
        Grid g1 = new StructuredGrid(5, 1, 1, "abst1d");
        writefln("sg.abst.1d nvtx=%d vtktype=%d celltype=%d", g1.number_of_vertices_for_cell(0),
                 g1.vtk_element_type_for_cell(0), g1.get_cell_type(0));
        dump_grid_base("sg.abst.handle2d", g);
        dump_ids("sg.abst.vtxlist", g.get_vtx_id_list_for_cell(1, 1));
        {
            auto c = prefilled(); size_t nvtx = 0;
            g.copy_vtx_id_list_for_cell(c, nvtx, 1, 1);
            dump_copy("sg.abst.copy", c, nvtx);
        }
        writefln("sg.abst.opidx %.17g %.17g", (*g[0,0,0]).x, (*g[0,0,0]).y);
        // get_boundary_grid returns a Grid handle; print the six faces.
        Grid v3 = new StructuredGrid(3, 4, 2, "bnd");
        foreach (j; 0 .. 4) {
            foreach (i; 0 .. 3) {
                (*v3[i,j,0]).set(to!double(i)*1.0, to!double(j)*2.0, to!double(i+j)*0.5);
            }
        }
        dump_grid("sg.abst.src3d", cast(StructuredGrid)v3);
        foreach (b; 0 .. 6) {
            auto bg = v3.get_boundary_grid(b);
            dump_grid_base(format("sg.abst.bnd b=%d", b), bg);
            dump_ids(format("sg.abst.blc b=%d", b), v3.get_list_of_boundary_cells(b));
        }
        tryit("sg.abst.bnd.2d", () { g.get_boundary_grid(0); });
        // 2D boundary cell lists (faces 0..3 only; Face.bottom/top is out of range).
        Grid q2 = mk2d(4, 3, "bnd2d");
        foreach (b; 0 .. 4) {
            dump_ids(format("sg.abst.blc2d b=%d", b), q2.get_list_of_boundary_cells(b));
        }
        tryit("sg.abst.blc2d.k", () { q2.get_list_of_boundary_cells(4); });
    }

    //================ sg.str.* — toString (D:223-232)
    {
        auto g = surf_grid2d("str");
        writefln("sg.str.2d=%s", to!string(g));
        auto g3 = vol_grid3d("str3d");
        writefln("sg.str.3d=%s", to!string(g3));
        auto g1 = new StructuredGrid(3, 1, 1, "str1d");
        writefln("sg.str.1d=%s", to!string(g1));
        auto gq = mk2d(3, 3, "q");
        gq.set_tag(Face.west, "W");
        gq.set_tag(99, "nope");
        writefln("sg.str.tags=%s", to!string(gq));
        // A NaN vertex prints as Vector3(nan, nan, nan) inside the repr.
        writefln("sg.str.nan=%s", to!string(new StructuredGrid(2, 2, 1, "nan")));
    }

    //================ sg.tag.* — set_tag / set_tags (D:174-204)
    {
        auto g = mk2d(4, 3, "tag");
        writefln("sg.tag.0=%s", g.set_tag(0, "west-tag"));
        writefln("sg.tag.inrange=%s", g.set_tag(5, "out"));
        writefln("sg.tag.bcs=%s", g.bcTags);
        writefln("sg.tag.n=%d", g.set_tags(["west": "W", "east": "E"]));
        writefln("sg.tag.bcs2=%s", g.bcTags);
        writefln("sg.tag.n2=%d", g.set_tags(["south": "S", "north": "N", "bottom": "B",
                                             "top": "T"]));
        writefln("sg.tag.bcs3=%s", g.bcTags);
        auto g3 = new StructuredGrid(2, 2, 2, "tag3");
        writefln("sg.tag.3d.n=%d", g3.set_tags(["west": "W", "east": "E", "south": "S",
                                                 "north": "N", "bottom": "B", "top": "T"]));
        writefln("sg.tag.3d.bcs=%s", g3.bcTags);
        auto g1 = new StructuredGrid(3, 1, 1, "tag1");
        writefln("sg.tag.1d.n=%d", g1.set_tags(["west": "W", "east": "E", "south": "S",
                                                 "north": "N", "bottom": "B", "top": "T"]));
        writefln("sg.tag.1d.bcs=%s", g1.bcTags);
        string[string] notags;
        writefln("sg.tag.empty.n=%d", g1.set_tags(notags));
        writefln("sg.tag.unknown.n=%d", g1.set_tags(["nonsense": "X"]));
    }

    //================ sg.dup.* — copy constructor and dup (D:162-172)
    {
        auto g = surf_grid2d("dupsrc");
        g.set_tag(Face.top, "T");
        auto c = new StructuredGrid(g);
        writefln("sg.dup.copy=%s", to!string(c));
        writefln("sg.dup.aliased=%s", (c is g));
        (*c[0,0]).x = 42.0;
        writefln("sg.dup.after.write src=%.17g copy=%.17g", (*g[0,0]).x, (*c[0,0]).x);
        auto d = g.dup();
        writefln("sg.dup.dup=%s", to!string(d));
        writefln("sg.dup.dup.aliased=%s", (d is g));
        auto c3 = new StructuredGrid(vol_grid3d("dup3d"));
        dump_grid("sg.dup.3d", c3);
    }

    //================ sg.sub.* — subgrid (D:376-420)
    {
        auto g = new StructuredGrid(4, 4, 1, "sub");
        foreach (j; 0 .. 4) {
            foreach (i; 0 .. 4) {
                (*g[i,j]).set(to!double(i)*2.0, to!double(j)*3.0, to!double(i)*to!double(j));
            }
        }
        dump_grid("sg.sub.a", g.subgrid(1, 3, 1, 2));
        dump_grid("sg.sub.b", g.subgrid(0, 2, 0, 2));
        dump_grid("sg.sub.c", g.subgrid(2, 2, 2, 2));
        dump_grid("sg.sub.d", g.subgrid(0, 4, 0, 4));
        auto g3 = new StructuredGrid(3, 3, 3, "sub3");
        foreach (k; 0 .. 3) {
            foreach (j; 0 .. 3) {
                foreach (i; 0 .. 3) {
                    (*g3[i,j,k]).set(to!double(i), to!double(j), to!double(k));
                }
            }
        }
        dump_grid("sg.sub.3d", g3.subgrid(1, 2, 1, 2, 1, 2));
        auto g1 = new StructuredGrid(5, 1, 1, "sub1");
        dump_grid("sg.sub.1d", g1.subgrid(1, 3));
        tryit("sg.sub.1d.ni1", () { g1.subgrid(0, 1); });
        tryit("sg.sub.1d.nj2", () { g1.subgrid(0, 2, 0, 2); });
        tryit("sg.sub.1d.nk2", () { g1.subgrid(0, 2, 0, 1, 0, 2); });
        tryit("sg.sub.2d.nj1", () { g.subgrid(0, 2, 0, 1); });
        tryit("sg.sub.2d.nk2", () { g.subgrid(0, 2, 0, 2, 0, 2); });
        tryit("sg.sub.2d.ovr.i", () { g.subgrid(2, 4, 0, 2); });
        tryit("sg.sub.2d.ovr.j", () { g.subgrid(0, 2, 2, 4); });
        tryit("sg.sub.3d.nk1", () { g3.subgrid(0, 2, 0, 2, 0, 1); });
        tryit("sg.sub.3d.ovr.k", () { g3.subgrid(0, 2, 0, 2, 1, 3); });
    }

    //================ sg.bnd.* / sg.blc.* — boundary extraction with distinct sizes
    {
        // niv, njv, nkv all different so any mix-up in D:427-446 is visible.
        auto g = new StructuredGrid(3, 4, 5, "bd");
        foreach (k; 0 .. 5) {
            foreach (j; 0 .. 4) {
                foreach (i; 0 .. 3) {
                    (*g[i,j,k]).set(to!double(i)*1.0 + 100.0*to!double(k) +
                                    10.0*to!double(j), to!double(j), to!double(k));
                }
            }
        }
        auto sg = cast(StructuredGrid)g;
        foreach (b; 0 .. 6) {
            dump_grid_base(format("sg.bnd b=%d", b), sg.get_boundary_grid(b));
            dump_ids(format("sg.blc b=%d n=%d", b, sg.get_list_of_boundary_cells(b).length),
                     sg.get_list_of_boundary_cells(b));
        }
    }

    //================ sg.path.* — Path constructor and make_grid_from_path (D:73, :580)
    {
        Path pth = new Line(Vector3(0.0, 0.1, 0.2), Vector3(1.0, 0.3, 0.4));
        auto g = new StructuredGrid(pth, 6, new LinearFunction(0.0, 1.0), "p1d");
        dump_grid("sg.path.linear", g);
        auto g2 = new StructuredGrid(pth, 6, new QuadraticFunction(0.3, false), "p1dc");
        dump_grid("sg.path.quad", g2);
        auto g3 = new StructuredGrid(pth, 6);  // clusterf and label defaults (D:74-75)
        dump_grid("sg.path.default", g3);
        // A 2D lattice built by driving make_grid_from_path twice is not possible here;
        // the Path constructor always delegates to this(niv,1,1,label) (D:77).
        writefln("sg.path.dims=%d ncells=%d", g.dimensions, g.ncells);
        tryit("sg.path.niv1", () { new StructuredGrid(pth, 1); });
    }

    //================ sg.surf.* — ParametricSurface constructor (D:82-101)
    {
        auto p00 = Vector3(0.0, 0.1);
        auto p10 = Vector3(1.0, 0.1);
        auto p11 = Vector3(1.0, 1.1);
        auto p01 = Vector3(0.0, 1.1);
        ParametricSurface patch = new CoonsPatch(p00, p10, p11, p01);
        UnivariateFunction[] cf4 = [new LinearFunction(0.0, 1.0),
                                    new RobertsFunction(true, true, 1.2),
                                    new LinearFunction(0.0, 1.0),
                                    new QuadraticFunction(0.7, true)];
        auto gs = new StructuredGrid(patch, 5, 4, cf4, "simple", "sfs");
        dump_grid("sg.surf.simple", gs);
        writefln("sg.surf.simple.bad=%.17g", gs.measure_of_badness());
        auto gl = new StructuredGrid(patch, 5, 4, cf4, "linear", "sfl");
        dump_grid("sg.surf.bilinear.by.linear", gl);
        auto gb = new StructuredGrid(patch, 5, 4, cf4, "bilinear", "sfb");
        dump_grid("sg.surf.bilinear", gb);
        // clusterf shorter than 4 is padded with LinearFunction (D:89).
        UnivariateFunction[] cf2 = [new QuadraticFunction(0.3, false)];
        auto gp = new StructuredGrid(patch, 4, 3, cf2, "simple", "sfpad");
        dump_grid("sg.surf.pad", gp);
        writefln("sg.surf.pad.bad=%.17g", gp.measure_of_badness());
        tryit("sg.surf.unknown", () { new StructuredGrid(patch, 4, 3, cf2, "cubic", "x"); });
        tryit("sg.surf.empty", () { new StructuredGrid(patch, 4, 3, cf2, "", "x"); });
        // The 12-argument padding for the volume constructor (D:123) is in sg.vol.*.
        writefln("sg.surf.tagcount=%d", gs.set_tags(["north": "N"]));
    }

    //================ sg.rgrid.* — redistributed parameters (D:104-113, :648-688)
    {
        auto p00 = Vector3(0.0, 0.1);
        auto p10 = Vector3(1.0, 0.1);
        auto p11 = Vector3(1.0, 1.1);
        auto p01 = Vector3(0.0, 1.1);
        ParametricSurface patch = new CoonsPatch(p00, p10, p11, p01);
        UnivariateFunction[] cf = [new LinearFunction(0.0, 1.0), new LinearFunction(0.0, 1.0),
                                   new LinearFunction(0.0, 1.0), new LinearFunction(0.0, 1.0)];
        double[4][4] rg, sgr;
        rs_grids_to_list_from_data("sg.rgrid.identity", rg, sgr);
        auto g0 = new StructuredGrid(patch, 5, 4, cf, rg, sgr, "rgid");
        dump_grid("sg.rgrid.ident", g0);
        // A non-trivial redistribution: the 16 free values of D:1479-1494.
        double[] data;
        data.length = 16;
        foreach (i; 0 .. 16) { data[i] = 0.05 * to!double(i); }
        list_to_rs_grids(data, rg, sgr);
        dump_g2d("sg.rgrid.r", rg);
        dump_g2d("sg.rgrid.s", sgr);
        auto g1 = new StructuredGrid(patch, 5, 4, cf, rg, sgr, "rg");
        dump_grid("sg.rgrid.grid", g1);
        writefln("sg.rgrid.bad=%.17g", g1.measure_of_badness());
        double[] back;
        back.length = 16;
        rs_grids_to_list(rg, sgr, back);
        dump_doubles("sg.rgrid.roundtrip", back);
    }

    //================ sg.vol.* — ParametricVolume constructor (D:116-136)
    {
        ParametricVolume vol = new ProbeVolume(Vector3(0.1, 0.2, 0.3), Vector3(1.0, 0.0, 0.0),
                                              Vector3(0.0, 1.0, 0.0), Vector3(0.0, 0.0, 1.0));
        UnivariateFunction[] cf;
        auto gs = new StructuredGrid(vol, 3, 3, 2, cf, "simple", "volS");
        dump_grid("sg.vol.simple", gs);
        auto gt = new StructuredGrid(vol, 3, 3, 2, cf, "trilinear", "volT");
        dump_grid("sg.vol.trilinear", gt);
        auto gl = new StructuredGrid(vol, 3, 3, 2, cf, "linear", "volL");
        dump_grid("sg.vol.linear", gl);
        auto gtfi = new StructuredGrid(vol, 3, 3, 2, cf, "tfi", "volTFI");
        dump_grid("sg.vol.tfi", gtfi);
        UnivariateFunction[] cf2 = [new LinearFunction(0.0, 1.0),
                                    new QuadraticFunction(0.4, false)];
        auto gc = new StructuredGrid(vol, 3, 4, 2, cf2, "simple", "volC");
        dump_grid("sg.vol.cluster", gc);
        tryit("sg.vol.unknown", () { new StructuredGrid(vol, 2, 2, 2, cf, "quad", "x"); });
        tryit("sg.vol.n1", () { new StructuredGrid(vol, 1, 1, 1, cf, "simple", "x"); });
        writefln("sg.vol.dims=%d nkv=%d", gs.dimensions, gs.nkv);
    }

    //================ sg.readtxt.* — read_from_text_file (D:783-852)
    {
        string plain = TD ~ "/plain.txt";
        // Build the whole file in one go.
        {
            string body = "4 3 2\n";
            foreach (k; 0 .. 2) foreach (j; 0 .. 3) foreach (i; 0 .. 4) {
                body ~= format("%.17g %.17g %.17g\n", to!double(i) + 0.5, to!double(j)*2.0,
                               to!double(k)*4.0);
            }
            mk(plain, body);
        }
        {
            auto g = new StructuredGrid(plain, "text", "plain");
            dump_grid("sg.readtxt.plain", g);
        }
        string vtk = TD ~ "/head.vtk";
        {
            string body = "# vtk DataFile Version 2.0\nmy label here\nASCII\n" ~
                "DATASET STRUCTURED_GRID\nDIMENSIONS 5 4 1\nPOINTS 20 float\n";
            foreach (j; 0 .. 4) foreach (i; 0 .. 5) {
                body ~= format("%.17g %.17g %.17g\n", to!double(i)*0.25, to!double(j)*0.5, 0.0);
            }
            mk(vtk, body);
            auto g = new StructuredGrid(vtk, "vtk", "vtklabel");
            dump_grid("sg.readtxt.vtk", g);
            writefln("sg.readtxt.vtk.label=%s", g.label);
            auto gh = new StructuredGrid(5, 4, 1, "nohdr");
            gh.read_from_text_file(vtk, true);
            dump_grid("sg.readtxt.vtk.over", gh);
        }
        // dimension inference (D:805-819): 1D, degenerate, and nkv>1 with njv==1.
        foreach (spec; [["3", "1", "1"], ["1", "1", "1"], ["1", "4", "2"], ["4", "1", "2"]]) {
            string p = TD ~ "/dim_" ~ spec[0] ~ "_" ~ spec[1] ~ "_" ~ spec[2] ~ ".txt";
            string body = spec[0] ~ " " ~ spec[1] ~ " " ~ spec[2] ~ "\n";
            auto nv = to!int(spec[0]) * to!int(spec[1]) * to!int(spec[2]);
            foreach (n; 0 .. nv) { body ~= format("%d %d %d\n", n, n+1, n+2); }
            mk(p, body);
            tryit(format("sg.readtxt.dim %s %s %s", spec[0], spec[1], spec[2]), () {
                auto g = new StructuredGrid(p, "text", "d");
                writefln("sg.readtxt.dim.done dims=%d ncells=%d nvertices=%d", g.dimensions,
                         g.ncells, g.nvertices);
            });
        }
        // Bad tokens: two tokens only, and a non-numeric token.
        string bad1 = TD ~ "/bad1.txt";
        mk(bad1, "2 1 1\n1.0 2.0\n3.0 4.0\n");
        tryit_cut("sg.readtxt.bad.tokens", () { new StructuredGrid(bad1, "text", "b"); });
        string bad2 = TD ~ "/bad2.txt";
        mk(bad2, "2 1 1\n1.0 xx 3.0\n3.0 4.0 5.0\n");
        tryit_cut("sg.readtxt.bad.conv", () { new StructuredGrid(bad2, "text", "b"); });
        // Missing file: the measured Phobos open-failure text.
        tryit("sg.readtxt.missing", () { new StructuredGrid(TD ~ "/nope.txt", "text", "m"); });
        // Unknown format and the not-implemented VTK XML arm (D:142-159).
        tryit("sg.file.vtkxml", () { new StructuredGrid(vtk, "vtkxml", "x"); });
        tryit("sg.file.unknown", () { new StructuredGrid(vtk, "su2", "x"); });
        tryit("sg.file.empty", () { new StructuredGrid(vtk, "", "x"); });
        // A file that ends early (D's readln past EOF gives null).
        string short_file = TD ~ "/short.txt";
        mk(short_file, "3 3 1\n1.0 1.0 1.0\n");
        tryit("sg.readtxt.eof", () { new StructuredGrid(short_file, "text", "s"); });
    }

    //================ sg.wgz.* / sg.readgz.* — gzip round trip (D:854-922, :999-1040)
    {
        auto g = surf_grid2d("gzlabel");
        g.set_tag(Face.west, "west-tag");
        g.set_tag(Face.top, "top-tag");
        string gz = TD ~ "/grid.gz";
        g.write_to_gzip_file(gz);
        writefln("sg.wgz.len=%d", len_of(gz));
        writefln("sg.wgz.body=%s", gunzip_body_of(gz));
        {
            auto r = new StructuredGrid(gz, "gziptext", "ignored");
            dump_grid("sg.readgz.roundtrip", r);
            writefln("sg.readgz.repr=%s", to!string(r));
        }
        // scale on read (D:854 default 1.0; the reader multiplies each coordinate).
        {
            auto r = new StructuredGrid(1, 1, 1, "s");
            r.read_from_gzip_file(gz, 0.001);
            dump_grid("sg.readgz.scale", r);
        }
        // A gzip file with an unsupported version line.
        string gzbad = TD ~ "/badver.gz";
        {
            auto o = new GzipOut(gzbad);
            o.compress("structured_grid 2.0\nlabel: x\ndimensions: 2\nniv: 2\nnjv: 2\nnkv: 1\n");
            o.compress(" 0 0 0\n 1 0 0\n 0 1 0\n 1 1 0\n");
            o.finish();
        }
        tryit("sg.readgz.badver", () { new StructuredGrid(gzbad, "gziptext", "b"); });
        tryit("sg.readgz.badver.direct", () {
            auto r = new StructuredGrid(2, 2, 1, "b");
            r.read_from_gzip_file(gzbad);
        });
        // A gzip file with no tag section at all (version 1.0).
        string gz10 = TD ~ "/v10.gz";
        {
            auto o = new GzipOut(gz10);
            o.compress("structured_grid 1.0\nlabel: ten\ndimensions: 2\nniv: 2\nnjv: 2\nnkv: 1\n");
            o.compress(" 0 0 0\n 0.5 0 0\n 0 0.25 0\n 0.5 0.25 0\n");
            o.finish();
        }
        {
            auto r = new StructuredGrid(gz10, "gziptext", "ten");
            dump_grid("sg.readgz.v10", r);
        }
        tryit("sg.readgz.missing", () {
            auto r = new StructuredGrid(2, 2, 1, "m");
            r.read_from_gzip_file(TD ~ "/nope.gz");
        });
        // Truncated file: fewer lines than the header promises.
        string gztr = TD ~ "/trunc.gz";
        {
            auto o = new GzipOut(gztr);
            o.compress("structured_grid 1.1\nlabel: t\ndimensions: 2\nniv: 3\nnjv: 3\nnkv: 1\n");
            o.compress(" 0 0 0\n 1 0 0\n");
            o.compress("ntags: 4\ntag[0]: a\ntag[1]: b\ntag[2]: c\ntag[3]: d\n");
            o.finish();
        }
        tryit("sg.readgz.trunc", () {
            auto r = new StructuredGrid(1, 1, 1, "t");
            r.read_from_gzip_file(gztr);
        });
        // A grid with an empty bcTags list writes ntags: 0 (D:1029).
        {
            auto e = new StructuredGrid(2, 2, 1, "notags");
            e.bcTags = [];
            string gz2 = TD ~ "/notags.gz";
            e.write_to_gzip_file(gz2);
            writefln("sg.wgz.notags=%s", gunzip_body_of(gz2));
            auto r = new StructuredGrid(1, 1, 1, "x");
            r.read_from_gzip_file(gz2);
            writefln("sg.readgz.notags bcTags=%s ntagsread=true", r.bcTags);
        }
        // The dispatch of Grid.write (grid.d:104-111) with fmt "gziptext".
        {
            Grid h = surf_grid2d("via.write");
            string gz3 = TD ~ "/via.write.gz";
            h.write(gz3, "gziptext");
            writefln("sg.wgz.viafmt=%s", gunzip_body_of(gz3));
            h.write(gz3, "unknown-format");
            writefln("sg.wgz.viaunknown=%s", gunzip_body_of(gz3));
        }
    }

    //================ sg.frmt.* — the formattedRead patterns of read_from_gzip_file
    // (D:861-914). Each row is one crafted .gz whose text departs from the directive in
    // a different way, so the parse-failure wording is measured rather than invented:
    // literal mismatch, %s on an empty remainder, %d on non-digits, %d on a value out of
    // range for the target type, and %g with too few numbers on the vertex line.
    {
        void probe_case(string name, string[] lines) {
            string path = TD ~ "/frmt_" ~ name ~ ".gz";
            auto o = new GzipOut(path);
            string body;
            foreach (l; lines) { body ~= l ~ "\n"; }
            o.compress(body);
            o.finish();
            tryit("sg.frmt." ~ name, () {
                auto r = new StructuredGrid(1, 1, 1, "x");
                r.read_from_gzip_file(path);
                writefln("sg.frmt.%s.after dims=%d niv=%d njv=%d nkv=%d label=%s bcTags=%s",
                         name, r.dimensions, r.niv, r.njv, r.nkv, r.label, r.bcTags);
            });
        }
        probe_case("badfirst", ["not_a_grid 1.1", "label: f", "dimensions: 2", "niv: 2",
                                "njv: 2", "nkv: 1", " 0 0 0", " 1 0 0", " 0 1 0", " 1 1 0"]);
        probe_case("emptyver", ["structured_grid", "label: f", "dimensions: 2", "niv: 2",
                               "njv: 2", "nkv: 1", " 0 0 0", " 1 0 0", " 0 1 0", " 1 1 0"]);
        probe_case("twospace", ["structured_grid  1.1", "label: f", "dimensions: 2", "niv: 2",
                                "njv: 2", "nkv: 1", " 0 0 0", " 1 0 0", " 0 1 0", " 1 1 0"]);
        probe_case("nolabel", ["structured_grid 1.1", "dimensions: 2", "niv: 2", "njv: 2",
                               "nkv: 1", " 0 0 0", " 1 0 0", " 0 1 0", " 1 1 0"]);
        probe_case("dimsword", ["structured_grid 1.1", "label: f", "dimensions: two",
                                "niv: 2", "njv: 2", "nkv: 1", " 0 0 0", " 1 0 0", " 0 1 0",
                                " 1 1 0"]);
        probe_case("dimsfloat", ["structured_grid 1.1", "label: f", "dimensions: 2.5",
                                 "niv: 2", "njv: 2", "nkv: 1", " 0 0 0", " 1 0 0", " 0 1 0",
                                 " 1 1 0"]);
        probe_case("dimsneg", ["structured_grid 1.1", "label: f", "dimensions: -2", "niv: 2",
                               "njv: 2", "nkv: 1", " 0 0 0", " 1 0 0", " 0 1 0", " 1 1 0"]);
        probe_case("nivbig", ["structured_grid 1.1", "label: f", "dimensions: 2",
                              "niv: 99999999999999", "njv: 2", "nkv: 1", " 0 0 0", " 1 0 0",
                              " 0 1 0", " 1 1 0"]);
        probe_case("shortvtx", ["structured_grid 1.1", "label: f", "dimensions: 2", "niv: 2",
                                "njv: 2", "nkv: 1", " 0 0 0", " 1 0"]);
        probe_case("wordvtx", ["structured_grid 1.1", "label: f", "dimensions: 2", "niv: 2",
                               "njv: 2", "nkv: 1", " 0 0 0", " a b c", " 0 1 0", " 1 1 0"]);
        probe_case("nospaces", ["structured_grid 1.1", "label: f", "dimensions: 2", "niv: 2",
                                "njv: 2", "nkv: 1", "0 0 01 0 00 1 01 1 0"]);
        probe_case("extraspace", ["structured_grid 1.1", "label: f", "dimensions: 2", "niv: 2",
                                  "njv: 2", "nkv: 1", "   0   0   0  ", " 1 0 0", " 0 1 0",
                                  " 1 1 0"]);
        probe_case("extrainline", ["structured_grid 1.1", "label: f", "dimensions: 2", "niv: 2",
                                   "njv: 2", "nkv: 1", " 0 0 0", " 1 0 0", " 0 1 0", " 1 1 0",
                                   "ntags: 2", "tag[0]: A", "tag[1]: B", "extra line"]);
        probe_case("notagsline", ["structured_grid 1.1", "label: f", "dimensions: 2", "niv: 2",
                                  "njv: 2", "nkv: 1", " 0 0 0", " 1 0 0", " 0 1 0", " 1 1 0",
                                  "ntags: 3", "tag[0]: A"]);
        probe_case("tagcountbad", ["structured_grid 1.1", "label: f", "dimensions: 2", "niv: 2",
                                   "njv: 2", "nkv: 1", " 0 0 0", " 1 0 0", " 0 1 0", " 1 1 0",
                                   "tags: 4"]);
        probe_case("emptylabel", ["structured_grid 1.1", "label:", "dimensions: 2", "niv: 2",
                                  "njv: 2", "nkv: 1", " 0 0 0", " 1 0 0", " 0 1 0", " 1 1 0"]);
        probe_case("version10x", ["structured_grid 1.01", "label: f", "dimensions: 2", "niv: 2",
                                  "njv: 2", "nkv: 1", " 0 0 0", " 1 0 0", " 0 1 0", " 1 1 0"]);
    }

    //================ sg.wraw.* / sg.readraw.* — raw binary (D:924-997, :1042-1082)
    {
        auto g = surf_grid2d("rawlabel");
        g.set_tag(Face.east, "E-tag");
        string rb = TD ~ "/grid.raw";
        g.write_to_raw_binary_file(rb);
        writefln("sg.wraw.len=%d", len_of(rb));
        writefln("sg.wraw.hex=%s", hex_of(rb));
        {
            auto r = new StructuredGrid(rb, "rawbinary", "ignored");
            dump_grid("sg.readraw.roundtrip", r);
        }
        {
            auto r = new StructuredGrid(1, 1, 1, "s");
            r.read_from_raw_binary_file(rb, 2.0);
            dump_grid("sg.readraw.scale", r);
        }
        // Empty label (D:940 `if (label_length > 0)`).
        {
            auto e = new StructuredGrid(2, 2, 1, "");
            string rb2 = TD ~ "/nolog.raw";
            e.write_to_raw_binary_file(rb2);
            writefln("sg.wraw.nolabel.hex=%s", hex_of(rb2));
            auto r = new StructuredGrid(1, 1, 1, "x");
            r.read_from_raw_binary_file(rb2);
            writefln("sg.readraw.nolabel label=%s bcTags=%s", r.label, r.bcTags);
        }
        // Bad version in the header, and a missing file.
        string badraw = TD ~ "/badver.raw";
        {
            auto f = File(badraw, "wb");
            f.rawWrite(cast(char[])"structured_grid 7.7");
            f.rawWrite(cast(int[1])[0]);
            f.close();
        }
        tryit("sg.readraw.badver", () {
            auto r = new StructuredGrid(1, 1, 1, "b");
            r.read_from_raw_binary_file(badraw);
        });
        tryit("sg.readraw.missing", () {
            auto r = new StructuredGrid(1, 1, 1, "m");
            r.read_from_raw_binary_file(TD ~ "/nope.raw");
        });
        tryit("sg.file.rawviafmt", () { new StructuredGrid(badraw, "rawbinary", "b"); });
        // Truncated: header only, then the readers run out of data.
        string trh = TD ~ "/trunchead.raw";
        {
            auto f = File(trh, "wb");
            f.rawWrite(cast(char[])"structured_grid 1.1");
            f.close();
        }
        tryit("sg.readraw.trunc", () {
            auto r = new StructuredGrid(1, 1, 1, "t");
            r.read_from_raw_binary_file(trh);
        });
        // Grid.write with "rawbinary" goes to the raw writer (grid.d:106).
        {
            Grid h = surf_grid2d("rw");
            string rb3 = TD ~ "/via.write.raw";
            h.write(rb3, "rawbinary");
            writefln("sg.wraw.viafmt.hex=%s", hex_of(rb3));
        }
    }

    //================ sg.vtk.* — write_to_vtk_file (D:1084-1102)
    {
        auto g = surf_grid2d("vtklabel");
        string v = TD ~ "/grid.vtk";
        g.write_to_vtk_file(v);
        writefln("sg.vtk.len=%d body=%s", len_of(v), body_of(v));
        Grid h = new StructuredGrid(3, 2, 1, "vtk3d");
        foreach (j; 0 .. 2) foreach (i; 0 .. 3) {
            (*h[i,j,0]).set(to!double(i)/3.0, to!double(j), 1.0e-40);
        }
        string v2 = TD ~ "/grid2.vtk";
        h.write_to_vtk_file(v2);
        writefln("sg.vtk.len2=%d body2=%s", len_of(v2), body_of(v2));
    }

    //================ sg.su2.* — write_to_su2_file (D:1312-1318), via UnstructuredGrid
    {
        auto g = surf_grid2d("su2label");
        string s = TD ~ "/grid.su2";
        g.write_to_su2_file(s);
        writefln("sg.su2.len=%d body=%s", len_of(s), body_of(s));
        g.write_to_su2_file(s, 0.001);
        writefln("sg.su2.scaled.len=%d body=%s", len_of(s), body_of(s));
        auto g3 = vol_grid3d("su23d");
        string s3 = TD ~ "/grid3.su2";
        g3.write_to_su2_file(s3);
        writefln("sg.su2.3d.len=%d body=%s", len_of(s3), body_of(s3));
    }

    //================ sg.join.* — joinGrid (D:1130-1256)
    {
        auto a = mk2d(4, 3, "A");
        foreach (j; 0 .. 3) foreach (i; 0 .. 4) {
            (*a[i,j]).set(to!double(i)*1.0, to!double(j)*1.0, 0.0);
        }
        auto b = mk2d(3, 3, "B");
        foreach (j; 0 .. 3) foreach (i; 0 .. 3) {
            (*b[i,j]).set(to!double(i+3)*1.0, to!double(j)*1.0, 0.0);
        }
        auto aj = a.dup();
        aj.joinGrid(b, "east");
        dump_grid("sg.join.east", aj);
        auto bj = a.dup();
        bj.joinGrid(b, "imax");
        dump_grid("sg.join.imax", bj);
        writefln("sg.join.same=%s", (to!string(aj) == to!string(bj)));
        // north join
        auto c = mk2d(4, 3, "C");
        foreach (j; 0 .. 3) foreach (i; 0 .. 4) {
            (*c[i,j]).set(to!double(i)*1.0, to!double(j)*1.0 + 2.0, 0.0);
        }
        auto nj = a.dup();
        nj.joinGrid(c, "north");
        dump_grid("sg.join.north", nj);
        auto nj2 = a.dup();
        nj2.joinGrid(c, "jmax");
        dump_grid("sg.join.jmax", nj2);
        // guards
        tryit("sg.join.badloc", () { a.joinGrid(b, "west"); });
        tryit("sg.join.emptyloc", () { a.joinGrid(b, ""); });
        Grid g3 = vol_grid3d("3d");
        tryit("sg.join.3d", () { (cast(StructuredGrid)g3).joinGrid(b, "east"); });
        auto mismatch = mk2d(3, 2, "M");
        foreach (j; 0 .. 2) foreach (i; 0 .. 3) {
            (*mismatch[i,j]).set(to!double(i+3)*1.0, to!double(j)*1.0, 0.0);
        }
        tryit("sg.join.njv", () { a.joinGrid(mismatch, "east"); });
        auto nkvbad = mk2d(3, 3, "K");
        writefln("sg.join.nkv %d %d", a.nkv, nkvbad.nkv);
        // non-coincident vertex
        auto apart = mk2d(3, 3, "AP");
        foreach (j; 0 .. 3) foreach (i; 0 .. 3) {
            (*apart[i,j]).set(to!double(i)*1.0 + 1.0e-3, to!double(j)*1.0, 0.0);
        }
        tryit("sg.join.coincide", () { a.joinGrid(apart, "east"); });
        auto nbad = mk2d(5, 3, "NB");
        foreach (j; 0 .. 3) foreach (i; 0 .. 5) {
            (*nbad[i,j]).set(to!double(i)*1.0, to!double(j)*1.0, 0.0);
        }
        tryit("sg.join.niv", () { a.joinGrid(nbad, "north"); });
        auto nbad2 = mk2d(4, 3, "NB2");
        foreach (j; 0 .. 3) foreach (i; 0 .. 4) {
            (*nbad2[i,j]).set(to!double(i)*1.0, to!double(j)*1.0 + 1.0e-2, 0.0);
        }
        tryit("sg.join.coincide.north", () { a.joinGrid(nbad2, "north"); });
    }

    //================ sg.slab.* / sg.wedge.* (D:1258-1310)
    {
        auto g = mk2d(3, 2, "slabsrc");
        foreach (j; 0 .. 2) foreach (i; 0 .. 3) {
            (*g[i,j]).set(to!double(i)*1.0, to!double(j)*1.0, 0.5);
        }
        dump_grid("sg.slab.sym", g.makeSlabGrid(Vector3(0.0, 0.0, 2.0), true, ""));
        dump_grid("sg.slab.asym", g.makeSlabGrid(Vector3(0.0, 0.0, 2.0), false, ""));
        dump_grid("sg.slab.dbl", g.makeSlabGrid(3.0, true, "dbl"));
        dump_grid("sg.slab.dblasym", g.makeSlabGrid(3.0, false, "dblasym"));
        writefln("sg.slab.srclabel=%s", g.makeSlabGrid(1.0).label);
        // makeWedgeGrid around the x-axis
        auto w = mk2d(3, 2, "wedgesrc");
        foreach (j; 0 .. 2) foreach (i; 0 .. 3) {
            (*w[i,j]).set(to!double(i)*0.5, 1.0 + to!double(j)*0.25, 0.1*to!double(i));
        }
        dump_grid("sg.wedge.sym", w.makeWedgeGrid(0.3, true, "", 3));
        dump_grid("sg.wedge.asym", w.makeWedgeGrid(0.3, false, "", 3));
        dump_grid("sg.wedge.sym2", w.makeWedgeGrid(1.0, true));
        dump_grid("sg.wedge.nkv2", w.makeWedgeGrid(0.75, false, "w2", 2));
        writefln("sg.wedge.pi=%s", text(w.makeWedgeGrid(3.141592653589793, true, "wpi", 5)));
        tryit("sg.slab.3d", () { vol_grid3d("x").makeSlabGrid(1.0); });
        tryit("sg.wedge.3d", () { vol_grid3d("x").makeWedgeGrid(0.3); });
    }

    //================ sg.bad.* — measure_of_badness (D:1320-1394)
    {
        auto p00 = Vector3(0.0, 0.1);
        auto p10 = Vector3(1.0, 0.1);
        auto p11 = Vector3(1.0, 1.1);
        auto p01 = Vector3(0.0, 1.1);
        ParametricSurface patch = new CoonsPatch(p00, p10, p11, p01);
        UnivariateFunction[] cflin = [new LinearFunction(0.0, 1.0), new LinearFunction(0.0, 1.0),
                                     new LinearFunction(0.0, 1.0), new LinearFunction(0.0, 1.0)];
        auto gid = new StructuredGrid(patch, 5, 5, cflin, "simple", "badid");
        writefln("sg.bad.identical=%.17g", gid.measure_of_badness());
        UnivariateFunction[] cfclust = [new LinearFunction(0.0, 1.0),
                                        new RobertsFunction(true, true, 2.0),
                                        new LinearFunction(0.0, 1.0),
                                        new RobertsFunction(true, true, -2.0)];
        auto gc = new StructuredGrid(patch, 5, 5, cfclust, "simple", "badc");
        writefln("sg.bad.clustered=%.17g", gc.measure_of_badness());
        writefln("sg.bad.clustered.bilinear=%.17g",
                 new StructuredGrid(patch, 5, 5, cfclust, "bilinear", "badcb").measure_of_badness());
        // A skewed (non-orthogonal) quadrilateral mesh.
        auto sk = mk2d(4, 4, "skew");
        foreach (j; 0 .. 4) foreach (i; 0 .. 4) {
            (*sk[i,j]).set(to!double(i)*1.0 + 0.3*to!double(j), to!double(j)*1.0, 0.0);
        }
        writefln("sg.bad.skew=%.17g", sk.measure_of_badness());
        // An inverted cell: negative area triggers the penalty (D:1373).
        auto inv = mk2d(3, 2, "inv");
        (*inv[0,0]).set(0.0, 0.0, 0.0);
        (*inv[1,0]).set(1.0, 0.0, 0.0);
        (*inv[2,0]).set(2.0, 0.0, 0.0);
        (*inv[0,1]).set(0.0, 1.0, 0.0);
        (*inv[1,1]).set(1.0, -1.0, 0.0);   // crossed quad
        (*inv[2,1]).set(2.0, 1.0, 0.0);
        writefln("sg.bad.inverted=%.17g", inv.measure_of_badness());
        // Degenerate: a zero-area cell (repeated vertices).
        auto deg = mk2d(3, 3, "deg");
        foreach (j; 0 .. 3) foreach (i; 0 .. 3) {
            (*deg[i,j]).set(to!double(i)*1.0, to!double(i)*1.0, 0.0);
        }
        writefln("sg.bad.degenerate=%.17g", deg.measure_of_badness());
        // A single-row mesh (njv == 2) and a single-column one (niv == 2).
        writefln("sg.bad.2row=%.17g", surf_grid2d("b2").measure_of_badness());
        tryit("sg.bad.3d", () { vol_grid3d("b3").measure_of_badness(); });
        tryit("sg.bad.1d", () { new StructuredGrid(5, 1, 1, "b1").measure_of_badness(); });
    }

    //================ sg.rs.* — the two list/grid mappers (D:1472-1522)
    {
        double[] data;
        data.length = 16;
        foreach (i; 0 .. 16) { data[i] = 0.01 * to!double(i) + 0.02; }
        double[4][4] rg, sgr;
        list_to_rs_grids(data, rg, sgr);
        dump_g2d("sg.rs.r", rg);
        dump_g2d("sg.rs.s", sgr);
        double[] back;
        back.length = 16;
        rs_grids_to_list(rg, sgr, back);
        dump_doubles("sg.rs.roundtrip", back);
        // The assert on a short array (D:1478, :1504).
        tryit("sg.rs.short.list", () {
            double[] d; d.length = 15;
            rs_grids_to_list(rg, sgr, d);
        });
        tryit("sg.rs.short.get", () {
            double[] d; d.length = 0;
            list_to_rs_grids(d, rg, sgr);
        });
    }

    //================ sg.determ.* — determine_rs_grids (D:1396-1423)
    {
        auto p00 = Vector3(0.0, 0.1);
        auto p10 = Vector3(1.0, 0.1);
        auto p11 = Vector3(1.0, 1.1);
        auto p01 = Vector3(0.0, 1.1);
        ParametricSurface patch = new CoonsPatch(p00, p10, p11, p01);
        UnivariateFunction[] cf = [new LinearFunction(0.0, 1.0),
                                  new RobertsFunction(true, true, 1.5),
                                  new LinearFunction(0.0, 1.0),
                                  new RobertsFunction(true, true, -1.5)];
        auto g = new StructuredGrid(patch, 6, 5, cf, "simple", "det");
        writefln("sg.determ.before=%.17g", g.measure_of_badness());
        double[4][4] rg, sgr;
        // Start from the identity redistribution (D:1407 reads these values).
        double[] id;
        id.length = 16;
        id[] = 0.0;
        id[2] = 0.0; id[7] = 1.0; id[13] = 1.0;
        list_to_rs_grids(id, rg, sgr);
        dump_g2d("sg.determ.rg0", rg);
        dump_g2d("sg.determ.sg0", sgr);
        bool conv = g.determine_rs_grids(patch, cf, rg, sgr);
        writefln("sg.determ.conv=%s", conv);
        dump_g2d("sg.determ.rg", rg);
        dump_g2d("sg.determ.sg", sgr);
        writefln("sg.determ.after=%.17g", g.measure_of_badness());
        auto gr = new StructuredGrid(patch, 6, 5, cf, rg, sgr, "detr");
        dump_grid("sg.determ.grid", gr);
    }

    //================ sg.rotq.* — rotateGrid (D:1425-1465)
    {
        auto g = new StructuredGrid(2, 2, 2, "rotq");
        foreach (k; 0 .. 2) foreach (j; 0 .. 2) foreach (i; 0 .. 2) {
            (*g[i,j,k]).set(to!double(i), to!double(j), to!double(k));
        }
        auto a = g.dup();
        a.rotateGrid(1.0, 0.0, 0.0, 0.0);
        dump_verts("sg.rotq.ident", a.vertices);
        auto b = g.dup();
        b.rotateGrid(0.7071067811865476, 0.0, 0.7071067811865476, 0.0);
        dump_verts("sg.rotq.pi2.y", b.vertices);
        auto c = g.dup();
        c.rotateGrid(0.0, 0.0, 0.0, 1.0);
        dump_verts("sg.rotq.pi.y", c.vertices);
        auto d = g.dup();
        d.rotateGrid(0.5, 0.5, 0.5, 0.5);
        dump_verts("sg.rotq.diag", d.vertices);
        tryit("sg.rotq.norm", () { g.dup().rotateGrid(1.0, 1.0, 0.0, 0.0); });
        tryit("sg.rotq.zero", () { g.dup().rotateGrid(0.0, 0.0, 0.0, 0.0); });
        tryit("sg.rotq.nan", () { g.dup().rotateGrid(double.nan, 0.0, 0.0, 0.0); });
        // norm exactly at the 1e-8 band edge.
        writefln("sg.rotq.band=%.17g", (abs(1.0 + 1e-9) - 1.0) > 1e-8);
        auto e = g.dup();
        e.rotateGrid(0.9999999999999999, 0.0, 0.0, 0.0);
        dump_verts("sg.rotq.justunder", e.vertices);
    }

    //================ sg.rot.* — the two inherited Grid.rotate entries
    {
        auto g = new StructuredGrid(3, 3, 1, "rot");
        foreach (j; 0 .. 3) foreach (i; 0 .. 3) {
            (*g[i,j]).set(to!double(i), to!double(j), 0.0);
        }
        Grid h = g;
        Quaternion q = [1.0, 0.0, 0.0, 0.0];
        h.rotate(q);
        dump_verts("sg.rot.q.ident", g.vertices);
        Quaternion q180 = [0.0, 0.0, 0.0, 1.0];
        h.rotate(q180);
        dump_verts("sg.rot.q.pi", g.vertices);
        h.rotate(3.141592653589793, Vector3(0.0, 0.0, 1.0));
        dump_verts("sg.rot.a.pi", g.vertices);
        h.rotate(1.5707963267948966, Vector3(0.0, 0.0, 1.0), Vector3(1.0, 1.0, 0.0));
        dump_verts("sg.rot.a.pi2 ctr", g.vertices);
        Vector3 ax = Vector3(0.0, 2.0, 0.0);
        h.rotate(0.5, ax);
        writefln("sg.rot.axis.after %.17g %.17g %.17g", ax.x, ax.y, ax.z);
        dump_verts("sg.rot.a.len2", g.vertices);
        tryit("sg.rot.q.badnorm", () { h.rotate(cast(Quaternion)[1.0, 1.0, 0.0, 0.0]); });
        Quaternion nq;
        tryit("sg.rot.q.naninit", () { h.rotate(nq); });
        dump_verts("sg.rot.q.naninit.after", g.vertices);
    }

    //================ sg.uflowz.* (D:1534-1540)
    {
        foreach (v; [1.0, 1.0e-32, 1.0e-29, 1.0e+29, 1.0e+32,
                     -1.0, -1.0e-32, -1.0e-29, -1.0e+29, -1.0e+32,
                     0.0, -0.0, 1.0e-30, 1.0e30, 1.0e-31, 1.0e31,
                     double.max, double.nan, double.infinity,
                     -double.infinity, 9.999999999999999e+29]) {
            writefln("sg.uflowz %.17g -> %.17g", v, uflowz(v));
        }
        writefln("sg.uflowz.custom %.17g %.17g -> %.17g", 1.0e-5, 1.0e-4, uflowz(1.0e-5, 1.0e-4));
        writefln("sg.uflowz.custom2 %.17g -> %.17g", 5.0, uflowz(5.0, 1.0e-30, 4.0));
        writefln("sg.uflowz.negcustom %.17g -> %.17g", -5.0, uflowz(-5.0, 1.0e-30, 4.0));
        // sign of the zero that comes out of the underflow branch.
        writefln("sg.uflowz.sign %.17g -> %.17g signbit=%d", -1.0e-32, uflowz(-1.0e-32),
                 cast(int)signbit(uflowz(-1.0e-32)));
    }

    //================ sg.vtkhdr.* — read_VTK_header_line (D:1544-1561)
    {
        string p = TD ~ "/hdr.vtk";
        mk(p, "# vtk DataFile Version 2.0\nLabel Line\nascii\ndataset structured_grid\n" ~
             "DIMENSIONS 3 2 1\nPOINTS 6 float\n");
        {
            auto f = File(p, "r");
            writefln("sg.vtkhdr.0=%s", read_VTK_header_line("vtk", f));
            writefln("sg.vtkhdr.1=%s", read_VTK_header_line("ASCII", f));
            writefln("sg.vtkhdr.2=%s", read_VTK_header_line("STRUCTURED_GRID", f));
            writefln("sg.vtkhdr.3=%s", read_VTK_header_line("DIMENSIONS", f));
            writefln("sg.vtkhdr.4=%s", read_VTK_header_line("POINTS", f));
            tryit("sg.vtkhdr.eof", () { read_VTK_header_line("CELLS", f); });
            f.close();
        }
        // A target that appears mid-word, and one that never appears.
        {
            auto f = File(p, "r");
            writefln("sg.vtkhdr.mid=%s", read_VTK_header_line("structured", f));
            f.close();
        }
        {
            auto f = File(p, "r");
            tryit("sg.vtkhdr.absent", () { read_VTK_header_line("LOOKUP", f); });
            f.close();
        }
        tryit("sg.vtkhdr.missing", () {
            auto f = File(TD ~ "/nope.vtk", "r");
            read_VTK_header_line("vtk", f);
        });
    }

    //================ sg.gridpro.* — import_gridpro_grid (D:1565-1612)
    {
        string p = TD ~ "/gp.dat";
        {
            string body = "# a comment line\n2 2 1\n";
            body ~= "0.0 0.0 0.0\n1000.0 0.0 0.0\n0.0 1000.0 0.0\n1000.0 1000.0 1.0e-40\n";
            body ~= "2 2 1\n";
            body ~= "1.0e+40 0.0 0.0\n2.0 3.0 4.0\n5.0 6.0 7.0\n8.0 9.0 1.0e-35\n";
            mk(p, body);
        }
        auto gr = import_gridpro_grid(p);
        writefln("sg.gridpro.count=%d", gr.length);
        foreach (i, gg; gr) { dump_grid(format("sg.gridpro[%d]", i), gg); }
        auto gr2 = import_gridpro_grid(p, 0.001);
        writefln("sg.gridpro.count2=%d", gr2.length);
        foreach (i, gg; gr2) { dump_grid(format("sg.gridpro.scale[%d]", i), gg); }
        tryit("sg.gridpro.missing", () { import_gridpro_grid(TD ~ "/nope.dat"); });
        // A single block with no trailing blank line.
        string p1 = TD ~ "/gp1.dat";
        mk(p1, "2 1 1\n1.0 2.0 3.0\n4.0 5.0 6.0\n");
        auto g1 = import_gridpro_grid(p1, 1.0);
        writefln("sg.gridpro.1=%d", g1.length);
        dump_grid("sg.gridpro.1blk", g1[0]);
    }

    //================ sg.rsgp.* — rotate_gridpro_blocks (D:1756-1798)
    {
        auto g = mk2d(3, 2, "gp");
        foreach (j; 0 .. 2) foreach (i; 0 .. 3) {
            (*g[i,j]).set(to!double(i)*1.0, to!double(j)*10.0, to!double(i+j)*0.25);
        }
        auto r1 = rotate_gridpro_blocks(g, "East", "South");
        dump_grid("sg.rsgp.east.south", r1);
        writefln("sg.rsgp.east.south.aliased=%s", (r1 is g));
        auto r2 = rotate_gridpro_blocks(g, "North", "East");
        dump_grid("sg.rsgp.north.east", r2);
        auto r3 = rotate_gridpro_blocks(g, "West", "North");
        dump_grid("sg.rsgp.west.north", r3);
        auto r4 = rotate_gridpro_blocks(g);
        writefln("sg.rsgp.default.aliased=%s", (r4 is g));
        auto r5 = rotate_gridpro_blocks(g, "North", "South");
        writefln("sg.rsgp.other.aliased=%s", (r5 is g));
        writefln("sg.rsgp.north.west=%s", to!string(rotate_gridpro_blocks(g, "North", "West")));
    }

    //================ sg.fswap.* — grid_faceswap (D:1800-1828)
    {
        auto g = mk2d(3, 2, "fs");
        foreach (j; 0 .. 2) foreach (i; 0 .. 3) {
            (*g[i,j]).set(to!double(i)*1.0, to!double(j)*10.0, 0.0);
        }
        dump_grid("sg.fswap.ns", grid_faceswap(g, true, false));
        dump_grid("sg.fswap.ew", grid_faceswap(g, false, true));
        dump_grid("sg.fswap.both", grid_faceswap(g, true, true));
        dump_grid("sg.fswap.none", grid_faceswap(g, false, false));
        dump_grid("sg.fswap.default", grid_faceswap(g));
        // 3D source: the readers only touch the (i,j) lattice, so z survives oddly.
        auto g3 = vol_grid3d("fs3d");
        dump_grid("sg.fswap.3d", grid_faceswap(g3, true, false));
    }

    //================ sg.plot3d.* — writeGridsAsPlot3D / importPlot3DGrid (D:1614-1754)
    {
        StructuredGrid[] gs;
        gs ~= mk2d(3, 2, "p1");
        foreach (j; 0 .. 2) foreach (i; 0 .. 3) {
            (*gs[0][i,j]).set(to!double(i)*1.25, to!double(j)*2.5, 0.0);
        }
        gs ~= new StructuredGrid(2, 2, 2, "p2");
        foreach (k; 0 .. 2) foreach (j; 0 .. 2) foreach (i; 0 .. 2) {
            (*gs[1][i,j,k]).set(to!double(i)+0.1, to!double(j)+0.2, to!double(k)+0.3);
        }
        string f2 = TD ~ "/out2.dat";
        writeGridsAsPlot3D(f2, [gs[0]], 2);
        writefln("sg.plot3d.2d.len=%d body=%s", len_of(f2), body_of(f2));
        string f3 = TD ~ "/out3.dat";
        writeGridsAsPlot3D(f3, gs, 3);
        writefln("sg.plot3d.3d.len=%d body=%s", len_of(f3), body_of(f3));
        tryit("sg.plot3d.baddim.write", () { writeGridsAsPlot3D(f3, gs, 4); });
        tryit("sg.plot3d.dim1.write", () { writeGridsAsPlot3D(f3, gs, 1); });
        // Round trip through the reader.
        auto rb2 = importPlot3DGrid(f2, 2);
        writefln("sg.plot3d.read2.count=%d", rb2.length);
        foreach (i, gg; rb2) { dump_grid(format("sg.plot3d.read2[%d]", i), gg); }
        auto rb3 = importPlot3DGrid(f3, 3);
        writefln("sg.plot3d.read3.count=%d", rb3.length);
        foreach (i, gg; rb3) { dump_grid(format("sg.plot3d.read3[%d]", i), gg); }
        auto rb3as2 = importPlot3DGrid(f3, 2);
        writefln("sg.plot3d.read3as2.count=%d", rb3as2.length);
        foreach (i, gg; rb3as2) { dump_grid(format("sg.plot3d.read3as2[%d]", i), gg); }
        tryit("sg.plot3d.baddim.read", () { importPlot3DGrid(f2, 5); });
        // A single-block file (the size line has more than one token).
        string s1 = TD ~ "/single.dat";
        {
            string body = "2 2 1\n";
            foreach (n; 0 .. 4) { body ~= format(" %.18e\n", to!double(n)*0.5); }
            foreach (n; 0 .. 4) { body ~= format(" %.18e\n", to!double(n)*0.25); }
            body ~= " 0.0 0.0 0.0 0.0\n";
            mk(s1, body);
        }
        auto rs = importPlot3DGrid(s1, 2);
        writefln("sg.plot3d.single.count=%d", rs.length);
        foreach (i, gg; rs) { dump_grid(format("sg.plot3d.single[%d]", i), gg); }
        // Values spread over several lines and in scientific format.
        string s2 = TD ~ "/messy.dat";
        mk(s2, " 2 2 \n1.0 2.0\n  3.0,4.0\n");
        {
            string body = "2 2 1\n";
            body ~= "1.0 2.0 3.0 4.0\n";
            body ~= "1.0e+0 2.0e+0 3.0e+0\n4.0e+0\n";
            mk(s2, body);
        }
        auto rm = importPlot3DGrid(s2, 2);
        dump_grid("sg.plot3d.messy", rm[0]);
        // The gzipped path, through pipeShell("gunzip -c ...") (D:1623).
        string gzf = TD ~ "/out3.dat.gz";
        {
            auto o = new GzipOut(gzf);
            o.compress(cast(string)read(f3));
            o.finish();
        }
        auto rg = importPlot3DGrid(gzf, 3, 1.0, true);
        writefln("sg.plot3d.gz.count=%d", rg.length);
        foreach (i, gg; rg) { dump_grid(format("sg.plot3d.gz[%d]", i), gg); }
        auto rgsc = importPlot3DGrid(gzf, 3, 0.001, true);
        dump_grid("sg.plot3d.gz.scale", rgsc[0]);
        tryit("sg.plot3d.gz.missing", () {
            importPlot3DGrid(TD ~ "/nope.dat.gz", 3, 1.0, true);
        });
        tryit("sg.plot3d.missing", () { importPlot3DGrid(TD ~ "/nope.dat", 3); });
        std.file.remove(gzf);
    }

    //================ sg.ccp.* — compute_cell_properties on a real lattice (grid.d:124)
    {
        Grid g3 = new StructuredGrid(2, 2, 2, "ccp3d");
        auto sg = cast(StructuredGrid)g3;
        foreach (k; 0 .. 2) foreach (j; 0 .. 2) foreach (i; 0 .. 2) {
            (*sg[i,j,k]).set(to!double(i), to!double(j), to!double(k));
        }
        Vector3 c; number vol;
        foreach (tc; [true, false]) {
            g3.compute_cell_properties(0, tc, c, vol);
            writefln("sg.ccp.hex tc=%s %.17g %.17g %.17g %.17g", tc, c.x, c.y, c.z, vol);
        }
        // Degenerate: all 8 vertices coincident -> zero volume.
        auto deg = new StructuredGrid(2, 2, 2, "ccpdeg");
        foreach (k; 0 .. 2) foreach (j; 0 .. 2) foreach (i; 0 .. 2) {
            (*deg[i,j,k]).set(1.0, 2.0, 3.0);
        }
        Grid dh = deg;
        dh.compute_cell_properties(0, true, c, vol);
        writefln("sg.ccp.degenerate %.17g %.17g %.17g %.17g", c.x, c.y, c.z, vol);
        // Inverted (mirrored) cell: negative volume.
        auto inv = new StructuredGrid(2, 2, 2, "ccpinv");
        foreach (k; 0 .. 2) foreach (j; 0 .. 2) foreach (i; 0 .. 2) {
            (*inv[i,j,k]).set(to!double(i), to!double(j), -to!double(k));
        }
        Grid ih = inv;
        ih.compute_cell_properties(0, true, c, vol);
        writefln("sg.ccp.inverted %.17g %.17g %.17g %.17g", c.x, c.y, c.z, vol);
        ih.compute_cell_properties(0, false, c, vol);
        writefln("sg.ccp.inverted.false %.17g %.17g %.17g %.17g", c.x, c.y, c.z, vol);
        // 2D lattice cells.
        Grid g2 = surf_grid2d("ccp2d");
        foreach (n; 0 .. g2.ncells) {
            g2.compute_cell_properties(n, true, c, vol);
            writefln("sg.ccp.2d n=%d %.17g %.17g %.17g %.17g", n, c.x, c.y, c.z, vol);
        }
        // 1D lattice: nvtx==2 falls to the default arm of the 2D/3D switch.
        Grid g1 = new StructuredGrid(3, 1, 1, "ccp1d");
        tryit("sg.ccp.1d", () { g1.compute_cell_properties(0, true, c, vol); });
        tryit("sg.ccp.oob", () { g3.compute_cell_properties(1, true, c, vol); });
    }

    //================ sg.pic.* / sg.bin.* / sg.fsc.* — search services
    {
        auto g = new StructuredGrid(3, 3, 2, "srch");
        foreach (k; 0 .. 2) foreach (j; 0 .. 3) foreach (i; 0 .. 3) {
            (*g[i,j,k]).set(to!double(i)*1.0, to!double(j)*2.0, to!double(k)*3.0);
        }
        Grid h = g;
        // point_is_inside_cell at inside / face / edge / vertex / outside points.
        Vector3[] probes = [Vector3(0.5, 1.0, 1.5), Vector3(1.0, 1.0, 1.5),
                           Vector3(1.5, 3.0, 1.5), Vector3(0.5, 1.0, 3.0),
                           Vector3(0.0, 0.0, 0.0), Vector3(2.0, 4.0, 3.0),
                           Vector3(2.5, 4.5, 3.5), Vector3(0.5, 1.0, 1.5)];
        foreach (i, p; probes) {
            foreach (c; 0 .. h.ncells) {
                writefln("sg.pic p=%d c=%d inside=%s", i, c, h.point_is_inside_cell(p, c));
            }
        }
        // cell_barycentre and find_nearest_cell_centre.
        foreach (c; 0 .. h.ncells) {
            Vector3 b = h.cell_barycentre(c);
            writefln("sg.cbc c=%d %.17g %.17g %.17g", c, b.x, b.y, b.z);
        }
        // Binning.
        h.sort_cells_into_bins();
        writefln("sg.bin.default nbx=%d nby=%d nbz=%d dx=%.17g dy=%.17g dz=%.17g", h.nbx,
                 h.nby, h.nbz, h.deltax, h.deltay, h.deltaz);
        writefln("sg.bin.bb0 %.17g %.17g %.17g", h.bb0.x, h.bb0.y, h.bb0.z);
        writefln("sg.bin.bb1 %.17g %.17g %.17g", h.bb1.x, h.bb1.y, h.bb1.z);
        foreach (c; 0 .. h.ncells) {
            Vector3 b = h.cell_barycentre(c);
            writefln("sg.bin.ix c=%d b=%.17g %.17g %.17g ix=%d iy=%d iz=%d", c, b.x, b.y, b.z,
                     h.get_bin_ix(b), h.get_bin_iy(b), h.get_bin_iz(b));
        }
        void dump_bins(string tag) {
            foreach (ix; 0 .. h.nbx) foreach (iy; 0 .. h.nby) foreach (iz; 0 .. h.nbz) {
                writefln("%s bin[%d][%d][%d]=%s", tag, ix, iy, iz, h.bins[ix][iy][iz]);
            }
        }
        dump_bins("sg.bin.contents");
        // Points exactly on a bin boundary and one ULP either side of it.
        double[] bx;
        foreach (ix; 1 .. h.nbx) { bx ~= h.bb0.x + ix * h.deltax; }
        foreach (v; bx) {
            foreach (name, off; ["exact", "nextup", "nextdown"]) {
                double x = (off == "nextup") ? nextProbeUp(v) :
                           ((off == "nextdown") ? nextProbeDown(v) : v);
                Vector3 p = Vector3(x, 1.0, 1.5);
                writefln("sg.bin.edge v=%.17g off=%s ix=%d iy=%d iz=%d", x, off,
                         h.get_bin_ix(p), h.get_bin_iy(p), h.get_bin_iz(p));
            }
        }
        // y and z boundaries too.
        Vector3 py = Vector3(0.5, h.bb0.y + h.deltay, 1.5);
        writefln("sg.bin.edgey iy=%d", h.get_bin_iy(py));
        Vector3 pyn = Vector3(0.5, nextProbeDown(h.bb0.y + h.deltay), 1.5);
        writefln("sg.bin.edgey.down iy=%d", h.get_bin_iy(pyn));
        Vector3 pz = Vector3(0.5, 1.0, h.bb0.z + h.deltaz);
        writefln("sg.bin.edgez iz=%d", h.get_bin_iz(pz));
        Vector3 pzn = Vector3(0.5, 1.0, nextProbeDown(h.bb0.z + h.deltaz));
        writefln("sg.bin.edgez.down iz=%d", h.get_bin_iz(pzn));
        // far outside / left of the box (the clamp rows).
        Vector3 far = Vector3(-1.0, -1.0, -1.0);
        writefln("sg.bin.far ix=%d iy=%d iz=%d", h.get_bin_ix(far), h.get_bin_iy(far),
                 h.get_bin_iz(far));
        Vector3 over = Vector3(1.0e6, 1.0e6, 1.0e6);
        writefln("sg.bin.over ix=%d iy=%d iz=%d", h.get_bin_ix(over), h.get_bin_iy(over),
                 h.get_bin_iz(over));
        Vector3 nanp = Vector3(double.nan, 1.0, 1.5);
        writefln("sg.bin.nan ix=%d iy=%d iz=%d", h.get_bin_ix(nanp), h.get_bin_iy(nanp),
                 h.get_bin_iz(nanp));
        // fast / slow / dispatcher search at a set of points
        size_t idx; bool found;
        foreach (p; probes) {
            h.find_enclosing_cell_fast(p, idx, found);
            writefln("sg.fsc.fast p=%.17g %.17g %.17g idx=%d found=%s", p.x, p.y, p.z, idx, found);
            h.find_enclosing_cell_slow(p, idx, found);
            writefln("sg.fsc.slow p=%.17g %.17g %.17g idx=%d found=%s", p.x, p.y, p.z, idx, found);
            h.find_enclosing_cell(p, idx, found);
            writefln("sg.fsc.dis p=%.17g %.17g %.17g idx=%d found=%s", p.x, p.y, p.z, idx, found);
        }
        // On-boundary cells searched at a shared face of two cells.
        Vector3 onface = Vector3(1.0, 1.0, 1.5);
        h.find_enclosing_cell_fast(onface, idx, found);
        writefln("sg.fsc.shared fast idx=%d found=%s", idx, found);
        h.find_enclosing_cell_slow(onface, idx, found);
        writefln("sg.fsc.shared slow idx=%d found=%s", idx, found);
        // A 1x1x1 bin grid (the degenerate delta floor of grid.d:301-304).
        Grid s1 = new StructuredGrid(2, 2, 1, "singlebin");
        s1.sort_cells_into_bins(1, 1, 1);
        writefln("sg.bin.one nbx=%d dx=%.17g contents=%s", s1.nbx, s1.deltax,
                 s1.bins[0][0][0]);
        tryit("sg.bin.zero", () { s1.sort_cells_into_bins(0, 1, 1); });
        // A second sort appends into the same bins (grid.d:306-310 keeps contents).
        Grid app = new StructuredGrid(2, 2, 1, "append");
        app.sort_cells_into_bins(2, 2, 1);
        writefln("sg.bin.twice a=%s b=%s", app.bins[0][0][0], app.bins[1][1][0]);
        app.sort_cells_into_bins(2, 2, 1);
        writefln("sg.bin.thrice a=%s b=%s", app.bins[0][0][0], app.bins[1][1][0]);
        // 2D: nbinz is forced to 1 (grid.d:280).
        Grid g2d = surf_grid2d("bin2d");
        g2d.sort_cells_into_bins(3, 2, 7);
        writefln("sg.bin.2d nbz=%d nby=%d contents0=%s", g2d.nbz, g2d.nby, g2d.bins[0][0][0]);
        // nearest cell centre
        double dist;
        foreach (p; probes) {
            h.find_nearest_cell_centre(p, idx, dist);
            writefln("sg.fncc p=%.17g %.17g %.17g idx=%d d=%.17g", p.x, p.y, p.z, idx, dist);
        }
    }

    //================ sg.unit.* — sgrid.d's own unittest (D:1833-1865)
    {
        auto p00 = Vector3(0.0, 0.1);
        auto p10 = Vector3(1.0, 0.1);
        auto p11 = Vector3(1.0, 1.1);
        auto p01 = Vector3(0.0, 1.1);
        auto my_patch = new CoonsPatch(p00, p10, p11, p01);
        auto cf = [new LinearFunction(), new LinearFunction(),
            new LinearFunction(), new LinearFunction()];
        auto my_grid = new StructuredGrid(my_patch, 11, 21, cf);
        my_grid.set_tag(Face.north, "north-face");
        my_grid.set_tags(["south": "south-face-here-sir", "west": "west-face-here-sir!"]);
        string repr = to!string(my_grid);
        writeln("my_grid=", repr);
        writefln("sg.unit.canFind.north=%s", repr.canFind("north-face"));
        writefln("sg.unit.canFind.south=%s", repr.canFind("south-face-here-sir"));
        writefln("sg.unit.canFind.west=%s", repr.canFind("west-face-here-sir!"));
        writefln("sg.unit.v55=%.17g %.17g %.17g", (*my_grid[5,5]).x, (*my_grid[5,5]).y,
                 (*my_grid[5,5]).z);
        writefln("sg.unit.v55.approx=%s",
                 approxEqualVectors(*my_grid[5, 5], Vector3(0.5, 0.35, 0.0)));
        auto my_subgrid = my_grid.subgrid(4, 3, 4, 5);
        writefln("sg.unit.sub=%.17g %.17g %.17g", (*my_subgrid[1,1]).x, (*my_subgrid[1,1]).y,
                 (*my_subgrid[1,1]).z);
        writefln("sg.unit.sub.approx=%s",
                 approxEqualVectors(*my_subgrid[1, 1], Vector3(0.5, 0.35, 0.0)));
        writefln("sg.unit.niv=%d njv=%d nkv=%d ncells=%d label=%s", my_grid.niv, my_grid.njv,
                 my_grid.nkv, my_grid.ncells, my_grid.label);
    }

    writeln("sg.done");
}

/// nextAfter, spelled so that the probe and the C++ replay agree on both directions.
double nextProbeUp(double x) { return nextafter(x, double.infinity); }
double nextProbeDown(double x) { return nextafter(x, -double.infinity); }

/// A redistribution grid that is the identity map, written out so the row order in
/// sg.rgrid.* is explicit rather than implied by D's defaults.
void rs_grids_to_list_from_data(string tag, out double[4][4] rg, out double[4][4] sgr)
{
    double[] id;
    id.length = 16;
    id[2] = 0.0; id[3] = 0.0; id[5] = 0.0; id[7] = 1.0;
    id[8] = 0.0; id[9] = 0.0; id[11] = 0.0; id[13] = 1.0;
    list_to_rs_grids(id, rg, sgr);
    dump_g2d(tag ~ ".r", rg);
    dump_g2d(tag ~ ".s", sgr);
}
