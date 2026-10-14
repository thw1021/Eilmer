#pragma once
// geom/grid/grid.d -> src/geom/grid/grid.hpp (module geom.grid.grid)
//
// The abstract base of the grid hierarchy: StructuredGrid (sgrid.d) and
// UnstructuredGrid (usgrid.d) both derive from this, so the pure-virtual table
// below IS their implementation contract. Measured against ldc2 1.40.0 at gdtk
// e587b235 through probes/geom/grid/probe_grid.d, whose captured corpus
// baselines/e587b235/probes/geom_grid_grid.txt is replayed row for row by
// tests/test_geom_grid_grid.cpp. grid.d has no `unittest` block
// (`grep -c unittest geom/grid/grid.d` -> 0), so that replay is the whole gate.
//
// Non-1:1 notes (registered in provenance/fidelity.yml):
//   * D's `class Grid` is a reference type; ported code holds it through
//     std::shared_ptr<Grid>, and `Grid get_boundary_grid(...)` (D:121) therefore
//     returns std::shared_ptr<Grid> (adapted-handles). The concrete members keep
//     D's value/reference-parameter shapes: `ref Vector3`/`ref number`/`ref size_t`
//     in/out parameters become C++ references, and `ref size_t[8]` becomes
//     std::array<size_t,8>&.
//   * The class fields are PUBLIC here because they are public in D — measured, not
//     assumed: the probe writes `g.vertices`, `g.niv`, `g.nvertices`, `g.dimensions`
//     and `g.label` from `main()`, outside any subclass (probe rows idx.fields,
//     idx.ijk truncated nvertices, bin.nvtx truncated).
//   * `opIndex` (D:93-94) is NOT mapped to `operator[]`, against the wave-02 mapping
//     table. D's multi-index spelling `g[i, j, k]` cannot be written in C++ (the
//     commas become the comma operator), and a 1-argument `operator[]` would be
//     ambiguous with the (i, j, k=0) form. Both overloads keep D's name and are
//     called as `g->opIndex(i, j, k)`. The two overloads are behaviourally identical
//     for a 3-argument call, because single_index(i, 0, 0) == i for every lattice,
//     so which one D picks for `copy_vtx_id_list_for_cell(ids, nv, indx)` (D:129,
//     :183, :236, :413) is not observable; the corpus drives both spellings
//     separately (rows ab.copy(ijk ...), ab.copy(indx ...)).
//   * `size_t[] get_vtx_id_list_for_cell(...)` (D:95, :98) returns a D slice aliasing
//     the subclass' own storage. eil::Array is a value type, so the port returns a
//     copy: the printed content is the same, but a caller cannot write through it.
//   * `bins` is D's 4-deep `size_t[][][][]` (D:57), so it is a 4-deep eil::Array.
//     D's `bins.length = nbx` with an unchanged length KEEPS the nested contents,
//     which is why calling sort_cells_into_bins() twice appends into the same bins
//     instead of clearing them — eil::Array::set_length reproduces that, and
//     probe rows bin.twice / bin.thrice shrunk pin it down.
//   * Uninitialized fields carry D's `.init`, not zero: `double deltax, deltay,
//     deltaz` (D:59) are NaN before the first sort_cells_into_bins() (measured:
//     row bin.unsorted prints deltax=nan and bb0/bb1 as Vector3(nan, nan, nan)),
//     while the size_t counters are 0 and `cells_are_sorted_into_bins` is false
//     (D:60 initialiser).
//   * `assert` texts are D's, not the substrate default. The three contract asserts of
//     grid.d:73-75 have a message and reach the caller as
//     core.exception.AssertError with `.msg == "index i is invalid"` etc.; the bare
//     one at grid.d:83 has `.msg == "Assertion failure"` (all four measured, rows
//     idx.1d si(...) and idx.ijk 3d(27)). `eil_assert()` renders
//     "Assertion failure: <stringified condition>", so these four sites use the
//     message-preserving helpers in grid.cpp instead of rewriting the substrate
//     (reported as a substrate gap for the wave-02 rule `assert(` -> `eil_assert`).
//     `.file()`/`.line()` necessarily name grid.cpp, not grid.d, so they are not in
//     the replay corpus.
//   * Every `throw new Error(...)` maps to eil::ErrorBase with the message ONLY (no
//     file/line), because a bare D object.Error has `.file == ""` and `.line == 0`
//     — measured in the probe (`gt.nocatch ERR ...`). Error and Exception are
//     siblings here, so a ported `catch (ExceptionBase)` does not catch them, exactly
//     as D's `catch (Exception)` does not catch them (row gt.nocatch proves it).
//     `throw new GeometryException(...)`/`new Exception(...)` do carry the throw
//     site's file/line, via EIL_THROW_GEOM / eil::ExceptionBase.
//   * D's `sort_cells_into_bins` (D:277-317) clamps the bin index with
//     `max(0, min(nbx-1, ix))`, mixing size_t with int. Measured (rows bin.clamp):
//     D's chosen result type is `int` and the comparison is SIGNED, so the port
//     converts the size_t bound to int rather than promoting ix to unsigned
//     (promoting would map a point left of the box to bin nbx-1).
//   * `write_to_stl_file` (D:388-433) has a nested function closing over `f` and
//     `scale` (D:399-410) -> a capturing lambda, as briefed. Its D `File` closes in
//     its destructor even when the body throws, so a scoped FILE* guard is used, the
//     same shape as src/geom/surface/parametricsurface.cpp:52-70.
//   * `rotate(Quaternion, Vector3)` (D:443) and `rotate(double, Vector3, Vector3)`
//     (D:481) both take their Vector3 arguments by value as D does, so the
//     `rotation_axis.normalize()` at D:485 cannot be seen by the caller (measured:
//     row rot.a axis after call still prints y=2). `double[] Rmat; Rmat.length = 9;`
//     (D:450-451) is a positive literal length, so backlog T-914's signed
//     `arr.length = L` problem does not arise in this module.
//   * `gridTypeName`/`gridTypeFromName` (D:24-39) keep their D spelling rather than
//     the snake_case the mapping table prefers, so that the sgrid.d/usgrid.d call
//     sites port unchanged.
//   * The `version(complex_numbers)` axis (D:15 imports ntypes.complex) is not
//     compiled here (backlog T-904); `number` is `double`, so `p.x.re` in D reads
//     `eil::re(p.x)` here.

#include <array>
#include <cstddef>
#include <memory>
#include <string>

#include "eil/array.hpp"
#include "eil/number.hpp"
#include "geom/elements/nomenclature.hpp"
#include "geom/elements/vector3.hpp"

namespace eil {
namespace geom {

// D:22 `enum Grid_t {structured_grid, unstructured_grid}`
enum class Grid_t { structured_grid, unstructured_grid };

// D:24-30. final switch, so there is no default arm.
std::string gridTypeName(Grid_t gt);

// D:32-39. Throws eil::ErrorBase (a D Error) for anything else.
Grid_t gridTypeFromName(const std::string& name);

class Grid {
  public:
    // D:42-60. Public, as in D — see the header note. deltax/deltay/deltaz and the
    // two bounding-box corners start at D's .init (NaN), the counters at 0.
    Grid_t grid_type;
    int dimensions; // 1, 2 or 3
    std::string label;
    size_t ncells = 0;
    size_t nvertices = 0;
    eil::Array<Vector3> vertices;
    // For both structured and unstructured grids, we can index vertices with i,j,k
    // indices. For unstructured grids, niv==vertices.length, njv==1, nkv==1.
    size_t niv = 0, njv = 0, nkv = 0;
    // For fast searches, we sort the cells according to location.
    Vector3 bb0, bb1; // bounding box for the entire grid
    using BinList = eil::Array<size_t>;
    using Bins = eil::Array<eil::Array<eil::Array<BinList>>>;
    Bins bins; // Array of bins for holding lists of cell indices
    size_t nbx = 0, nby = 0, nbz = 0; // number of bins in each coordinate direction
    double deltax = eil::d_init<double>(), deltay = eil::d_init<double>(),
           deltaz = eil::d_init<double>(); // size of bin in each direction
    bool cells_are_sorted_into_bins = false;

    // D:62-67
    Grid(Grid_t grid_type, int dimensions, const std::string& label = "");
    virtual ~Grid() = default;

    // D:70-79. Unified indexing, with the three contract asserts.
    size_t single_index(size_t i, size_t j, size_t k = 0) const;

    // D:81-91
    eil::Array<size_t> ijk_indices(size_t indx) const;

    // ---- the pure-virtual table of D:93-122, in D's declaration order. Signatures
    // are copied verbatim: no added const, no changed return type. ----
    virtual Vector3* opIndex(size_t i, size_t j, size_t k = 0) = 0;
    virtual Vector3* opIndex(size_t indx) = 0;
    virtual eil::Array<size_t> get_vtx_id_list_for_cell(size_t i, size_t j,
                                                        size_t k = 0) const = 0;
    virtual void copy_vtx_id_list_for_cell(std::array<size_t, 8>& vtx_list_copy,
                                           size_t& nvtx, size_t i, size_t j,
                                           size_t k = 0) const = 0;
    virtual eil::Array<size_t> get_vtx_id_list_for_cell(size_t indx) const = 0;
    virtual void copy_vtx_id_list_for_cell(std::array<size_t, 8>& vtx_list_copy,
                                           size_t& nvtx, size_t indx) const = 0;

    virtual void read_from_gzip_file(const std::string& fileName, double scale = 1.0) = 0;
    virtual void read_from_raw_binary_file(const std::string& fileName,
                                           double scale = 1.0) = 0;
    // D:104-111. Note the `default:` arm goes to the gzip writer, not to an error.
    void write(const std::string& fileName, const std::string& fmt);
    virtual void write_to_gzip_file(const std::string& fileName) = 0;
    virtual void write_to_raw_binary_file(const std::string& fileName) = 0;
    virtual void write_to_vtk_file(const std::string& fileName) = 0;
    virtual void write_to_su2_file(const std::string& fileName, double scale = 1.0,
                                   bool use_gmsh_order_for_wedges = true) = 0;

    virtual size_t number_of_vertices_for_cell(size_t i) = 0;
    virtual int vtk_element_type_for_cell(size_t i) = 0;
    virtual int get_cell_type(size_t i) = 0;
    virtual std::shared_ptr<Grid> get_boundary_grid(size_t boundary_indx) = 0;
    virtual eil::Array<size_t> get_list_of_boundary_cells(size_t boundary_indx) = 0;

    // D:124-176
    void compute_cell_properties(size_t indx, bool true_centroid, Vector3& centroid,
                                 number& volume);

    // D:178-229
    bool point_is_inside_cell(const Vector3& p, size_t i);

    // D:231-241. The "centre-of-mass" of the vertices defining the cell.
    Vector3 cell_barycentre(size_t indx);

    // D:247-275. Bin indices, determined cautiously because the position is given.
    int get_bin_ix(const Vector3& p);
    int get_bin_iy(const Vector3& p);
    int get_bin_iz(const Vector3& p);

    // D:277-317. A method rather than part of the constructor, because a moving grid
    // may need it called more than once.
    void sort_cells_into_bins(size_t nbinx = 5, size_t nbiny = 5, size_t nbinz = 5);

    // D:319-352, D:354-363, D:365-373
    void find_enclosing_cell_fast(const Vector3& p, size_t& indx, bool& found);
    void find_enclosing_cell_slow(const Vector3& p, size_t& indx, bool& found);
    void find_enclosing_cell(const Vector3& p, size_t& indx, bool& found);

    // D:375-386
    void find_nearest_cell_centre(const Vector3& p, size_t& nearestCell, double& minDist);

    // D:388-433. STL (ASCII); coordinates scaled arbitrarily, no connectivity.
    void write_to_stl_file(const std::string& fileName, double scale = 1.0);

    // D:443-471, D:481-488. Flynn Hack and RJG's rotation entries; the second
    // converts to a quaternion and delegates to the first.
    void rotate(Quaternion q, Vector3 rotation_ctr = Vector3(0.0, 0.0, 0.0));
    void rotate(double theta, Vector3 rotation_axis,
                Vector3 rotation_ctr = Vector3(0.0, 0.0, 0.0));
};

} // namespace geom
} // namespace eil
