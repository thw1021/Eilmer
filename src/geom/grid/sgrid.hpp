#pragma once
// geom/grid/sgrid.d -> src/geom/grid/sgrid.hpp (module geom.grid.sgrid)
//
// StructuredGrid: the i,j,k lattice grid (sgrid.d:28-1467) plus the module-level
// helpers of sgrid.d:1472-1828 (rs_grids_to_list, list_to_rs_grids, uflowz,
// read_VTK_header_line, import_gridpro_grid, importPlot3DGrid, writeGridsAsPlot3D,
// rotate_gridpro_blocks, grid_faceswap).
//
// Measured against ldc2 1.40.0 at gdtk e587b235 through
// probes/geom/grid/probe_sgrid.d; its captured corpus
// baselines/e587b235/probes/geom_grid_sgrid.txt (2097 rows) is replayed row for
// row by tests/test_geom_grid_sgrid.cpp. sgrid.d has one `unittest` block
// (sgrid.d:1833-1865); its lines are in the corpus as sg.unit.*.
//
// THE CYCLE (grid brief sections 8 and 10.1): sgrid.d:1313-1318's
// write_to_su2_file body is `auto usg = new UnstructuredGrid(this);
// usg.write_to_su2_file(...)`, while usgrid.d:553 takes `const StructuredGrid`
// and reads its members. The two headers therefore forward-declare each other and
// each .cpp includes the partner, so only the .cpp files see both complete types.
//
// Non-1:1 notes (registered in provenance/fidelity.yml):
//   * adapted-handles: D's class handles (StructuredGrid, ParametricSurface,
//     ParametricVolume, Path, UnivariateFunction) are held through
//     std::shared_ptr, so `dup()`, `subgrid()`, `makeSlabGrid()`, `makeWedgeGrid()`
//     and `get_boundary_grid()` return shared_ptr, and the surface/volume/path
//     constructor parameters take the object by const reference (the house
//     convention, src/geom/surface/parametricsurface.cpp:48,
//     src/geom/volume/tfivolume.hpp:115). Call sites pass `*handle`.
//   * Import of `import geom;` (sgrid.d:24) is replaced by the specific module
//     headers actually used (wave-02-brief section 4 item 5): grid, elements,
//     gpath/path, misc/univariatefunctions, surface/parametricsurface,
//     volume/parametricvolume, nm/nelmin, extern/gzip.
//   * `arr.length = L` with a SIGNED L (sgrid.d:611? no — the real sites are
//     D:60, :887, :911, :958, :981 and the read-from-text/raw paths) goes through
//     eil::Array::set_length_signed (T-914); corpus row sg.frmt.nivbig is exactly
//     this path and carries D's OutOfMemoryError text "Memory allocation failed".
//   * 15 `^^` sites (grid brief section 9.3) become eil::pow, EXCEPT an exponent
//     of 0.5 which would become eil::sqrt (T-925). Measured on this module: sgrid.d
//     has no `^^0.5` at all (the 15 sites are :664-670 cubic-Bezier weights, :1367
//     perimeter^^2), so every site here is eil::pow and none is expanded into
//     multiplication.
//   * sgrid.d:1299 and :1304 pair cos(theta)/sin(theta) of one argument, and
//     :1294 has atan2 — all three go to the substrate wrappers (T-920, T-910), as
//     briefed.
//   * Throw sites follow T-922 per D's class, not per convenience: sgrid.d has 29
//     bare `throw new Error(...)` (-> eil::ErrorBase with the message ONLY) and 12
//     GeometryException (-> EIL_THROW_GEOM, which carries the site) plus 1
//     Exception at D:1439 (-> eil::ExceptionBase with the site).
//   * sgrid.d:345 `case 3: ... nkc = njv-1;` in get_vtx_id_list_for_cell(indx) is a
//     typo in the reference (it should be nkv-1) and sgrid.d:352 then calls the
//     (i,j,k) form with nkc unused. Kept verbatim; corpus rows sg.idx.* exercise it.
//   * sgrid.d:1337 `else { b.set(*this[i,j-1,k]); }` writes b, not c, in the
//     non-orthogonality loop of measure_of_badness (so c keeps its NaN .init for the
//     last row). Kept verbatim; it is observable through measure_of_badness.
//   * sgrid.d:1552 read_VTK_header_line is called from usgrid.d:1790 as well, via
//     D's `import geom;`. The port exposes it here (declared at module scope, as in
//     D) and usgrid.cpp includes this header for it.
//   * sgrid.d:1756-1828 grid_faceswap returns its uninitialised handle when both
//     swap flags are false (D:1804/:1817 leave `new_grid` null) — the port returns
//     an empty shared_ptr, which is the same observable null.
//   * The `version(complex_numbers)` axis is not compiled (backlog T-904); `number`
//     is `double`, so D's `p.x.re` reads as `eil::re(p.x)` and `.re` on a double is
//     dropped.

#include <array>
#include <cstddef>
#include <cstdio>
#include <memory>
#include <string>

#include "eil/aa.hpp"
#include "eil/array.hpp"
#include "eil/format.hpp"
#include "eil/number.hpp"
#include "geom/elements/nomenclature.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/grid/grid.hpp"
#include "geom/gpath/path.hpp"
#include "geom/misc/univariatefunctions.hpp"
#include "geom/surface/parametricsurface.hpp"
#include "geom/volume/parametricvolume.hpp"

namespace eil {
namespace geom {

// The partner of the mutual forward declaration (usgrid.hpp declares this the same
// way round). Only pointers/references appear in this header.
class UnstructuredGrid;

using UnivariateFunctionPtr = std::shared_ptr<UnivariateFunction>;
using ClusterF = eil::Array<UnivariateFunctionPtr>;
// D's `const(double[4][4])` — a static array of four static arrays of four doubles.
using RSGrid = std::array<std::array<double, 4>, 4>;

class StructuredGrid : public Grid {
  public:
    eil::Array<size_t> vtx_id;          // D:30 single-index id of each vertex
    eil::Array<std::string> bcTags;     // D:32 one user name per boundary

    // D:35-70 blank grid; dimensions inferred from the vertex counts.
    StructuredGrid(size_t niv, size_t njv, size_t nkv = 1, const std::string& label = "");
    // D:72-79 1D grid on a Path.
    StructuredGrid(const Path& my_path, size_t niv,
                   UnivariateFunctionPtr clusterf = std::make_shared<LinearFunction>(0.0, 1.0),
                   const std::string& label = "");
    // D:81-101 2D grid on a parametric surface.
    StructuredGrid(const ParametricSurface& surf, size_t niv, size_t njv,
                   const ClusterF& clusterf, const std::string& interpolation = "simple",
                   const std::string& label = "");
    // D:103-113 2D grid with internal redistribution of the r,s parameters.
    StructuredGrid(const ParametricSurface& surf, size_t niv, size_t njv,
                   const ClusterF& clusterf, const RSGrid& r_grid, const RSGrid& s_grid,
                   const std::string& label = "");
    // D:115-136 3D grid on a parametric volume.
    StructuredGrid(const ParametricVolume& pvolume, size_t niv, size_t njv, size_t nkv,
                   const ClusterF& clusterf, const std::string& interpolation = "simple",
                   const std::string& label = "");
    // D:138-160 import from a file, format dispatch.
    StructuredGrid(const std::string& fileName, const std::string& fmt,
                   const std::string& label = "");
    // D:162-167 copy constructor.
    explicit StructuredGrid(const StructuredGrid& other);

    std::shared_ptr<StructuredGrid> dup() const;      // D:169-172

    std::string set_tag(size_t boundary_indx, const std::string& new_tag);   // D:174-183
    int set_tags(const eil::AA<std::string, std::string>& tags);             // D:185-204

    // ---- D:206-260: indexing and the type table ----
    Vector3* opIndex(size_t i, size_t j, size_t k = 0) override;   // D:206
    Vector3* opIndex(size_t indx) override;                        // D:216
    std::string toString() const;                         // D:223-232 (no virtual toString in Grid)
    size_t number_of_vertices_for_cell(size_t i) override;         // D:234
    int vtk_element_type_for_cell(size_t i) override;              // D:245
    int get_cell_type(size_t i) override;                          // D:256
    eil::Array<size_t> get_vtx_id_list_for_cell(size_t i, size_t j,
                                               size_t k = 0) const override; // D:262
    void copy_vtx_id_list_for_cell(std::array<size_t, 8>& vtx_list_copy, size_t& nvtx,
                                   size_t i, size_t j,
                                   size_t k = 0) const override;            // D:307
    eil::Array<size_t> get_vtx_id_list_for_cell(size_t indx) const override; // D:339
    void copy_vtx_id_list_for_cell(std::array<size_t, 8>& vtx_list_copy, size_t& nvtx,
                                   size_t indx) const override;             // D:355

    // ---- D:376-578: subsetting and boundaries ----
    std::shared_ptr<StructuredGrid> subgrid(size_t i0, size_t ni, size_t j0 = 0, size_t nj = 1,
                                           size_t k0 = 0, size_t nk = 1) const; // D:376
    std::shared_ptr<Grid> get_boundary_grid(size_t boundary_indx) override;      // D:422
    eil::Array<size_t> get_list_of_boundary_cells(size_t boundary_indx) override; // D:492

    // ---- D:580-781: the grid generators ----
    void make_grid_from_path(const Path& pth, const UnivariateFunctionPtr& clusterf); // D:580
    void make_grid_from_surface(const ParametricSurface& surf, const ClusterF& clusterf); // D:595
    void make_grid_from_surface_bilinear(const ParametricSurface& surf,
                                        const ClusterF& clusterf);                 // D:620
    void make_grid_from_surface(const ParametricSurface& surf, const ClusterF& clusterf,
                                const RSGrid& r_grid, const RSGrid& s_grid);        // D:648
    void make_grid_from_volume(const ParametricVolume& pvolume, const ClusterF& clusterf); // D:690
    void make_grid_from_volume_trilinear(const ParametricVolume& pvolume,
                                        const ClusterF& clusterf);                 // D:734

    // ---- D:783-1102: file input and output ----
    void read_from_text_file(const std::string& fileName, bool vtkHeader = true); // D:783
    void read_from_gzip_file(const std::string& fileName, double scale = 1.0) override;   // D:854
    void read_from_raw_binary_file(const std::string& fileName, double scale = 1.0) override; // D:924
    void write_to_gzip_file(const std::string& fileName) override;                 // D:999
    void write_to_raw_binary_file(const std::string& fileName) override;           // D:1042
    void write_to_vtk_file(const std::string& fileName) override;                  // D:1084
    // D:1312-1318. The whole body delegates to UnstructuredGrid, which is why G4 and
    // G5 are one work package (grid brief section 10.1).
    void write_to_su2_file(const std::string& fileName, double scale = 1.0,
                           bool use_gmsh_order_for_wedges = true) override;        // D:1312

    // ---- D:1104-1310: joining and extruding ----
    void joinGrid(const std::shared_ptr<StructuredGrid>& gridToJoin,
                  const std::string& joinLocation);                                 // D:1130
    std::shared_ptr<StructuredGrid> makeSlabGrid(Vector3 dz, bool symmetric = true,
                                                 const std::string& label = "");    // D:1258
    std::shared_ptr<StructuredGrid> makeSlabGrid(double dz, bool symmetric = true,
                                                 const std::string& label = "");    // D:1277
    std::shared_ptr<StructuredGrid> makeWedgeGrid(double dtheta, bool symmetric = true,
                                                  const std::string& label = "",
                                                  int nkv_new = 2);                 // D:1282

    // ---- D:1320-1465: quality measure, redistribution, rotation ----
    double measure_of_badness();                                                    // D:1320
    bool determine_rs_grids(const ParametricSurface& surf, const ClusterF& clusterf,
                            RSGrid& r_grid, RSGrid& s_grid);                        // D:1396
    void rotateGrid(double q0, double q1, double q2, double q3);                    // D:1425
};

// ---- module-level helpers, in D's declaration order ----
void rs_grids_to_list(const RSGrid& r_grid, const RSGrid& s_grid, eil::Array<double>& data); // D:1472
void list_to_rs_grids(const eil::Array<double>& data, RSGrid& r_grid, RSGrid& s_grid);       // D:1498
double uflowz(double q, double tiny = 1.0e-30, double huge = 1.0e30);                        // D:1534
// D:1544 `string[] read_VTK_header_line(string target, File f)` — also called by
// usgrid.d:1790, so it is declared at module scope here as in D.
eil::Array<std::string> read_VTK_header_line(const std::string& target, std::FILE* f);
using StructuredGridPtr = std::shared_ptr<StructuredGrid>;
using StructuredGridArray = eil::Array<StructuredGridPtr>;

StructuredGridArray import_gridpro_grid(const std::string& fileName, double scale = 1.0);    // D:1565
StructuredGridArray importPlot3DGrid(const std::string& fileName, int dim, double scale = 1.0,
                                     bool is_gzipped = false);                               // D:1614
void writeGridsAsPlot3D(const std::string& fname, const StructuredGridArray& grids, int dim); // D:1711
StructuredGridPtr rotate_gridpro_blocks(StructuredGridPtr grid,
                                        const std::string& rotateSouthToThis = "South",
                                        const std::string& rotateWestToThis = "West");        // D:1756
StructuredGridPtr grid_faceswap(StructuredGridPtr grid, bool swapNorthToSouth = false,
                                bool swapEastToWest = false);                                // D:1800

} // namespace geom
} // namespace eil
