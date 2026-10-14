// geom/grid/grid.d -> src/geom/grid/grid.cpp (module geom.grid.grid)
//
// Definitions are kept in the order they appear in the D source (D:22 through D:488).
// See src/geom/grid/grid.hpp for the non-1:1 notes; the ones that matter most here are
// the D assert texts (D:73-75, :83), the signed/unsigned clamp of the bin indices
// (D:252-253, :261-262, :272-273) and the `default:` arm of Grid.write (D:109), which
// goes to the gzip writer rather than to an error.
//
// Measured through probes/geom/grid/probe_grid.d and replayed row for row by
// tests/test_geom_grid_grid.cpp against
// baselines/e587b235/probes/geom_grid_grid.txt.

#include "geom/grid/grid.hpp"

#include <cerrno>
#include <cmath>
#include "eil/math.hpp"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>

#include "eil/array.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/number.hpp"
#include "geom/elements/properties.hpp"
#include "geom/elements/projection.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/geometry_exception.hpp"

namespace {

// D's `assert(cond, "msg")` reaches the caller as core.exception.AssertError whose
// .msg is exactly "msg", and a bare `assert(cond)` with .msg == "Assertion failure"
// (measured: probe rows idx.1d si(0,1) -> "index j is invalid" and idx.ijk 3d(27) ->
// "Assertion failure"). grid.d's four contract asserts route through these two
// helpers, which forward to the substrate's eil::d_assert / eil::d_assert_msg; both
// stay live in a release build,
// as eil_assert() does and as D's solver requires. The recorded file/line necessarily
// names grid.cpp, not grid.d.
// Thin forwarders: substrate/include/eil/error.hpp now owns the measured rule
// (T-918), and two copies of a transcendental-or-message rule drift -- see the T-910
// history in reports/task-log.md. Call sites keep naming the file/line of the throw.
void d_assert_msg(bool cond, const char* msg, const char* file, size_t line) {
    ::eil::d_assert_msg(cond, msg, file, line);
}
void d_assert_bare(bool cond, const char* file, size_t line) {
    ::eil::d_assert(cond, file, line);
}

// D's `max(0, min(nbx-1, ix))` (D:253, :262, :273) mixes a size_t bound with an int
// index. Measured (probe rows bin.clamp) that D's min() result type is `int` and that
// the comparison is signed, so the bound is brought into int. Promoting ix to unsigned
// instead would send a point left of the bounding box to bin nbx-1.
inline int clamp_bin_index(int ix, size_t n) {
    return std::max(0, std::min(static_cast<int>(n - 1), ix));
}

} // namespace

namespace eil {
namespace geom {

// D:24-30
std::string gridTypeName(Grid_t gt) {
    switch (gt) {
      case Grid_t::structured_grid: return "structured_grid";
      case Grid_t::unstructured_grid: return "unstructured_grid";
    }
    // D's `final switch` has no default arm: an enumerator outside the declared set
    // raises core.exception.SwitchError rather than yielding a name. Measured wording
    // with ldc2 1.40.0 on `f(cast(Grid_t)5)` over both out-of-range values tried:
    // .msg == "No appropriate switch clause found" (the value is not in the text). Not
    // a replay row, because D's own toString() appends the switch's source line.
    throw eil::ErrorBase("No appropriate switch clause found", __FILE__, __LINE__);
}

// D:32-39. A D Error, not an Exception: `catch (ExceptionBase)` must not swallow it.
Grid_t gridTypeFromName(const std::string& name) {
    if (name == "structured_grid") return Grid_t::structured_grid;
    if (name == "unstructured_grid") return Grid_t::unstructured_grid;
    throw eil::ErrorBase("Unknown type of grid: " + name);
}

// D:62-67. Only these three fields are assigned; the rest keep D's .init, which the
// header records (deltax/deltay/deltaz and bb0/bb1 are NaN before the first sort).
Grid::Grid(Grid_t grid_type, int dimensions, const std::string& label)
    : grid_type(grid_type), dimensions(dimensions), label(label) {}

// D:70-79
size_t Grid::single_index(size_t i, size_t j, size_t k) const {
    d_assert_msg(i < niv, "index i is invalid", __FILE__, __LINE__);
    d_assert_msg(j < njv, "index j is invalid", __FILE__, __LINE__);
    d_assert_msg(k < nkv, "index k is invalid", __FILE__, __LINE__);
    return i + niv * (j + njv * k);
}

// D:81-91
eil::Array<size_t> Grid::ijk_indices(size_t indx) const {
    d_assert_bare(indx < vertices.length(), __FILE__, __LINE__);
    size_t k = indx / (niv * njv);
    indx -= k * (niv * njv);
    size_t j = indx / niv;
    indx -= j * niv;
    eil::Array<size_t> out;
    out.append(indx);
    out.append(j);
    out.append(k);
    return out;
}

// D:104-111. Note that the `default:` arm is the gzip writer, so an unknown format
// silently writes gzip text (measured: rows wr.unknown, wr.empty, wr.vtkfmt).
void Grid::write(const std::string& fileName, const std::string& fmt) {
    if (fmt == "rawbinary") {
        write_to_raw_binary_file(fileName);
    } else {
        write_to_gzip_file(fileName);
    }
}

// D:124-176
void Grid::compute_cell_properties(size_t indx, bool true_centroid, Vector3& centroid,
                                   number& volume) {
    // Declared uninitialised in D (D:127) and never read back: they are .init (NaN)
    // here too, and only the properties functions write through them.
    number iLen = eil::d_init<number>(), jLen = eil::d_init<number>(),
           kLen = eil::d_init<number>(), Lmin = eil::d_init<number>();
    std::array<size_t, 8> vtx_ids{};
    size_t nvtx = 0;
    copy_vtx_id_list_for_cell(vtx_ids, nvtx, indx);

    if (dimensions == 2) {
        switch (nvtx) {
          case 3:
            xyplane_triangle_cell_properties(vertices[vtx_ids[0]], vertices[vtx_ids[1]],
                                             vertices[vtx_ids[2]], centroid, volume, iLen,
                                             jLen, Lmin);
            return;
          case 4:
            xyplane_quad_cell_properties(vertices[vtx_ids[0]], vertices[vtx_ids[1]],
                                         vertices[vtx_ids[2]], vertices[vtx_ids[3]], centroid,
                                         volume, iLen, jLen, Lmin);
            return;
          default:
            throw eil::ErrorBase("Unhandled number of vertices.");
        } // end switch
    } // end: if 2D
    // else, 3D
    switch (nvtx) {
      case 4:
        tetrahedron_properties(vertices[vtx_ids[0]], vertices[vtx_ids[1]],
                               vertices[vtx_ids[2]], vertices[vtx_ids[3]], centroid, volume);
        return;
      case 8:
        hex_cell_properties(vertices[vtx_ids[0]], vertices[vtx_ids[1]], vertices[vtx_ids[2]],
                            vertices[vtx_ids[3]], vertices[vtx_ids[4]], vertices[vtx_ids[5]],
                            vertices[vtx_ids[6]], vertices[vtx_ids[7]], true_centroid, centroid,
                            volume, iLen, jLen, kLen);
        return;
      case 5:
        pyramid_properties(vertices[vtx_ids[0]], vertices[vtx_ids[1]], vertices[vtx_ids[2]],
                           vertices[vtx_ids[3]], vertices[vtx_ids[4]], true_centroid, centroid,
                           volume);
        return;
      case 6:
        wedge_properties(vertices[vtx_ids[0]], vertices[vtx_ids[1]], vertices[vtx_ids[2]],
                         vertices[vtx_ids[3]], vertices[vtx_ids[4]], vertices[vtx_ids[5]],
                         true_centroid, centroid, volume);
        return;
      default:
        throw eil::ErrorBase("Unhandled number of vertices.");
    } // end switch
} // end compute_cell_properties()

// D:178-229
bool Grid::point_is_inside_cell(const Vector3& p, size_t i) {
    bool inside_cell = false;
    std::array<size_t, 8> vtx_id{};
    size_t nvtx = 0;
    copy_vtx_id_list_for_cell(vtx_id, nvtx, i);
    switch (dimensions) {
      case 1:
        EIL_THROW_GEOM("cell search not implemented for 1D grids");
      case 2:
        switch (nvtx) {
          case 3:
            inside_cell = inside_xy_triangle(vertices[vtx_id[0]], vertices[vtx_id[1]],
                                             vertices[vtx_id[2]], p);
            break;
          case 4:
            inside_cell = inside_xy_quad(vertices[vtx_id[0]], vertices[vtx_id[1]],
                                         vertices[vtx_id[2]], vertices[vtx_id[3]], p);
            break;
          default:
            EIL_THROW_GEOM("invalid cell type in 2D");
        } // end switch (vtx_id.length)
        break;
      case 3:
        switch (nvtx) {
          case 4:
            inside_cell = inside_tetrahedron(vertices[vtx_id[0]], vertices[vtx_id[1]],
                                             vertices[vtx_id[2]], vertices[vtx_id[3]], p);
            break;
          case 8:
            inside_cell = inside_hexahedron(vertices[vtx_id[0]], vertices[vtx_id[1]],
                                            vertices[vtx_id[2]], vertices[vtx_id[3]],
                                            vertices[vtx_id[4]], vertices[vtx_id[5]],
                                            vertices[vtx_id[6]], vertices[vtx_id[7]], p);
            break;
          case 5:
            inside_cell = inside_pyramid(vertices[vtx_id[0]], vertices[vtx_id[1]],
                                         vertices[vtx_id[2]], vertices[vtx_id[3]],
                                         vertices[vtx_id[4]], p);
            break;
          case 6:
            inside_cell = inside_wedge(vertices[vtx_id[0]], vertices[vtx_id[1]],
                                       vertices[vtx_id[2]], vertices[vtx_id[3]],
                                       vertices[vtx_id[4]], vertices[vtx_id[5]], p);
            break;
          default:
            EIL_THROW_GEOM("invalid cell type in 3D");
        } // end switch (vtx_id.length)
        break;
      default:
        EIL_THROW_GEOM("invalid number of dimensions");
    } // end switch (dimensions)
    return inside_cell;
} // end point_is_inside_cell()

// D:231-241. The "centre-of-mass" of the vertices defining the cell.
Vector3 Grid::cell_barycentre(size_t indx) {
    std::array<size_t, 8> vtx_ids{};
    size_t nvtx = 0;
    copy_vtx_id_list_for_cell(vtx_ids, nvtx, indx);
    Vector3 cbc;
    cbc.set(vertices[vtx_ids[0]]);
    for (size_t i = 1; i < nvtx; ++i) { cbc.add(vertices[vtx_ids[i]]); }
    cbc.scale(1.0 / nvtx);
    return cbc;
} // end cell_barycentre()

// Bin-sorting methods to speed up the search for the enclosing cell.

// D:247-254. Since we cannot control what position we are given, the bin index is
// determined cautiously. Note that `cast(int)` of a non-finite or out-of-range double
// is undefined in both languages (ldc2 happens to give -2147483648) and is deliberately
// not an observable of the replay corpus; see the bin.clamp/bin.trunc rows.
int Grid::get_bin_ix(const Vector3& p) {
    double dx = eil::re(p.x) - eil::re(bb0.x);
    int ix = static_cast<int>(dx / deltax);
    ix = clamp_bin_index(ix, nbx);
    return ix;
}

// D:256-263
int Grid::get_bin_iy(const Vector3& p) {
    double dy = eil::re(p.y) - eil::re(bb0.y);
    int iy = static_cast<int>(dy / deltay);
    iy = clamp_bin_index(iy, nby);
    return iy;
}

// D:265-275
int Grid::get_bin_iz(const Vector3& p) {
    int iz = 0;
    if (dimensions == 3) {
        double dz = eil::re(p.z) - eil::re(bb0.z);
        iz = static_cast<int>(dz / deltaz);
        iz = clamp_bin_index(iz, nbz);
    }
    return iz;
}

// D:277-317. A method rather than part of the constructor, because a moving grid may
// need it called more than once — and note that a second call does NOT clear the bins:
// `bins.length = nbx` with an unchanged length keeps the contents (measured, rows
// bin.twice and bin.thrice shrunk).
void Grid::sort_cells_into_bins(size_t nbinx, size_t nbiny, size_t nbinz) {
    if (dimensions == 2) { nbinz = 1; }
    // Numbers of bins in each coordinate direction.
    if (nbinx < 1 || nbiny < 1 || nbinz < 1) {
        EIL_THROW_GEOM("Need to have nbinx, nbiny and nbinz >= 1");
    }
    nbx = nbinx; nby = nbiny; nbz = nbinz;
    // Determine the bounding box for the grid. Note the loop bound is nvertices, not
    // vertices.length (measured: row bin.nvtx truncated).
    Vector3 p = vertices[0];
    bb0 = p; bb1 = p;
    for (size_t i = 1; i < nvertices; ++i) {
        p = vertices[i];
        bb0.set(std::fmin(eil::re(p.x), eil::re(bb0.x)), std::fmin(eil::re(p.y), eil::re(bb0.y)),
                std::fmin(eil::re(p.z), eil::re(bb0.z)));
        bb1.set(std::fmax(eil::re(p.x), eil::re(bb1.x)), std::fmax(eil::re(p.y), eil::re(bb1.y)),
                std::fmax(eil::re(p.z), eil::re(bb1.z)));
    }
    // Sizes of the bins.
    deltax = (eil::re(bb1.x) - eil::re(bb0.x)) / nbx;
    deltay = (eil::re(bb1.y) - eil::re(bb0.y)) / nby;
    deltaz = (eil::re(bb1.z) - eil::re(bb0.z)) / nbz;
    const double very_small_delta = 1.0e-6;
    deltax = std::fmax(deltax, very_small_delta);
    deltay = std::fmax(deltay, very_small_delta);
    deltaz = std::fmax(deltaz, very_small_delta);
    // Now, set up the array of bins and sort the cells into those bins.
    bins.set_length(nbx);
    for (size_t ix = 0; ix < nbx; ++ix) {
        bins[ix].set_length(nby);
        for (size_t iy = 0; iy < nby; ++iy) { bins[ix][iy].set_length(nbz); }
    }
    for (size_t i = 0; i < ncells; ++i) {
        p = cell_barycentre(i);
        int ix = get_bin_ix(p);
        int iy = get_bin_iy(p);
        int iz = get_bin_iz(p);
        bins[ix][iy][iz].append(i);
    }
    cells_are_sorted_into_bins = true;
} // end sort_cells_into_bins()

// D:319-352
void Grid::find_enclosing_cell_fast(const Vector3& p, size_t& indx, bool& found) {
    // Search for the cell only in the near-by bins.
    found = false; indx = 0;
    if (!inside_bounding_box(p, bb0, bb1, dimensions)) return;
    //
    // So, the point is inside or on the grid's bounding box; look more closely.
    // Pick the most-likely bin, based on position.
    int ix0 = get_bin_ix(p);
    int iy0 = get_bin_iy(p);
    int iz0 = get_bin_iz(p);
    // Visit that bin first.
    for (size_t t = 0; t < bins[ix0][iy0][iz0].length(); ++t) {
        size_t i = bins[ix0][iy0][iz0][t];
        if (point_is_inside_cell(p, i)) { found = true; indx = i; return; }
    }
    // If we reach this point, extend the search one bin further in each direction.
    int ix_start = std::max(ix0 - 1, 0);
    int ix_end = std::min(ix0 + 1, static_cast<int>(nbx - 1));
    int iy_start = std::max(iy0 - 1, 0);
    int iy_end = std::min(iy0 + 1, static_cast<int>(nby - 1));
    int iz_start = std::max(iz0 - 1, 0);
    int iz_end = std::min(iz0 + 1, static_cast<int>(nbz - 1));
    for (int iz = iz_start; iz <= iz_end; ++iz) {
        for (int iy = iy_start; iy <= iy_end; ++iy) {
            for (int ix = ix_start; ix <= ix_end; ++ix) {
                // Search only bins we have not already visited.
                if (ix == ix0 && iy == iy0 && iz == iz0) continue;
                for (size_t t = 0; t < bins[ix][iy][iz].length(); ++t) {
                    size_t i = bins[ix][iy][iz][t];
                    if (point_is_inside_cell(p, i)) { found = true; indx = i; return; }
                }
            }
        }
    }
    // If we arrive here, give up anyway and look no further.
    return;
} // end find_enclosing_cell_fast()

// D:354-363
void Grid::find_enclosing_cell_slow(const Vector3& p, size_t& indx, bool& found) {
    // Search every cell in the block.
    found = false; indx = 0;
    for (size_t i = 0; i < ncells; ++i) {
        if (point_is_inside_cell(p, i)) { found = true; indx = i; break; }
    }
    return;
} // end find_enclosing_cell_slow()

// D:365-373
void Grid::find_enclosing_cell(const Vector3& p, size_t& indx, bool& found) {
    if (cells_are_sorted_into_bins) {
        find_enclosing_cell_fast(p, indx, found);
    } else {
        find_enclosing_cell_slow(p, indx, found);
    }
} // end find_enclosing_cell()

// D:375-386
void Grid::find_nearest_cell_centre(const Vector3& p, size_t& nearestCell, double& minDist) {
    Vector3 c = cell_barycentre(0);
    double d = distance_between(c, p);
    minDist = d; nearestCell = 0;
    for (size_t i = 1; i < ncells; ++i) {
        c = cell_barycentre(i);
        d = distance_between(c, p);
        // Strict `<`, so on an exact tie the earlier cell keeps the slot (measured:
        // rows fncc.tie exact / lo / hi).
        if (d < minDist) { minDist = d; nearestCell = i; }
    }
} // end find_nearest_cell_centre

// D:388-433. STL (ASCII) output for 2D grids. D's nested write_STL_triangle closes
// over the file handle and `scale` (D:399-410), so it is a capturing lambda here.
void Grid::write_to_stl_file(const std::string& fileName, double scale) {
    if (dimensions != 2) {
        // D:395 `throw new Exception(...)` — an Exception, not the Errors used
        // elsewhere in this module (measured: row stl.3D comes back tagged `exc=`).
        throw eil::ExceptionBase("write_to_stl_file is only for 2D grids.", __FILE__,
                                 __LINE__);
    }
    // D's std.stdio.File closes (and so flushes) in its destructor, which runs even
    // when the body below throws — the mid-write partial file is what a user finds on
    // disk. A bare FILE* would leak the handle and lose those bytes, so the guard
    // restores the destructor scope (same shape as
    // src/geom/surface/parametricsurface.cpp:52-70).
    std::FILE* raw = std::fopen(fileName.c_str(), "w");
    if (raw == nullptr) {
        throw eil::ExceptionBase("Cannot open file `" + fileName + "' in mode `w' (" +
                                     std::strerror(errno) + ")",
                                 __FILE__, __LINE__);
    }
    struct File_guard {
        std::FILE* h;
        ~File_guard() { if (h) std::fclose(h); }
    } guard{raw};
    std::FILE* f = guard.h;
    // D's File.writefln is format-then-newline.
    auto wf = [&f](const std::string& s) {
        std::fwrite(s.data(), 1, s.size(), f);
        std::fwrite("\n", 1, 1, f);
    };
    wf(eil::format("solid %s", label));
    auto write_STL_triangle = [&wf, &scale](const Vector3& p0, const Vector3& p1,
                                            const Vector3& p2) {
        Vector3 p01 = p1 - p0;
        Vector3 p02 = p2 - p0;
        Vector3 n;
        cross(n, p01, p02);
        n.normalize();
        wf(eil::format("facet normal %.6e %.6e %.6e", eil::re(n.x), eil::re(n.y), eil::re(n.z)));
        wf("  outer loop");
        wf(eil::format("    vertex %.6e %.6e %.6e", eil::re(p0.x) * scale, eil::re(p0.y) * scale,
                       eil::re(p0.z) * scale));
        wf(eil::format("    vertex %.6e %.6e %.6e", eil::re(p1.x) * scale, eil::re(p1.y) * scale,
                       eil::re(p1.z) * scale));
        wf(eil::format("    vertex %.6e %.6e %.6e", eil::re(p2.x) * scale, eil::re(p2.y) * scale,
                       eil::re(p2.z) * scale));
        wf("  endloop");
        wf("endfacet");
    };
    std::array<size_t, 8> vtx_ids{};
    size_t nvtx = 0; // to suit copy_vtx_id_list_for_cell()
    for (size_t i = 0; i < ncells; ++i) {
        copy_vtx_id_list_for_cell(vtx_ids, nvtx, i);
        switch (nvtx) {
          case 3: {
            // Single triangle.
            write_STL_triangle(vertices[vtx_ids[0]], vertices[vtx_ids[1]],
                               vertices[vtx_ids[2]]);
            break;
          }
          case 4: {
            // Quad, split into 4 triangles.
            Vector3 mid = cell_barycentre(i);
            write_STL_triangle(vertices[vtx_ids[0]], vertices[vtx_ids[1]], mid);
            write_STL_triangle(vertices[vtx_ids[1]], vertices[vtx_ids[2]], mid);
            write_STL_triangle(vertices[vtx_ids[2]], vertices[vtx_ids[3]], mid);
            write_STL_triangle(vertices[vtx_ids[3]], vertices[vtx_ids[0]], mid);
            break;
          }
          default:
            // A D Error (D:428), so eil::ErrorBase — and the partial file above stays
            // on disk, as it does in D, because the guard closes the handle.
            throw eil::ErrorBase(eil::format("Unexpected number of vertices, nvtx=%d.", nvtx));
        } // end switch nvtx
    } // end foreach cell
    wf(eil::format("endsolid %s", label));
    // D's `f.close()` (D:432) is the guard's job here; the bytes on disk are the same.
} // end write_to_STL_file()

/**
 * Rotate the vertices of a grid given a unit quaternion and centre of rotation.
 *
 * The quaternion should be a unit quaternion of the form:
 * q = q0 + q1 i + q2 j + q3 k
 *
 * Authors: Flynn Hack and RJG
 */
// D:435-471
void Grid::rotate(Quaternion q, Vector3 rotation_ctr) {
    auto norm = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
    if (std::fabs(norm - 1.0) > 1.0e-8) {
        // Note that an all-NaN quaternion (D's `Quaternion q;`, four times
        // double.init) gets past this test, because NaN comparisons are false —
        // measured: row rot.q nan rotates to all-NaN vertices rather than throwing.
        throw eil::ExceptionBase(
            "grid.rotate: Invalid quaternion provided. Should be unit quaternion with "
            "norm = 1",
            __FILE__, __LINE__);
    }

    // D:450-451 `double[] Rmat; Rmat.length = 9;` — a positive literal length, so
    // backlog T-914's signed `arr.length = L` case does not arise here.
    eil::Array<double> Rmat;
    Rmat.set_length(9);
    Rmat[0] = q[0] * q[0] + q[1] * q[1] - q[2] * q[2] - q[3] * q[3];
    Rmat[1] = 2 * q[1] * q[2] - 2 * q[0] * q[3];
    Rmat[2] = 2 * q[1] * q[3] + 2 * q[0] * q[2];
    Rmat[3] = 2 * q[1] * q[2] + 2 * q[0] * q[3];
    Rmat[4] = q[0] * q[0] - q[1] * q[1] + q[2] * q[2] - q[3] * q[3];
    Rmat[5] = 2 * q[2] * q[3] - 2 * q[0] * q[1];
    Rmat[6] = 2 * q[1] * q[3] - 2 * q[0] * q[2];
    Rmat[7] = 2 * q[2] * q[3] + 2 * q[0] * q[1];
    Rmat[8] = q[0] * q[0] - q[1] * q[1] - q[2] * q[2] + q[3] * q[3]; // Implementation
                                                                     // checked against
                                                                     // scipy rotation package.

    // D's `foreach (ref v; vertices)` writes through, over vertices.length (not
    // nvertices).
    for (size_t t = 0; t < vertices.length(); ++t) {
        Vector3& v = vertices[t];
        // Translate all points so that centre is at origin.
        v = v - rotation_ctr;
        // Apply rotation
        v.apply_matrix_transform(Rmat.all());
        // Translate all points so that centre is where we started.
        v = v + rotation_ctr;
    }
}

/**
 * Rotate the vertices of a grid given a rotation angle, an axis of rotation and a
 * centre of rotation.
 *
 * This method converts to quaternion representation and then delegates to the method
 * that accepts a quaternion.
 *
 * Authors: Flynn Hack and RJG
 */
// D:473-488
void Grid::rotate(double theta, Vector3 rotation_axis, Vector3 rotation_ctr) {
    auto C = eil::cos(theta / 2.0);
    auto S = eil::sin(theta / 2.0);
    // rotation_axis is a by-value copy of the caller's Vector3, as in D, so this
    // normalize() is not visible to the caller (measured: row rot.a axis after call).
    rotation_axis.normalize();
    Quaternion q = {C, S * eil::re(rotation_axis.x), S * eil::re(rotation_axis.y),
                    S * eil::re(rotation_axis.z)};
    rotate(q, rotation_ctr);
}

} // namespace geom
} // namespace eil
