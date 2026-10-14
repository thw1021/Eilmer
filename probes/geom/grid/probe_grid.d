// probes/geom/grid/probe_grid.d — what geom/grid/grid.d (module geom.grid.grid)
// produces, measured against ldc2 1.40.0 at gdtk e587b235.
//
// Grid is the abstract base of StructuredGrid (sgrid.d) and UnstructuredGrid
// (usgrid.d), so this probe is the whole gate for the stage: grid.d declares NO
// `unittest` block (verified: `grep -c unittest geom/grid/grid.d` -> 0), and 17 of
// its members are abstract, so the only way to observe the base class at all is to
// build it through a concrete subclass. That is what ProbeGrid is: a grid whose
// per-cell vertex-id lists are handed in explicitly, so every row below is grid.d's
// own arithmetic, rounding and text — not a derived class' evaluation.
//
// Build and capture (reproducible; the gdtk tree itself is read-only):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/grid \
//     probes/geom/grid/probe_grid.d -of=/tmp/probe_g1 -od/tmp/probe_obj_g1
//   /tmp/probe_g1 > baselines/e587b235/probes/geom_grid_grid.txt
// @nogc has to go for the probe to compile at all: ldc2 1.40 rejects the
// `throw new GeometryException(...)` inside grid.d's own @nogc
// point_is_inside_cell (grid.d:178-229). It is a compile-time attribute with no
// runtime effect, and the C++ port has no @nogc to carry.
//
// What each group pins down:
//   gt.*   gridTypeName / gridTypeFromName (grid.d:24-39), including the unknown-name
//          Error text. Rows are tagged `err=` or `exc=` by which side of D's
//          Throwable split they land on, and one row proves `catch (Exception)` does
//          NOT catch it (grid.d:37 is `throw new Error`).
//   idx.*  single_index (grid.d:71-79) over 1D/2D/3D grids with the k=0 default and
//          out-of-range i/j/k, and ijk_indices (grid.d:81-91). Its guard tests
//          `vertices.length`, NOT `nvertices`, so one row truncates nvertices to show
//          the two differ. Assert messages are the D ones: "index i is invalid" for
//          the three with a message (grid.d:73-75) and "Assertion failure" for the
//          bare one (grid.d:83).
//   ab.*   every abstract member, called through a `Grid` base-class handle, so the
//          pure-virtual table of grid.d:93-122 is exercised with its real arities,
//          const-ness and default arguments. The three overload families (opIndex,
//          get_vtx_id_list_for_cell, copy_vtx_id_list_for_cell) are each driven in
//          both spellings. The 8-slot static array is fully written (tail = 999) so a
//          `ref size_t[8]` out-parameter is observable, not just its first nvtx slots.
//   wr.*   Grid.write(fileName, fmt) (grid.d:104-111): "gziptext", "rawbinary", an
//          unknown fmt and "". The `default:` arm goes to write_to_gzip_file, so the
//          unknown and empty fmts must land on the gzip writer. Marker file bodies are
//          dumped, because they are artefacts users diff.
//   ccp.*  compute_cell_properties (grid.d:124-176) for both true_centroid branches:
//          2D triangle/quad, 3D tetra/hex/pyramid/wedge, all 8 cells of a 2x2x2 grid,
//          and both `throw new Error("Unhandled number of vertices.")` arms (2D nvtx=5,
//          3D nvtx=7) plus the cross-checks that the branch is chosen by `dimensions`
//          and not by the vertex count.
//   pic.*  point_is_inside_cell (grid.d:178-229) at inside / outside / on-face /
//          on-edge / on-vertex points for every supported cell type, plus all four
//          GeometryException wordings (1D, 2D bad cell, 3D bad cell, bad dimensions).
//   cbc.*  cell_barycentre (grid.d:231-241) — the sum-then-scale-by-1/nvtx order and
//          its nvtx coming back through the ref out-parameter.
//   bin.*  get_bin_ix/iy/iz (grid.d:247-275) and sort_cells_into_bins (grid.d:277-317).
//          The clamp `ix = max(0, min(nbx-1, ix))` mixes size_t with int; the
//          `bin.clamp` rows measure D's chosen result *type* and comparison direction,
//          which is signed, so the port must not promote to unsigned. Also measured:
//          the truncating cast(int) on exact halves, points below/above/far outside
//          the box, the 1e-6 floor on deltax/deltay/deltaz (flat and single-point
//          grids), `dimensions == 2 -> nbinz = 1` overriding a passed nbinz=7,
//          get_bin_iz returning 0 in 2D whatever p.z is, the GeometryException for a
//          zero bin count, the bbox loop running to `nvertices` rather than
//          vertices.length, and calling sort twice — which appends into the existing
//          bins rather than clearing them, because `bins.length = nbx` with an
//          unchanged length keeps the contents (grid.d:304-308).
//   fsc.*  find_enclosing_cell_fast (grid.d:319-352) against _slow (grid.d:354-363)
//          against the dispatcher find_enclosing_cell (grid.d:365-373), at the same
//          points before and after sorting. Before sorting the dispatcher takes the
//          slow arm and cannot fail, while a direct fast call dies on an empty `bins`
//          — measured, with D's bounds text. The `fsc.onebin` row is a coarse binning
//          where the 3x3-bin neighbourhood still finds the cell, and the unsorted/sorted
//          pairs are the control that the binning did not change any answer.
//   fncc.* find_nearest_cell_centre (grid.d:375-386), whose comparison is `<` (strict),
//          so on an exact tie the FIRST minimum keeps the slot: three rows show it.
//   stl.*  write_to_stl_file (grid.d:388-433): whole file bodies with newlines marked,
//          for a triangle grid, a quad grid (the 4-triangle fan about
//          cell_barycentre), a 6-cell grid at scale 0.5, the default scale, the 3D
//          `throw new Exception` wording, the two `nvtx=%d` Error paths, and a NaN
//          vertex grid — where normalize() zeroes the facet normal (measured: the
//          magnitude > 0 test fails on NaN) while the vertex lines print nan / -nan
//          under `%.6e`.
//   vtk.*  write_to_vtk_file is abstract in grid.d:114, so what these rows measure is
//          the dispatch of a base-class handle into a subclass writer plus the artefact
//          body it produces; the VTK layout itself belongs to G4/G5.
//   rot.*  rotate(Quaternion, ctr) (grid.d:443-471) and rotate(theta, axis, ctr)
//          (grid.d:481-488): identity, 90 and 180 degree cases, quaternion [0.5,0.5,0.5,
//          0.5], a non-origin centre, the invalid-quaternion Exception text, the
//          uninitialised-Quaternion case (four NaNs, which *passes* the norm test
//          because NaN comparisons are false, and then propagates NaN into every
//          vertex), the near-unit thresholds either side of the 1e-8 window, the
//          zero-axis case (normalize() zeroes it, so the quaternion degenerates and
//          the norm test fires), and a control proving the caller's axis Vector3 is
//          unchanged because D passes structs by value.
//
// Constants are written as explicit decimal literals rather than as folded
// expressions (backlog T-909: ldc2's compile-time folding of literal arithmetic is
// not always correctly rounded), so 1.5707963267948966 appears where double(PI/2) is
// meant. Nothing in the corpus converts a non-finite or out-of-range double to int:
// that is undefined in both languages (ldc happens to print -2147483648) and is
// therefore deliberately not an observable here — see the `bin.clamp` rows.

import std.algorithm;
import std.array;
import std.conv;
import std.file;
import std.format;
import std.math;
import std.stdio;

import geom.grid.grid;
import geom.elements;
import geom.geometry_exception;
import nm.number;

//----------------------------------------------------------------- fixtures

string[] writer_log; // what the abstract readers/writers were asked to do

// Writes all 8 slots, so the untouched tail of a D `size_t[8]` is observable too.
void fill_copy(ref size_t[8] dest, const size_t[] src, ref size_t nvtx)
{
    for (size_t t = 0; t < 8; ++t) {
        dest[t] = (t < src.length) ? src[t] : 999;
    }
    nvtx = src.length;
}

void write_marker(const char[] kind, string fileName, Grid g, double scale)
{
    auto f = File(fileName, "w");
    f.writefln("marker kind=%s scale=%.17g ncells=%d nvertices=%d dims=%d label=%s",
               kind, scale, g.ncells, g.nvertices, g.dimensions, g.label);
    f.close();
}

class ProbeGrid : Grid {
    size_t[][] cell_ids; // vertex ids of each cell index, handed in by the fixture
    // A separate list addressed by single_index(i,j,k), because the vertex lattice
    // (niv,njv,nkv) is not the cell lattice for a structured grid. It cycles through
    // cell_ids so that every in-range (i,j,k) triple has a defined answer.
    size_t[][] ijk_cell_ids;

    this(Grid_t gt, int dims, string lbl)
    {
        super(gt, dims, lbl);
    }

    void load(Vector3[] vs, size_t ni, size_t nj, size_t nk, size_t[][] cids)
    {
        vertices = vs;
        niv = ni; njv = nj; nkv = nk;
        nvertices = vs.length;
        cell_ids = cids;
        ncells = cids.length;
        ijk_cell_ids.length = 0;
        for (size_t t = 0; t < ni * nj * nk; ++t) {
            ijk_cell_ids ~= cids[t % cids.length];
        }
    }

    // ---- the pure-virtual table of grid.d:93-122, in D's declaration order ----
    override Vector3* opIndex(size_t i, size_t j, size_t k = 0)
    {
        return &vertices[single_index(i, j, k)];
    }

    override Vector3* opIndex(size_t indx)
    {
        return &vertices[indx];
    }

    override size_t[] get_vtx_id_list_for_cell(size_t i, size_t j, size_t k = 0) const
    {
        return cast(size_t[]) ijk_cell_ids[single_index(i, j, k)];
    }

    override void copy_vtx_id_list_for_cell(ref size_t[8] vtx_list_copy, ref size_t nvtx,
                                           size_t i, size_t j, size_t k = 0) const
    {
        fill_copy(vtx_list_copy, ijk_cell_ids[single_index(i, j, k)], nvtx);
    }

    override size_t[] get_vtx_id_list_for_cell(size_t indx) const
    {
        return cast(size_t[]) cell_ids[indx];
    }

    override void copy_vtx_id_list_for_cell(ref size_t[8] vtx_list_copy, ref size_t nvtx,
                                           size_t indx) const
    {
        fill_copy(vtx_list_copy, cell_ids[indx], nvtx);
    }

    override void read_from_gzip_file(string fileName, double scale = 1.0)
    {
        writer_log ~= text("read_from_gzip_file ", fileName, " ", scale);
    }

    override void read_from_raw_binary_file(string fileName, double scale = 1.0)
    {
        writer_log ~= text("read_from_raw_binary_file ", fileName, " ", scale);
    }

    override void write_to_gzip_file(string fileName)
    {
        writer_log ~= text("write_to_gzip_file ", fileName);
        write_marker("gziptext", fileName, this, 1.0);
    }

    override void write_to_raw_binary_file(string fileName)
    {
        writer_log ~= text("write_to_raw_binary_file ", fileName);
        write_marker("rawbinary", fileName, this, 1.0);
    }

    override void write_to_vtk_file(string fileName)
    {
        writer_log ~= text("write_to_vtk_file ", fileName);
        auto f = File(fileName, "w");
        f.writeln("# vtk DataFile Version 3.0");
        f.writefln("%s grid", label);
        f.writeln("ASCII");
        f.writeln("DATASET UNSTRUCTURED_GRID");
        f.writefln("POINTS %d float", nvertices);
        foreach (v; vertices) { f.writefln("%.6e %.6e %.6e", v.x, v.y, v.z); }
        size_t tot = 0;
        foreach (c; cell_ids) tot += 1 + c.length;
        f.writefln("CELLS %d %d", ncells, tot);
        foreach (c; cell_ids) {
            string line = text(c.length);
            foreach (q; c) line ~= text(" ", q);
            f.writeln(line);
        }
        f.writefln("CELL_TYPES %d", ncells);
        foreach (i; 0 .. ncells) f.writefln("%d", vtk_element_type_for_cell(i));
        f.close();
    }

    override void write_to_su2_file(string fileName, double scale = 1.0,
                                   bool use_gmsh_order_for_wedges = true)
    {
        writer_log ~= text("write_to_su2_file ", fileName, " ", scale, " ",
                           use_gmsh_order_for_wedges);
        write_marker("su2", fileName, this, scale);
    }

    override size_t number_of_vertices_for_cell(size_t i)
    {
        return cell_ids[i].length;
    }

    override int vtk_element_type_for_cell(size_t i)
    {
        switch (cell_ids[i].length) {
        case 3: return VTKElement.triangle;
        case 4: return VTKElement.quad;
        case 5: return VTKElement.pyramid;
        case 6: return VTKElement.wedge;
        case 8: return VTKElement.hexahedron;
        default: return -1;
        }
    }

    override int get_cell_type(size_t i)
    {
        return cast(int) cell_ids[i].length;
    }

    override Grid get_boundary_grid(size_t boundary_indx)
    {
        if (boundary_indx > 2) return null;
        auto b = new ProbeGrid(grid_type, dimensions, text("boundary_", boundary_indx));
        Vector3[] vs = [vertices[0], vertices[1]];
        size_t[][] cid;
        cid ~= [cast(size_t) 0, cast(size_t) 1];
        b.load(vs, 2, 1, 1, cid);
        return b;
    }

    override size_t[] get_list_of_boundary_cells(size_t boundary_indx)
    {
        size_t[] res;
        foreach (i; 0 .. ncells) {
            if (i % (1 + boundary_indx) == 0) res ~= i;
        }
        return res;
    }
}

size_t[] ids(int[] vals...)
{
    size_t[] res;
    foreach (v; vals) res ~= cast(size_t) v;
    return res;
}

Vector3 V(double x, double y, double z)
{
    return Vector3(x, y, z);
}

// 1D: three vertices along x, two 2-vertex "cells".
ProbeGrid mk_1d()
{
    auto g = new ProbeGrid(Grid_t.structured_grid, 1, "g1d");
    size_t[][] cid;
    cid ~= ids(0, 1);
    cid ~= ids(1, 2);
    g.load([V(0.0, 0.0, 0.0), V(0.5, 0.0, 0.0), V(1.5, 0.0, 0.0)], 3, 1, 1, cid);
    return g;
}

// 2D single triangle: the unit corner.
ProbeGrid mk_2d_tri()
{
    auto g = new ProbeGrid(Grid_t.structured_grid, 2, "tri2d");
    size_t[][] cid;
    cid ~= ids(0, 1, 2);
    g.load([V(0.0, 0.0, 0.0), V(1.0, 0.0, 0.0), V(0.0, 1.0, 0.0)], 3, 1, 1, cid);
    return g;
}

// 2D single quad: a 0.125..0.875 box, so no face sits on 0 or 1.
ProbeGrid mk_2d_quad()
{
    auto g = new ProbeGrid(Grid_t.unstructured_grid, 2, "quad2d");
    size_t[][] cid;
    cid ~= ids(0, 1, 2, 3);
    g.load([V(0.125, 0.25, 0.0), V(0.875, 0.25, 0.0), V(0.875, 0.75, 0.0),
            V(0.125, 0.75, 0.0)], 4, 1, 1, cid);
    return g;
}

// 2D quad grid, 3 x 2 cells over [0,3] x [0,2], for the binning/search rows.
ProbeGrid mk_2d_grid32()
{
    auto g = new ProbeGrid(Grid_t.structured_grid, 2, "grid32");
    Vector3[] vs;
    for (size_t j = 0; j < 3; ++j) {
        for (size_t i = 0; i < 4; ++i) {
            vs ~= V(cast(double) i, cast(double) j, 0.0);
        }
    }
    size_t[][] cid;
    for (size_t j = 0; j < 2; ++j) {
        for (size_t i = 0; i < 3; ++i) {
            cid ~= [i + 4 * j, i + 1 + 4 * j, i + 1 + 4 * (j + 1), i + 4 * (j + 1)];
        }
    }
    g.load(vs, 4, 3, 1, cid);
    return g;
}

// 3D single tetrahedron on the unit corner.
ProbeGrid mk_3d_tetra()
{
    auto g = new ProbeGrid(Grid_t.unstructured_grid, 3, "tet3d");
    size_t[][] cid;
    cid ~= ids(0, 1, 2, 3);
    g.load([V(0.0, 0.0, 0.0), V(1.0, 0.0, 0.0), V(0.0, 1.0, 0.0), V(0.0, 0.0, 1.0)],
           4, 1, 1, cid);
    return g;
}

// 3D single hexahedron, deliberately skewed so that the vertex mean (false) and the
// true centroid differ in all three components.
ProbeGrid mk_3d_hex_skew()
{
    auto g = new ProbeGrid(Grid_t.unstructured_grid, 3, "hexskew");
    size_t[][] cid;
    cid ~= ids(0, 1, 2, 3, 4, 5, 6, 7);
    g.load([V(0.0, 0.0, 0.0), V(1.0, 0.125, 0.0), V(1.125, 1.0, 0.25), V(0.0, 0.875, 0.125),
            V(0.25, 0.25, 1.0), V(1.0, 0.375, 1.0), V(1.25, 1.0, 1.0), V(0.0, 1.0, 1.0)],
           2, 2, 2, cid);
    return g;
}

// 3D single hexahedron, an axis-aligned unit cube: the control on which both
// true_centroid branches must agree.
ProbeGrid mk_3d_hex_unit()
{
    auto g = new ProbeGrid(Grid_t.unstructured_grid, 3, "hexunit");
    size_t[][] cid;
    cid ~= ids(0, 1, 2, 3, 4, 5, 6, 7);
    g.load([V(0.0, 0.0, 0.0), V(1.0, 0.0, 0.0), V(1.0, 1.0, 0.0), V(0.0, 1.0, 0.0),
            V(0.0, 0.0, 1.0), V(1.0, 0.0, 1.0), V(1.0, 1.0, 1.0), V(0.0, 1.0, 1.0)],
           2, 2, 2, cid);
    return g;
}

// 3D single pyramid (quad base + off-centre apex).
ProbeGrid mk_3d_pyramid()
{
    auto g = new ProbeGrid(Grid_t.unstructured_grid, 3, "pyr3d");
    size_t[][] cid;
    cid ~= ids(0, 1, 2, 3, 4);
    g.load([V(0.0, 0.0, 0.0), V(1.0, 0.0, 0.0), V(1.0, 1.0, 0.0), V(0.0, 1.0, 0.0),
            V(0.125, 0.25, 1.0)], 2, 2, 2, cid);
    return g;
}

// 3D single wedge (two triangles + three quads).
ProbeGrid mk_3d_wedge()
{
    auto g = new ProbeGrid(Grid_t.unstructured_grid, 3, "wedge3d");
    size_t[][] cid;
    cid ~= ids(0, 1, 2, 3, 4, 5);
    g.load([V(0.0, 0.0, 0.0), V(0.5, 1.0, 0.0), V(1.0, 0.0, 0.0), V(0.0, 0.0, 1.0),
            V(0.5, 1.0, 1.0), V(1.0, 0.0, 1.0)], 2, 2, 2, cid);
    return g;
}

// 2x2x2 hexahedral grid over [0,1]^3: 27 vertices, 8 cells.
ProbeGrid mk_3d_grid222()
{
    auto g = new ProbeGrid(Grid_t.structured_grid, 3, "grid222");
    Vector3[] vs;
    for (size_t k = 0; k < 3; ++k) {
        for (size_t j = 0; j < 3; ++j) {
            for (size_t i = 0; i < 3; ++i) {
                vs ~= V(cast(double) i * 0.5, cast(double) j * 0.5, cast(double) k * 0.5);
            }
        }
    }
    size_t[][] cid;
    for (size_t k = 0; k < 2; ++k) {
        for (size_t j = 0; j < 2; ++j) {
            for (size_t i = 0; i < 2; ++i) {
                size_t a = i + 3 * j + 9 * k;
                size_t b = i + 1 + 3 * j + 9 * k;
                size_t c = i + 1 + 3 * (j + 1) + 9 * k;
                size_t d = i + 3 * (j + 1) + 9 * k;
                size_t e = i + 3 * j + 9 * (k + 1);
                size_t f = i + 1 + 3 * j + 9 * (k + 1);
                size_t uu = i + 1 + 3 * (j + 1) + 9 * (k + 1);
                size_t w = i + 3 * (j + 1) + 9 * (k + 1);
                cid ~= [a, b, c, d, e, f, uu, w];
            }
        }
    }
    g.load(vs, 3, 3, 3, cid);
    return g;
}

// A 2D cell with 5 vertices: hits both the compute_cell_properties and the
// point_is_inside_cell default arms.
ProbeGrid mk_2d_pent()
{
    auto g = new ProbeGrid(Grid_t.unstructured_grid, 2, "pent2d");
    size_t[][] cid;
    cid ~= ids(0, 1, 2, 3, 4);
    g.load([V(0.0, 0.0, 0.0), V(1.0, 0.0, 0.0), V(1.0, 1.0, 0.0), V(0.0, 1.0, 0.0),
            V(0.5, 0.5, 0.0)], 5, 1, 1, cid);
    return g;
}

// A 3D cell with 7 vertices: hits the `default:` Error of the 3D switch.
ProbeGrid mk_3d_hept()
{
    auto g = new ProbeGrid(Grid_t.unstructured_grid, 3, "hept3d");
    size_t[][] cid;
    cid ~= ids(0, 1, 2, 3, 4, 5, 6);
    g.load([V(0.0, 0.0, 0.0), V(1.0, 0.0, 0.0), V(1.0, 1.0, 0.0), V(0.0, 1.0, 0.0),
            V(0.0, 0.0, 1.0), V(1.0, 0.0, 1.0), V(1.0, 1.0, 1.0)], 7, 1, 1, cid);
    return g;
}

// dimensions=4: only reachable through the invalid-dimensions wording.
ProbeGrid mk_4d()
{
    auto g = new ProbeGrid(Grid_t.unstructured_grid, 4, "fourd");
    size_t[][] cid;
    cid ~= ids(0, 1, 2);
    g.load([V(0.0, 0.0, 0.0), V(1.0, 0.0, 0.0), V(0.0, 1.0, 0.0)], 3, 1, 1, cid);
    return g;
}

// A flat 2D "grid" of one 2-vertex cell: deltay hits the 1e-6 floor, and nvtx=2 is
// an unhandled cell type in 2D.
ProbeGrid mk_2d_flat()
{
    auto g = new ProbeGrid(Grid_t.unstructured_grid, 2, "flat2d");
    size_t[][] cid;
    cid ~= ids(0, 1);
    g.load([V(0.0, 0.5, 0.0), V(1.0, 0.5, 0.0)], 2, 1, 1, cid);
    return g;
}

// One vertex, referenced four times: bb0 == bb1, so all three deltas are floored.
ProbeGrid mk_1pt()
{
    auto g = new ProbeGrid(Grid_t.unstructured_grid, 3, "onept");
    size_t[][] cid;
    cid ~= ids(0, 0, 0, 0);
    g.load([V(0.25, 0.25, 0.25)], 1, 1, 1, cid);
    return g;
}

// A 2D grid holding a NaN vertex, for the %.6e NaN / -NaN rows of the STL writer.
ProbeGrid mk_2d_nan()
{
    auto g = new ProbeGrid(Grid_t.unstructured_grid, 2, "nan2d");
    size_t[][] cid;
    cid ~= ids(0, 1, 2);
    g.load([V(double.nan, 0.0, 0.0), V(-double.nan, 1.0, 0.0), V(1.0, 0.0, 0.0)],
           3, 1, 1, cid);
    return g;
}

//----------------------------------------------------------------- dumping

void dump3(const char[] tag, Vector3 v)
{
    writefln("%s x=%.17g y=%.17g z=%.17g", tag, v.x, v.y, v.z);
}

string body_of(string path)
{
    return join(split(cast(string) read(path), "\n"), "<newline>");
}

size_t len_of(string path)
{
    return (cast(string) read(path)).length;
}

// `err=` is a D Error (grid.d:37, :144, :174, :428 and every assert failure);
// `exc=` is a D Exception (GeometryException, and the guards at grid.d:395 and :447).
void tryit(const char[] tag, void delegate() dg)
{
    try {
        dg();
        writefln("%s done", tag);
    }
    catch (Exception e) {
        writefln("%s exc=%s", tag, e.msg);
    }
    catch (Error e) {
        writefln("%s err=%s", tag, e.msg);
    }
}

void dump_props(Grid g, size_t indx, bool true_centroid, const char[] tag)
{
    Vector3 c;
    number vol;
    g.compute_cell_properties(indx, true_centroid, c, vol);
    writefln("%s tc=%s vol=%.17g cx=%.17g cy=%.17g cz=%.17g", tag, true_centroid, vol, c.x, c.y,
             c.z);
}

void dump_inside(Grid g, Vector3 p, size_t indx, const char[] tag)
{
    Vector3 pp = p;
    bool inside = g.point_is_inside_cell(pp, indx);
    writefln("%s inside=%s", tag, inside);
}

// Only the non-empty bins are printed, plus a count of the empty ones and the number
// of cells placed: the occupancy is the observable, the 125 empty rows are not.
void dump_bins(Grid g, const char[] tag)
{
    writefln("%s nbx=%d nby=%d nbz=%d deltax=%.17g deltay=%.17g deltaz=%.17g sorted=%s", tag,
             g.nbx, g.nby, g.nbz, g.deltax, g.deltay, g.deltaz, g.cells_are_sorted_into_bins);
    dump3(tag ~ ".bb0", g.bb0);
    dump3(tag ~ ".bb1", g.bb1);
    size_t nempty = 0; size_t placed = 0;
    foreach (ix; 0 .. g.nbx) {
        foreach (iy; 0 .. g.nby) {
            foreach (iz; 0 .. g.nbz) {
                auto lst = g.bins[ix][iy][iz];
                placed += lst.length;
                if (lst.length == 0) { ++nempty; }
                else { writefln("%s[%d][%d][%d]=%s", tag, ix, iy, iz, lst); }
            }
        }
    }
    writefln("%s empty=%d placed=%d depth=%d", tag, nempty, placed, g.bins.length);
}

void dump_bin_indices(Grid g, Vector3 p, const char[] tag)
{
    Vector3 pp = p;
    writefln("%s ix=%d iy=%d iz=%d", tag, g.get_bin_ix(pp), g.get_bin_iy(pp), g.get_bin_iz(pp));
}

void dump_search(Grid g, Vector3 p, const char[] tag)
{
    Vector3 pp = p;
    size_t indx;
    bool found;
    tryit(text(tag, " fast"), { g.find_enclosing_cell_fast(pp, indx, found);
                              writefln("  found=%s indx=%d", found, indx); });
    tryit(text(tag, " slow"), { g.find_enclosing_cell_slow(pp, indx, found);
                              writefln("  found=%s indx=%d", found, indx); });
    tryit(text(tag, " disp"), { g.find_enclosing_cell(pp, indx, found);
                               writefln("  found=%s indx=%d", found, indx); });
}

void nearest(Grid g, Vector3 p, const char[] tag)
{
    Vector3 pp = p;
    size_t nc;
    double md;
    g.find_nearest_cell_centre(pp, nc, md);
    writefln("%s nearest=%d dist=%.17g", tag, nc, md);
}

void dump_verts(Grid g, const char[] tag)
{
    writefln("%s nv=%d", tag, g.nvertices);
    foreach (i; 0 .. g.nvertices) {
        dump3(text(tag, " v", i), *g[i]);
    }
}

//----------------------------------------------------------------- main

void main()
{
    //============================= gt: grid type names
    writefln("gt.name structured=%s unstructured=%s", gridTypeName(Grid_t.structured_grid),
             gridTypeName(Grid_t.unstructured_grid));
    writefln("gt.from structured=%d unstructured=%d", gridTypeFromName("structured_grid"),
             gridTypeFromName("unstructured_grid"));
    tryit("gt.from.empty", { writefln("  %d", gridTypeFromName("")); });
    tryit("gt.from.unknown", { writefln("  %d", gridTypeFromName("tetrahedral")); });
    tryit("gt.from.nearmiss", { writefln("  %d", gridTypeFromName("Structured_grid")); });
    tryit("gt.from.dash", { writefln("  %d", gridTypeFromName("structured-grid")); });
    tryit("gt.from.spaces", { writefln("  %d", gridTypeFromName(" structured_grid ")); });
    // A catch(Exception) must not swallow the Error thrown at grid.d:37.
    try {
        auto t = gridTypeFromName("unstructured");
        writefln("gt.nocatch %d", t);
    }
    catch (Exception e) {
        writefln("gt.nocatch EXC %s", e.msg);
    }
    catch (Error e) {
        writefln("gt.nocatch ERR %s", e.msg);
    }

    //============================= idx: unified indexing
    auto g1 = mk_1d();
    auto g2 = mk_2d_quad();
    auto g3 = mk_3d_grid222();
    writefln("idx.fields niv=%d njv=%d nkv=%d nvertices=%d vlength=%d ncells=%d label=%s dims=%d",
             g1.niv, g1.njv, g1.nkv, g1.nvertices, g1.vertices.length, g1.ncells, g1.label,
             g1.dimensions);
    writefln("idx.1d si(0,0)=%d si(1,0)=%d si(2,0)=%d si(2,0,0)=%d", g1.single_index(0, 0),
             g1.single_index(1, 0), g1.single_index(2, 0), g1.single_index(2, 0, 0));
    tryit("idx.1d si(0,1)", { writefln("  %d", g1.single_index(0, 1)); });
    tryit("idx.1d si(3,0)", { writefln("  %d", g1.single_index(3, 0)); });
    tryit("idx.1d si(0,0,1)", { writefln("  %d", g1.single_index(0, 0, 1)); });
    writefln("idx.2d si(0,0)=%d si(3,0)=%d si(3,0,0)=%d", g2.single_index(0, 0),
             g2.single_index(3, 0), g2.single_index(3, 0, 0));
    tryit("idx.2d si(4,0)", { writefln("  %d", g2.single_index(4, 0)); });
    writefln("idx.3d si(0,0,0)=%d si(2,0,0)=%d si(0,2,0)=%d si(0,0,2)=%d si(2,2,2)=%d",
             g3.single_index(0, 0, 0), g3.single_index(2, 0, 0), g3.single_index(0, 2, 0),
             g3.single_index(0, 0, 2), g3.single_index(2, 2, 2));
    tryit("idx.3d si(3,0,0)", { writefln("  %d", g3.single_index(3, 0, 0)); });
    tryit("idx.3d si(0,3,0)", { writefln("  %d", g3.single_index(0, 3, 0)); });
    tryit("idx.3d si(0,0,3)", { writefln("  %d", g3.single_index(0, 0, 3)); });
    writefln("idx.ijk 1d(0)=%s 1d(2)=%s 2d(3)=%s 3d(0)=%s 3d(2)=%s 3d(4)=%s 3d(12)=%s 3d(26)=%s",
             g1.ijk_indices(0), g1.ijk_indices(2), g2.ijk_indices(3), g3.ijk_indices(0),
             g3.ijk_indices(2), g3.ijk_indices(4), g3.ijk_indices(12), g3.ijk_indices(26));
    tryit("idx.ijk 3d(27)", { writefln("  %s", g3.ijk_indices(27)); });
    auto gt = mk_3d_grid222();
    gt.nvertices = 2;
    tryit("idx.ijk truncated nvertices", {
        writefln("  %s nvertices=%d vlength=%d", gt.ijk_indices(26), gt.nvertices,
                 gt.vertices.length); });
    writefln("idx.ijk tri(2)=%s tri(0)=%s", mk_2d_tri().ijk_indices(2), mk_2d_tri().ijk_indices(0));

    //============================= ab: the abstract table through a Grid handle
    Grid hg = g3;
    writefln("ab.handle label=%s dims=%d ncells=%d nvertices=%d", hg.label, hg.dimensions,
             hg.ncells, hg.nvertices);
    dump3("ab.opIndex(0,0,0)", *hg[0, 0, 0]);
    dump3("ab.opIndex(2,1,0)", *hg[2, 1, 0]);
    dump3("ab.opIndex(2,1)", *hg[2, 1]);
    dump3("ab.opIndex(7)", *hg[7]);
    dump3("ab.opIndex(0)", *hg[0]);
    writefln("ab.vtxids(ijk) (0,0,0)=%s (1,1,1)=%s (2,1,0)=%s", hg.get_vtx_id_list_for_cell(0, 0, 0),
             hg.get_vtx_id_list_for_cell(1, 1, 1), hg.get_vtx_id_list_for_cell(2, 1, 0));
    writefln("ab.vtxids(indx) 0=%s 7=%s", hg.get_vtx_id_list_for_cell(0),
             hg.get_vtx_id_list_for_cell(7));
    size_t[8] vc;
    size_t vn;
    hg.copy_vtx_id_list_for_cell(vc, vn, 0, 0, 0);
    writefln("ab.copy(ijk 0,0,0) nvtx=%d list=%s", vn, vc[]);
    hg.copy_vtx_id_list_for_cell(vc, vn, 1, 1, 1);
    writefln("ab.copy(ijk 1,1,1) nvtx=%d list=%s", vn, vc[]);
    hg.copy_vtx_id_list_for_cell(vc, vn, 7);
    writefln("ab.copy(indx 7) nvtx=%d list=%s", vn, vc[]);
    hg.copy_vtx_id_list_for_cell(vc, vn, 3);
    writefln("ab.copy(indx 3) nvtx=%d list=%s", vn, vc[]);
    writefln("ab.nvtx 0=%d 7=%d vtktype 0=%d celltype 0=%d", hg.number_of_vertices_for_cell(0),
             hg.number_of_vertices_for_cell(7), hg.vtk_element_type_for_cell(0),
             hg.get_cell_type(0));
    writefln("ab.bdc 0=%s 1=%s 2=%s", hg.get_list_of_boundary_cells(0),
             hg.get_list_of_boundary_cells(1), hg.get_list_of_boundary_cells(2));
    Grid b0 = hg.get_boundary_grid(1);
    writefln("ab.bdgrid label=%s dims=%d ncells=%d nvertices=%d", b0.label, b0.dimensions,
             b0.ncells, b0.nvertices);
    dump3("ab.bdgrid v0", *b0[0]);
    dump3("ab.bdgrid v1", *b0[1]);
    Grid bnull = hg.get_boundary_grid(5);
    writefln("ab.bdgrid(5) isnull=%s", bnull is null);
    writer_log.length = 0;
    hg.read_from_gzip_file("in.gz");
    hg.read_from_gzip_file("in.gz", 2.5);
    hg.read_from_raw_binary_file("in.bin");
    hg.read_from_raw_binary_file("in.bin", 0.125);
    hg.write_to_su2_file("out.su2");
    hg.write_to_su2_file("out.su2", 2.5);
    hg.write_to_su2_file("out.su2", 2.5, false);
    foreach (k, entry; writer_log) writefln("ab.log[%d]=%s", k, entry);
    writefln("ab.log=%s", writer_log);

    //============================= wr: Grid.write(fileName, fmt) dispatch
    writer_log.length = 0;
    string wg = "/tmp/probe_g1_write_gziptext.txt";
    string wb = "/tmp/probe_g1_write_rawbinary.txt";
    string wu = "/tmp/probe_g1_write_unknown.txt";
    string we = "/tmp/probe_g1_write_empty.txt";
    string wv = "/tmp/probe_g1_write_vtkfmt.txt";
    g2.write(wg, "gziptext");
    g2.write(wb, "rawbinary");
    g2.write(wu, "voxbin");
    g2.write(we, "");
    g2.write(wv, "vtk");
    foreach (k, entry; writer_log) writefln("wr.log[%d]=%s", k, entry);
    writefln("wr.gz len=%d body=%s", len_of(wg), body_of(wg));
    writefln("wr.bin len=%d body=%s", len_of(wb), body_of(wb));
    writefln("wr.unknown len=%d body=%s", len_of(wu), body_of(wu));
    writefln("wr.empty len=%d body=%s", len_of(we), body_of(we));
    writefln("wr.vtkfmt len=%d body=%s", len_of(wv), body_of(wv));

    //============================= ccp: compute_cell_properties
    auto t2 = mk_2d_tri();
    dump_props(t2, 0, false, "ccp.tri2d");
    dump_props(t2, 0, true, "ccp.tri2d");
    auto q2 = mk_2d_quad();
    dump_props(q2, 0, false, "ccp.quad2d");
    dump_props(q2, 0, true, "ccp.quad2d");
    auto fq = mk_2d_flat();
    tryit("ccp.flat2d nvtx=2", { dump_props(fq, 0, false, "  ccp.flat2d"); });
    auto tet = mk_3d_tetra();
    dump_props(tet, 0, false, "ccp.tet3d");
    dump_props(tet, 0, true, "ccp.tet3d");
    auto hx = mk_3d_hex_skew();
    dump_props(hx, 0, false, "ccp.hexskew");
    dump_props(hx, 0, true, "ccp.hexskew");
    auto hu = mk_3d_hex_unit();
    dump_props(hu, 0, false, "ccp.hexunit");
    dump_props(hu, 0, true, "ccp.hexunit");
    auto py = mk_3d_pyramid();
    dump_props(py, 0, false, "ccp.pyr3d");
    dump_props(py, 0, true, "ccp.pyr3d");
    auto wd = mk_3d_wedge();
    dump_props(wd, 0, false, "ccp.wedge3d");
    dump_props(wd, 0, true, "ccp.wedge3d");
    auto g222 = mk_3d_grid222();
    foreach (i; 0 .. 8) dump_props(g222, i, false, text("ccp.grid222 cell", i));
    foreach (i; 0 .. 8) dump_props(g222, i, true, text("ccp.grid222 tc cell", i));
    tryit("ccp.pent2d nvtx=5", { dump_props(mk_2d_pent(), 0, false, "  ccp.pent2d"); });
    tryit("ccp.hept3d nvtx=7", { dump_props(mk_3d_hept(), 0, false, "  ccp.hept3d"); });
    tryit("ccp.onept nvtx=4 coincident", { dump_props(mk_1pt(), 0, false, "  ccp.onept"); });
    // The branch is chosen by `dimensions`, not by the vertex count.
    auto as2d = mk_3d_hept();
    as2d.dimensions = 2;
    tryit("ccp.hept relabelled 2D", { dump_props(as2d, 0, false, "  ccp.hept2d"); });
    auto as3d = mk_2d_pent();
    as3d.dimensions = 3;
    dump_props(as3d, 0, true, "ccp.pent relabelled 3D");
    dump_props(as3d, 0, false, "ccp.pent relabelled 3D");
    auto quad3d = mk_2d_quad();
    quad3d.dimensions = 3;
    dump_props(quad3d, 0, true, "ccp.quad relabelled 3D");
    tryit("ccp.tri relabelled 3D", { dump_props(mk_2d_tri(), 0, false, "  ccp.tri3d"); });
    tryit("ccp.wedge relabelled 2D", { dump_props(mk_3d_wedge(), 0, false, "  ccp.wedge2d"); });

    //============================= pic: point_is_inside_cell
    auto qt = mk_2d_quad();
    dump_inside(qt, V(0.5, 0.5, 0.0), 0, "pic.quad2d centre");
    dump_inside(qt, V(0.2, 0.3, 0.0), 0, "pic.quad2d inside");
    dump_inside(qt, V(0.1875, 0.78125, 0.0), 0, "pic.quad2d inside near lo");
    dump_inside(qt, V(1.5, 0.5, 0.0), 0, "pic.quad2d outside");
    dump_inside(qt, V(-0.5, 0.5, 0.0), 0, "pic.quad2d outside neg");
    dump_inside(qt, V(0.875, 0.5, 0.0), 0, "pic.quad2d on face xhi");
    dump_inside(qt, V(0.125, 0.5, 0.0), 0, "pic.quad2d on face xlo");
    dump_inside(qt, V(0.5, 0.25, 0.0), 0, "pic.quad2d on face ylo");
    dump_inside(qt, V(0.5, 0.75, 0.0), 0, "pic.quad2d on face yhi");
    dump_inside(qt, V(0.875, 0.75, 0.0), 0, "pic.quad2d on edge hi");
    dump_inside(qt, V(0.125, 0.25, 0.0), 0, "pic.quad2d on edge lo");
    dump_inside(qt, V(0.5, 0.5, 3.0), 0, "pic.quad2d off plane z");
    dump_inside(qt, V(0.87500011920928955, 0.5, 0.0), 0, "pic.quad2d just out xhi");
    auto tt = mk_2d_tri();
    dump_inside(tt, V(0.25, 0.25, 0.0), 0, "pic.tri2d inside");
    dump_inside(tt, V(0.5, 0.5, 0.0), 0, "pic.tri2d on hypotenuse");
    dump_inside(tt, V(0.75, 0.75, 0.0), 0, "pic.tri2d outside");
    dump_inside(tt, V(0.0, 0.0, 0.0), 0, "pic.tri2d on vertex 0");
    dump_inside(tt, V(1.0, 0.0, 0.0), 0, "pic.tri2d on vertex 1");
    dump_inside(tt, V(0.5, 0.0, 0.0), 0, "pic.tri2d on edge ylo");
    dump_inside(tt, V(0.0, 0.5, 0.0), 0, "pic.tri2d on edge xlo");
    dump_inside(tt, V(0.125, 0.8125, 0.0), 0, "pic.tri2d inside upper");
    auto h1 = mk_3d_hex_unit();
    dump_inside(h1, V(0.5, 0.5, 0.5), 0, "pic.hexunit centre");
    dump_inside(h1, V(0.125, 0.375, 0.625), 0, "pic.hexunit inside");
    dump_inside(h1, V(1.5, 0.5, 0.5), 0, "pic.hexunit outside");
    dump_inside(h1, V(1.0, 0.5, 0.5), 0, "pic.hexunit on face xhi");
    dump_inside(h1, V(0.0, 0.5, 0.5), 0, "pic.hexunit on face xlo");
    dump_inside(h1, V(0.5, 0.5, 1.0), 0, "pic.hexunit on face zhi");
    dump_inside(h1, V(1.0, 1.0, 0.5), 0, "pic.hexunit on edge");
    dump_inside(h1, V(0.0, 0.0, 0.0), 0, "pic.hexunit on vertex");
    dump_inside(h1, V(0.5, 1.0, 0.0), 0, "pic.hexunit on edge 2");
    auto hsk = mk_3d_hex_skew();
    dump_inside(hsk, V(0.5, 0.5, 0.5), 0, "pic.hexskew centre");
    dump_inside(hsk, V(1.2, 0.5, 0.5), 0, "pic.hexskew outside");
    auto t3 = mk_3d_tetra();
    dump_inside(t3, V(0.125, 0.125, 0.125), 0, "pic.tet3d inside");
    dump_inside(t3, V(0.5, 0.5, 0.5), 0, "pic.tet3d outside");
    dump_inside(t3, V(0.25, 0.25, 0.5), 0, "pic.tet3d on slanted face");
    dump_inside(t3, V(0.5, 0.0, 0.5), 0, "pic.tet3d on xz face");
    dump_inside(t3, V(0.0, 0.0, 0.0), 0, "pic.tet3d on vertex");
    auto p3 = mk_3d_pyramid();
    dump_inside(p3, V(0.125, 0.25, 0.25), 0, "pic.pyr3d inside");
    dump_inside(p3, V(0.5, 0.5, 1.5), 0, "pic.pyr3d outside");
    dump_inside(p3, V(0.5, 0.5, 0.0), 0, "pic.pyr3d on base");
    dump_inside(p3, V(0.125, 0.25, 1.0), 0, "pic.pyr3d on apex");
    auto w3 = mk_3d_wedge();
    dump_inside(w3, V(0.5, 0.25, 0.5), 0, "pic.wedge3d inside");
    dump_inside(w3, V(0.5, 0.3125, 0.5), 0, "pic.wedge3d inside 2");
    dump_inside(w3, V(0.9, 0.5, 0.5), 0, "pic.wedge3d outside");
    dump_inside(w3, V(0.5, 0.0, 0.5), 0, "pic.wedge3d on face ylo");
    dump_inside(w3, V(0.5, 0.25, 1.0), 0, "pic.wedge3d on face zhi");
    dump_inside(w3, V(0.75, 0.5, 0.5), 0, "pic.wedge3d on slant face");
    dump_inside(w3, V(0.5, 1.0, 0.5), 0, "pic.wedge3d on edge apex");
    tryit("pic.1d", { dump_inside(g1, V(0.5, 0.0, 0.0), 0, "  pic.1d"); });
    tryit("pic.4d", { dump_inside(mk_4d(), V(0.5, 0.5, 0.0), 0, "  pic.4d"); });
    auto d0 = mk_4d();
    d0.dimensions = 0;
    tryit("pic.0d", { dump_inside(d0, V(0.5, 0.5, 0.0), 0, "  pic.0d"); });
    tryit("pic.2d nvtx=5", { dump_inside(mk_2d_pent(), V(0.5, 0.5, 0.0), 0, "  pic.pent2d"); });
    tryit("pic.3d nvtx=7", { dump_inside(mk_3d_hept(), V(0.5, 0.5, 0.5), 0, "  pic.hept3d"); });
    tryit("pic.2d nvtx=2", { dump_inside(fq, V(0.5, 0.5, 0.0), 0, "  pic.flat2d"); });
    auto wd2 = mk_3d_wedge();
    wd2.dimensions = 4;
    tryit("pic.dims4 nvtx=6", { dump_inside(wd2, V(0.5, 0.25, 0.5), 0, "  pic.wedge4d"); });

    //============================= cbc: cell_barycentre
    dump3("cbc.tri2d", t2.cell_barycentre(0));
    dump3("cbc.quad2d", qt.cell_barycentre(0));
    dump3("cbc.flat2d", fq.cell_barycentre(0));
    dump3("cbc.tet3d", t3.cell_barycentre(0));
    dump3("cbc.hexunit", h1.cell_barycentre(0));
    dump3("cbc.hexskew", hx.cell_barycentre(0));
    dump3("cbc.pyr3d", p3.cell_barycentre(0));
    dump3("cbc.wedge3d", w3.cell_barycentre(0));
    dump3("cbc.pent2d", mk_2d_pent().cell_barycentre(0));
    dump3("cbc.hept3d", mk_3d_hept().cell_barycentre(0));
    dump3("cbc.onept", mk_1pt().cell_barycentre(0));
    foreach (i; 0 .. 8) dump3(text("cbc.grid222 cell", i), g222.cell_barycentre(i));
    dump3("cbc.nan2d", mk_2d_nan().cell_barycentre(0));

    //============================= bin: get_bin_* and sort_cells_into_bins
    auto b3 = mk_3d_grid222();
    writefln("bin.unsorted nbx=%d nby=%d nbz=%d deltax=%.17g deltay=%.17g deltaz=%.17g sorted=%s depth=%d",
             b3.nbx, b3.nby, b3.nbz, b3.deltax, b3.deltay, b3.deltaz,
             b3.cells_are_sorted_into_bins, b3.bins.length);
    dump3("bin.unsorted bb0", b3.bb0);
    dump3("bin.unsorted bb1", b3.bb1);
    b3.sort_cells_into_bins();
    dump_bins(b3, "bin.222.default");
    dump_bin_indices(b3, V(0.0, 0.0, 0.0), "bin.222 corner lo");
    dump_bin_indices(b3, V(1.0, 1.0, 1.0), "bin.222 corner hi");
    dump_bin_indices(b3, V(0.5, 0.5, 0.5), "bin.222 centre");
    dump_bin_indices(b3, V(0.125, 0.875, 0.375), "bin.222 inside");
    dump_bin_indices(b3, V(-0.5, -0.5, -0.5), "bin.222 outside lo");
    dump_bin_indices(b3, V(2.0, 3.0, 4.0), "bin.222 outside hi");
    dump_bin_indices(b3, V(1.0, 0.0, 1.0), "bin.222 edge mix");
    dump_bin_indices(b3, V(0.0, 1.0, 0.5), "bin.222 edge mix2");
    dump_bin_indices(b3, V(-1000.0, 500.0, -0.25), "bin.222 far");
    dump_bin_indices(b3, V(0.2, 0.3, 0.30000000000000004), "bin.222 nonexact");
    auto b3c = mk_3d_grid222();
    b3c.sort_cells_into_bins(3, 4, 2);
    dump_bins(b3c, "bin.222.n342");
    dump_bin_indices(b3c, V(0.5, 0.5, 0.5), "bin.n342 centre");
    dump_bin_indices(b3c, V(-0.5, -0.5, -0.5), "bin.n342 outside lo");
    dump_bin_indices(b3c, V(1.5, 1.5, 1.5), "bin.n342 outside hi");
    dump_bin_indices(b3c, V(0.33333333333333331, 0.66666666666666663, 1.0), "bin.n342 thirds");
    auto b2 = mk_2d_grid32();
    b2.sort_cells_into_bins();
    dump_bins(b2, "bin.32.default");
    auto b2z = mk_2d_grid32();
    b2z.sort_cells_into_bins(2, 3, 7);
    dump_bins(b2z, "bin.32.nbinz7");
    dump_bin_indices(b2z, V(1.5, 1.0, 0.0), "bin.32 centre");
    dump_bin_indices(b2z, V(-0.5, -0.5, 4.0), "bin.32 outside");
    dump_bin_indices(b2z, V(3.0, 2.0, 0.0), "bin.32 corner hi");
    Vector3 pz = V(1.0, 1.0, 4.0);
    writefln("bin.iz 2d p.z=4 -> iz=%d", b2z.get_bin_iz(pz));
    auto b1 = mk_1d();
    b1.sort_cells_into_bins(2, 1, 1);
    dump_bins(b1, "bin.1d");
    dump_bin_indices(b1, V(1.5, 0.0, 0.0), "bin.1d far hi");
    dump_bin_indices(b1, V(-1.5, 3.0, 3.0), "bin.1d far lo");
    tryit("bin.nbinx=0", { mk_3d_grid222().sort_cells_into_bins(0, 5, 5); });
    tryit("bin.nbiny=0", { mk_3d_grid222().sort_cells_into_bins(5, 0, 5); });
    tryit("bin.nbinz=0", { mk_3d_grid222().sort_cells_into_bins(5, 5, 0); });
    tryit("bin.all zero", { mk_3d_grid222().sort_cells_into_bins(0, 0, 0); });
    auto bbf = mk_3d_grid222();
    tryit("bin.reorder after throw", {
        bbf.sort_cells_into_bins(0, 2, 2);
    });
    dump_bins(bbf, "bin.after throw");
    auto bf = mk_2d_flat();
    bf.sort_cells_into_bins(2, 2, 2);
    dump_bins(bf, "bin.flat floor");
    dump_bin_indices(bf, V(0.5, 0.5, 0.0), "bin.flat centre");
    dump_bin_indices(bf, V(0.5, 100.0, 0.0), "bin.flat far y");
    auto bp = mk_1pt();
    bp.sort_cells_into_bins(4, 4, 4);
    dump_bins(bp, "bin.onept floor");
    dump_bin_indices(bp, V(0.25, 0.25, 0.25), "bin.onept same");
    dump_bin_indices(bp, V(9.0, 9.0, 9.0), "bin.onept far");
    auto bt = mk_3d_grid222();
    bt.sort_cells_into_bins(2, 2, 2);
    bt.sort_cells_into_bins(2, 2, 2);
    dump_bins(bt, "bin.twice");
    bt.sort_cells_into_bins(1, 1, 1);
    dump_bins(bt, "bin.thrice shrunk");
    auto bn = mk_3d_grid222();
    bn.nvertices = 8;
    bn.sort_cells_into_bins(2, 2, 2);
    dump_bins(bn, "bin.nvtx truncated");

    // The clamp of grid.d:252-253, measured so the port keeps D's chosen operand type.
    size_t nbx_probe = 4;
    foreach (int ix; [-5, -1, 0, 1, 3, 4, 99]) {
        auto m = min(nbx_probe - 1, ix);
        writefln("bin.clamp ix=%d min=%s (%s) max=%d", ix, to!string(m),
                 typeof(m).stringof, max(0, m));
    }
    foreach (int n; [-7, -3, -1, 0, 1, 3, 7, 9]) {
        double d = cast(double) n / 2.0;
        writefln("bin.trunc n=%d d=%.17g castint=%d", n, d, cast(int) d);
    }

    //============================= fsc: enclosing-cell search
    auto fs = mk_3d_grid222();
    dump_search(fs, V(0.75, 0.25, 0.25), "fsc.unsorted a");
    dump_search(fs, V(0.5, 0.5, 0.5), "fsc.unsorted b");
    dump_search(fs, V(0.0, 0.0, 0.0), "fsc.unsorted c");
    dump_search(fs, V(1.0, 1.0, 1.0), "fsc.unsorted d");
    dump_search(fs, V(2.0, 0.5, 0.5), "fsc.unsorted e");
    dump_search(fs, V(-1.0, -1.0, -1.0), "fsc.unsorted f");
    fs.sort_cells_into_bins(2, 2, 2);
    dump_search(fs, V(0.75, 0.25, 0.25), "fsc.sorted a");
    dump_search(fs, V(0.5, 0.5, 0.5), "fsc.sorted b");
    dump_search(fs, V(0.0, 0.0, 0.0), "fsc.sorted c");
    dump_search(fs, V(1.0, 1.0, 1.0), "fsc.sorted d");
    dump_search(fs, V(2.0, 0.5, 0.5), "fsc.sorted e");
    dump_search(fs, V(-1.0, -1.0, -1.0), "fsc.sorted f");
    dump_search(fs, V(0.125, 0.875, 0.625), "fsc.sorted g");
    dump_search(fs, V(0.9999998807907104, 0.5, 0.5), "fsc.sorted near hi face");
    dump_search(fs, V(0.75, 0.75, 0.25), "fsc.sorted h");
    // One bin in every direction: the centre bin is the only bin, so the neighbourhood
    // loop can add nothing. Same answers as the fine binning is the control here.
    auto fm = mk_3d_grid222();
    fm.sort_cells_into_bins(1, 1, 1);
    dump_search(fm, V(0.75, 0.25, 0.25), "fsc.onebin a");
    dump_search(fm, V(-1.0, -1.0, -1.0), "fsc.onebin out");
    auto ff = mk_2d_grid32();
    ff.sort_cells_into_bins(3, 2, 1);
    dump_search(ff, V(1.5, 1.5, 0.0), "fsc.2d centre");
    dump_search(ff, V(0.5, 0.5, 0.0), "fsc.2d cell0");
    dump_search(ff, V(2.5, 1.5, 0.0), "fsc.2d cell5");
    dump_search(ff, V(1.5, 1.5, 5.0), "fsc.2d off plane");
    dump_search(ff, V(3.5, 1.0, 0.0), "fsc.2d outside");
    dump_search(ff, V(0.0, 0.0, 0.0), "fsc.2d corner");
    auto ftet = mk_3d_tetra();
    ftet.sort_cells_into_bins(2, 2, 2);
    dump_search(ftet, V(0.125, 0.125, 0.125), "fsc.tet inside");
    dump_search(ftet, V(0.5, 0.5, 0.5), "fsc.tet outside");

    //============================= fncc: nearest cell centre
    nearest(fs, V(0.25, 0.25, 0.25), "fncc.222 a");
    nearest(fs, V(0.75, 0.75, 0.75), "fncc.222 b");
    nearest(fs, V(5.0, 5.0, 5.0), "fncc.222 far");
    nearest(fs, V(0.25, 0.75, 0.25), "fncc.222 c");
    nearest(fs, V(0.0, 0.0, 0.0), "fncc.222 corner");
    nearest(fs, V(0.5, 0.5, 0.5), "fncc.222 centre tie");
    nearest(hx, V(0.5, 0.5, 0.5), "fncc.hexskew");
    nearest(tt, V(0.5, 0.5, 0.5), "fncc.tri2d");
    auto tg = new ProbeGrid(Grid_t.unstructured_grid, 2, "tie2d");
    size_t[][] tids;
    tids ~= ids(0, 1, 2);
    tids ~= ids(1, 3, 2);
    tg.load([V(0.0, 0.0, 0.0), V(1.0, 0.0, 0.0), V(0.0, 1.0, 0.0), V(1.0, 1.0, 0.0)], 4, 1, 1, tids);
    nearest(tg, V(0.5, 0.5, 0.0), "fncc.tie exact");
    nearest(tg, V(0.375, 0.375, 0.0), "fncc.tie lo");
    nearest(tg, V(0.625, 0.625, 0.0), "fncc.tie hi");
    nearest(fq, V(0.5, 0.5, 0.0), "fncc.flat");
    nearest(mk_1pt(), V(1.0, 1.0, 1.0), "fncc.onept");

    //============================= stl: write_to_stl_file
    string s_tri = "/tmp/probe_g1_tri.stl";
    string s_quad = "/tmp/probe_g1_quad.stl";
    string s_scaled = "/tmp/probe_g1_quad_scaled.stl";
    string s_grid = "/tmp/probe_g1_grid32.stl";
    t2.label = "tri label";
    t2.write_to_stl_file(s_tri);
    writefln("stl.tri len=%d body=%s", len_of(s_tri), body_of(s_tri));
    qt.write_to_stl_file(s_quad);
    writefln("stl.quad len=%d body=%s", len_of(s_quad), body_of(s_quad));
    qt.write_to_stl_file(s_scaled, 2.5);
    writefln("stl.quad scaled len=%d body=%s", len_of(s_scaled), body_of(s_scaled));
    auto g32 = mk_2d_grid32();
    g32.write_to_stl_file(s_grid, 0.5);
    writefln("stl.grid32 len=%d body=%s", len_of(s_grid), body_of(s_grid));
    tryit("stl.3D", { hx.write_to_stl_file("/tmp/probe_g1_3d.stl"); });
    tryit("stl.nvtx=5", { mk_2d_pent().write_to_stl_file("/tmp/probe_g1_pent.stl"); });
    tryit("stl.nvtx=2", { fq.write_to_stl_file("/tmp/probe_g1_flat.stl"); });
    string s_nan = "/tmp/probe_g1_nan.stl";
    auto gn = mk_2d_nan();
    gn.write_to_stl_file(s_nan);
    writefln("stl.nan len=%d body=%s", len_of(s_nan), body_of(s_nan));
    gn.write_to_stl_file(s_nan, 2.5);
    writefln("stl.nan scaled len=%d body=%s", len_of(s_nan), body_of(s_nan));

    //============================= vtk: the abstract writer through a handle
    string v_hex = "/tmp/probe_g1_hex.vtk";
    string v_quad = "/tmp/probe_g1_q2.vtk";
    hg.write_to_vtk_file(v_hex);
    writefln("vtk.222 len=%d body=%s", len_of(v_hex), body_of(v_hex));
    Grid hq = q2;
    hq.write_to_vtk_file(v_quad);
    writefln("vtk.quad len=%d body=%s", len_of(v_quad), body_of(v_quad));

    //============================= rot: rotate(Quaternion, ctr)
    auto r0 = mk_3d_hex_unit();
    r0.rotate([1.0, 0.0, 0.0, 0.0]);
    dump_verts(r0, "rot.identity");
    auto r1 = mk_3d_hex_unit();
    r1.rotate([0.0, 0.0, 0.0, 1.0]);
    dump_verts(r1, "rot.180 z");
    auto r2 = mk_3d_hex_unit();
    r2.rotate([0.7071067811865476, 0.0, 0.0, 0.7071067811865476]);
    dump_verts(r2, "rot.90 z");
    auto r3 = mk_3d_hex_unit();
    r3.rotate([0.7071067811865476, 0.7071067811865476, 0.0, 0.0]);
    dump_verts(r3, "rot.90 x");
    auto r4 = mk_3d_hex_unit();
    r4.rotate([0.0, 0.0, 1.0, 0.0], V(0.5, 0.5, 0.5));
    dump_verts(r4, "rot.180 y ctr");
    auto r5 = mk_3d_hex_unit();
    r5.rotate([0.5, 0.5, 0.5, 0.5], V(0.25, -0.5, 1.0));
    dump_verts(r5, "rot.q half ctr");
    auto r5b = mk_2d_quad();
    r5b.rotate([0.5, 0.5, 0.5, 0.5]);
    dump_verts(r5b, "rot.q half quad2d");
    Quaternion qnan;
    writefln("rot.qinit=%s", qnan[]);
    auto r6 = mk_3d_hex_unit();
    r6.rotate(qnan);
    dump_verts(r6, "rot.q nan");
    auto r7 = mk_3d_hex_unit();
    tryit("rot norm 2", { r7.rotate([2.0, 0.0, 0.0, 0.0]); });
    dump_verts(r7, "rot norm 2 unchanged");
    tryit("rot norm near 0.9999", { mk_3d_hex_unit().rotate([0.9999, 0.0, 0.0, 0.0]); });
    tryit("rot norm zero", { mk_3d_hex_unit().rotate([0.0, 0.0, 0.0, 0.0]); });
    tryit("rot norm just inside", { mk_3d_hex_unit().rotate([1.000000001, 0.0, 0.0, 0.0]); });
    tryit("rot norm just outside", { mk_3d_hex_unit().rotate([1.00000002, 0.0, 0.0, 0.0]); });
    tryit("rot norm negative", { mk_3d_hex_unit().rotate([-1.0, 0.0, 0.0, 0.0]); });

    //============================= rot: rotate(theta, axis, ctr)
    auto a0 = mk_3d_hex_unit();
    a0.rotate(0.0, V(0.0, 0.0, 1.0));
    dump_verts(a0, "rot.a identity");
    auto a1 = mk_3d_hex_unit();
    a1.rotate(3.141592653589793, V(0.0, 0.0, 1.0));
    dump_verts(a1, "rot.a pi z");
    auto a2 = mk_3d_hex_unit();
    a2.rotate(1.5707963267948966, V(1.0, 0.0, 0.0));
    dump_verts(a2, "rot.a halfpi x");
    auto a3 = mk_3d_hex_unit();
    a3.rotate(1.0471975511965976, V(1.0, 1.0, 1.0), V(0.5, 0.5, 0.5));
    dump_verts(a3, "rot.a thirdpi diag ctr");
    auto a4 = mk_3d_hex_unit();
    a4.rotate(2.356194490192345, V(0.0, 0.0, 2.0));
    dump_verts(a4, "rot.a 3pi/4 unnormalized axis");
    auto a5 = mk_3d_hex_unit();
    tryit("rot.a pi zero axis", { a5.rotate(3.141592653589793, V(0.0, 0.0, 0.0)); });
    dump_verts(a5, "rot.a pi zero axis unchanged");
    tryit("rot.a nan theta", { mk_3d_hex_unit().rotate(double.nan, V(1.0, 0.0, 0.0)); });
    Vector3 ax = V(0.0, 2.0, 0.0);
    auto a6 = mk_3d_hex_unit();
    a6.rotate(1.5707963267948966, ax);
    dump3("rot.a axis after call", ax);
    dump_verts(a6, "rot.a axis len2");
    auto a7 = mk_3d_hex_unit();
    a7.rotate(-1.5707963267948966, V(0.0, 1.0, 0.0));
    dump_verts(a7, "rot.a minus halfpi y");

    // One grid exercised through both writers, with a label that needs escaping.
    auto both = mk_2d_quad();
    both.label = "both\"label";
    string s_both = "/tmp/probe_g1_both.stl";
    string v_both = "/tmp/probe_g1_both.vtk";
    both.write_to_stl_file(s_both);
    both.write_to_vtk_file(v_both);
    writefln("both.stl len=%d body=%s", len_of(s_both), body_of(s_both));
    writefln("both.vtk len=%d body=%s", len_of(v_both), body_of(v_both));
    writer_log.length = 0;
    both.write("/tmp/probe_g1_both.gz.txt", "gziptext");
    writefln("both.log=%s", writer_log);
    writefln("both.gz body=%s", body_of("/tmp/probe_g1_both.gz.txt"));

    foreach (path; [wg, wb, wu, we, wv, s_tri, s_quad, s_scaled, s_grid, s_nan, v_hex,
                   v_quad, s_both, v_both, "/tmp/probe_g1_both.gz.txt"]) {
        std.file.remove(path);
    }
}
