#pragma once
// geom/grid/usgrid.d -> src/geom/grid/usgrid.hpp (module geom.grid.usgrid)
//
// UnstructuredGrid: the face/cell topology grid, plus its three helper classes
// USGFace (usgrid.d:154), USGCell (usgrid.d:218) and BoundaryFaceSet
// (usgrid.d:328), the module-level tag tables (usgrid.d:40-60) and the three
// free helpers convert_cell_type/n_vertices_for_vtk_element/cell_type_from_name
// (usgrid.d:62-110) and makeFaceTag/sameOrientation (usgrid.d:112-152).
//
// Measured against ldc2 1.40.0 at gdtk e587b235 through
// probes/geom/grid/probe_usgrid.d; its captured corpus
// baselines/e587b235/probes/geom_grid_usgrid.txt (1393 rows) is replayed row for
// row by tests/test_geom_grid_usgrid.cpp. usgrid.d has one `unittest` block
// (usgrid.d:2484-2524) and its rows are in the same corpus (prefix us.unit.*).
//
// THE CYCLE (reports/wave-02-grid-brief.md section 8, section 10.1): usgrid.d
// and sgrid.d reference each other in BOTH directions —
//   usgrid.d:553  this(const StructuredGrid sg, ...)      reads sg.niv/njv/nkv/vertices/bcTags
//   sgrid.d:1316  auto usg = new UnstructuredGrid(this);   (sgrid's whole write_to_su2_file body)
// so the two headers forward-declare each other and only name the partner
// through a reference/pointer here; each .cpp includes the partner's header.
//
// Non-1:1 notes (registered in provenance/fidelity.yml):
//   * adapted-handles. D's class arrays `USGFace[] faces`, `USGCell[] cells`,
//     `BoundaryFaceSet[] boundaries` (D:427-432) are arrays of GC references, and
//     `USGCell left_cell/right_cell` (D:163-164) are nullable class references
//     that must survive reallocation of the `cells` array. The port therefore uses
//     std::shared_ptr for both the containers and the back-links: appending to
//     `cells` moves the *handles*, never the objects, so a face's left_cell stays
//     valid exactly as D's reference does. Raw pointers into eil::Array storage
//     would dangle on the first append, which D's semantics never allow.
//   * D's `ref File fin` constructors (D:188, :272, :367) and `rawWrite(ref File)`
//     (D:209, :314, :410) take std.stdio.File by reference; the port passes the
//     C stream handle (std::FILE*) instead, which is what every other ported file
//     reader here already does (src/nm/stmatrix.hpp:96-116,
//     src/geom/surface/parametricsurface.cpp:52-70).
//   * The two module-level arrays cell_names (D:54) and vtk_element_types (D:58)
//     are `inline` variables so that there is one instance across TUs, matching
//     D's mutable module globals (same device as src/geom/grid/paver.hpp:195-197
//     for POINT_LIST/FACE_LIST/CELL_LIST).
//   * usgrid.d:236-238 `this(const USGCell other, size_t id) { id = other.id; ... }`
//     assigns to the PARAMETER (D shadowing), so the new cell's `id` field keeps
//     its .init value (0 for size_t), NOT other.id and not the passed id. Kept
//     verbatim; corpus rows us.cell.copy.id / us.dup.* pin it.
//   * usgrid.d:2148 `if (objName.length == 0) { label = format(...) }` writes the
//     GRID's `label` member instead of the local `objName` (a shadowed-name bug in
//     the reference). Kept verbatim; corpus us.foam.* rows show both the overwritten
//     label and the file opened as "<setsDir>/" (empty objName).
//   * usgrid.d:1776 `format("SU2 grid has non-zero bounding vector area=", varea)`
//     is a format() call with an argument but no conversion spec, which Phobos
//     rejects with FormatException. The port raises the same class with the same
//     text rather than "fixing" the missing %s (wave-02-brief section 0 rule 1).
//   * Throw-site rule (T-922, applied per site by looking at D's class): a bare
//     `throw new Error(m)` becomes eil::ErrorBase(m) with NO __FILE__/__LINE__
//     (D's object.Error has .file == "" and .line == 0); GeometryException and
//     Exception DO carry the site (EIL_THROW_GEOM / eil::ExceptionBase with
//     __FILE__/__LINE__); assert carries the site (T-918, eil_assert/eil_assert_msg).
//   * `x ^^ 0.5` -> eil::sqrt (T-925); other `^^` -> eil::pow (usgrid has 0 sites,
//     measured: grep -c '\^\^' usgrid.d -> 0).
//   * Signed array lengths: D's `vtx_id_list.length = to!int(tokens[0])` (D:184,
//     :256, :262, :266, :353, :358) and `cells.length = ncells` (D:1413),
//     `vertices.length = nvertices` (D:1479), `new_grid.vertices.length = nvertices`
//     (D:1074) all take a SIGNED value from user input, so they go through
//     eil::Array::set_length_signed (T-914), which raises D's
//     OutOfMemoryError "Memory allocation failed" text for a negative value.
//   * The `version(complex_numbers)` axis is not compiled (backlog T-904);
//     `number` is `double`, so D's `p.x.re` reads as `eil::re(p.x)` here.

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

namespace eil {
namespace geom {

// The partner of the mutual forward declaration (see the header note): sgrid.hpp
// forward-declares UnstructuredGrid the same way.
class StructuredGrid;

// ---- usgrid.d:40-60: the cell-type tag tables ----
// D:40-53 `enum USGCell_type`. `: int` is D's default enum base and the house
// spelling; the declaration in src/geom/grid/paver2d.hpp:114 must keep matching.
enum class USGCell_type : int {
    none = 0, triangle = 1, quad = 2, polygon = 3, tetra = 4, wedge = 5,
    hexahedron = 6, pyramid = 7, line = 8
};

// D:54-56 `string[] cell_names`. Module-level, mutable in D, one instance here.
inline eil::Array<std::string> cell_names = {
    "none", "triangle", "quad", "polygon", "tetra", "wedge", "hexahedron", "pyramid", "line"};

// D:58-60 `int[] vtk_element_types`, built from geom/elements/nomenclature.hpp.
inline eil::Array<int> vtk_element_types = {
    0, static_cast<int>(VTKElement::triangle), static_cast<int>(VTKElement::quad),
    static_cast<int>(VTKElement::polygon), static_cast<int>(VTKElement::tetra),
    static_cast<int>(VTKElement::wedge), static_cast<int>(VTKElement::hexahedron),
    static_cast<int>(VTKElement::pyramid), static_cast<int>(VTKElement::line)};

// D:62-76, D:78-92, D:94-110. @nogc in D; no C++ equivalent needed.
USGCell_type convert_cell_type(int vtk_element_type);      // D:62
int n_vertices_for_vtk_element(int vtk_element_type);      // D:78
USGCell_type cell_type_from_name(const std::string& name); // D:94

// D:278 uses `to!USGCell_type(int)`, whose out-of-range arm is a
// std.conv.ConvException with Phobos' own wording (corpus rows us.name.tenum[9],
// us.name.tenum[-1], us.name.tenum[2147483647]). The port renders that text here
// rather than in substrate (no substrate edit is allowed from a stage).
USGCell_type to_usg_cell_type(int v);

// ---- usgrid.d:112-152: the two free helpers ----
// D:112 `string makeFaceTag(const size_t[] vtx_id_list)`. Signature kept exactly as
// forward-declared by src/geom/grid/paver2d.hpp:115 so that G3's already-green
// template resolves to this definition (grid brief section 10.4).
std::string makeFaceTag(eil::Slice<size_t> vtx_id_list);

// D:134 `bool sameOrientation(const size_t[] a, const size_t[] b)`.
bool sameOrientation(eil::Slice<size_t> vtx_id_list_a, eil::Slice<size_t> vtx_id_list_b);

// ---- usgrid.d:154-216 ----
class USGCell; // D:163-164 name it before its definition (D forward-references classes)

class USGFace {
  public:
    eil::Array<size_t> vtx_id_list;             // D:156
    std::string tag;                            // D:157 (declared, never assigned in this module)
    std::shared_ptr<USGCell> left_cell;         // D:163
    std::shared_ptr<USGCell> right_cell;        // D:164
    bool is_on_boundary = false;                // D:165

    // D:167-170 `this(const size_t[] vtx_id_list)` — dups the id list.
    explicit USGFace(eil::Slice<size_t> vtx_id_list);
    // D:172-178 `this(const USGFace other)` — copies the id list ONLY; D's comment
    // says the const constraint means left_cell/right_cell are NOT copied (measured:
    // corpus us.face.copy.links).
    USGFace(const USGFace& other);
    // D:180-186 `this(string str)` — the text-stream form: "nvtx id0 id1 ...".
    explicit USGFace(const std::string& str);
    // D:188-194 `this(ref File fin)` — the raw-binary form (one int32 at a time).
    explicit USGFace(std::FILE* fin);

    std::string toString() const;               // D:196-199
    std::string toIOString() const;             // D:201-207
    void rawWrite(std::FILE* fout) const;       // D:209-215
};

// ---- usgrid.d:218-326 ----
class USGCell {
  public:
    USGCell_type cell_type;                     // D:220
    size_t id = 0;                              // D:221 (size_t.init is 0)
    eil::Array<size_t> vtx_id_list;             // D:222
    eil::Array<size_t> face_id_list;            // D:223
    eil::Array<int> outsign_list;               // D:224

    // D:226-234. Field names shadow the parameters in D too; written out here.
    USGCell(USGCell_type cell_type, size_t id, eil::Slice<size_t> vtx_id_list,
            eil::Slice<size_t> face_id_list, eil::Slice<int> outsign_list);
    // D:236-243. THE SHADOWED-PARAMETER QUIRK: `id = other.id;` writes the
    // parameter, so this->id stays 0. Kept (see the header note).
    USGCell(const USGCell& other, size_t id);
    // D:245-270 `this(string str, size_t id)` — "quad vtx 4 ... faces 4 ... outsigns 4 ...".
    USGCell(const std::string& str, size_t id);
    // D:272-288 `this(ref File fin, size_t id)`.
    USGCell(std::FILE* fin, size_t id);

    std::string toString() const;               // D:290-299
    std::string toIOString() const;             // D:301-312
    void rawWrite(std::FILE* fout) const;       // D:314-325
};

// ---- usgrid.d:328-421 ----
class BoundaryFaceSet {
  public:
    std::string tag;                            // D:330
    eil::Array<size_t> face_id_list;            // D:331
    eil::Array<int> outsign_list;               // D:332

    BoundaryFaceSet(const std::string& tag, eil::Slice<size_t> face_id_list,
                    eil::Slice<int> outsign_list);          // D:334-339
    explicit BoundaryFaceSet(const std::string& str);        // D:341-365 text form
    explicit BoundaryFaceSet(std::FILE* fin);                // D:367-380 raw form
    BoundaryFaceSet(const BoundaryFaceSet& other);           // D:382-387

    // D:389-397. Indexes face_id_list[0] and [length-1], so a tag-only set
    // (constructed by D:345-347) throws RangeError here — measured, corpus us.bfs.*.
    std::string toString() const;
    std::string toIOString() const;                          // D:399-408
    void rawWrite(std::FILE* fout) const;                     // D:410-420
};

using USGFacePtr = std::shared_ptr<USGFace>;
using USGCellPtr = std::shared_ptr<USGCell>;
using BoundaryFaceSetPtr = std::shared_ptr<BoundaryFaceSet>;

// ---- usgrid.d:424-2479 ----
class UnstructuredGrid : public Grid {
  public:
    size_t nfaces = 0, nboundaries = 0, ninteriorfaces = 0; // D:426
    eil::Array<USGFacePtr> faces;                            // D:427
    eil::AA<std::string, size_t> faceIndices;                // D:430 size_t[string]
    eil::Array<USGCellPtr> cells;                            // D:431
    eil::Array<std::shared_ptr<BoundaryFaceSet>> boundaries; // D:432

    // D:434-438 a new empty grid.
    explicit UnstructuredGrid(int dimensions, const std::string& label = "");
    // D:440-456 copy constructor.
    UnstructuredGrid(const UnstructuredGrid& other, const std::string& new_label = "");
    // D:458-512 construction by paving in 2D (`const Vector3[][] bPoints`).
    UnstructuredGrid(const eil::Array<eil::Array<Vector3>>& bPoints,
                     const std::string& new_label = "", double tol = 1.0e-9);
    // D:514-551 Heather Muir's PavedGrid constructor (deprecated in D, kept).
    UnstructuredGrid(const eil::Array<Vector3>& boundary,
                     const eil::Array<std::shared_ptr<BoundaryFaceSet>>& in_boundaries,
                     const std::string& new_label = "");
    // D:553-827 from a StructuredGrid. The partner type is complete only in
    // usgrid.cpp, which is why this takes a reference (grid brief section 10.1).
    explicit UnstructuredGrid(const StructuredGrid& sg, const std::string& new_label = "");
    // D:830-846 import from a file, format dispatch.
    UnstructuredGrid(const std::string& fileName, const std::string& fmt, bool true_centroids,
                     double scale = 1.0, bool expect_gmsh_order_for_wedges = false,
                     const std::string& new_label = "");

    std::shared_ptr<UnstructuredGrid> dup() const; // D:848-851 (adapted-handles)

    // ---- indexing and location (D:857-1043) ----
    Vector3* opIndex(size_t i, size_t j, size_t k = 0) override;      // D:857
    Vector3* opIndex(size_t indx) override;                           // D:868
    // D:875-886. NOT marked override: geom::Grid (src/geom/grid/grid.hpp:106-204)
    // declares no virtual toString — D's `override string toString()` there overrides
    // core.object.Object.toString, which the port's Grid does not model (grid brief
    // section 1, and reports/wave-02-surfvol-brief.md section 1's correction that
    // classString() exists only in geom/gpath).
    std::string toString() const;                                     // D:875-886
    void sort_faces_in_boundary_order();                              // D:888-991
    size_t number_of_vertices_for_cell(size_t i) override;            // D:993
    int vtk_element_type_for_cell(size_t i) override;                 // D:999
    int get_cell_type(size_t i) override;                             // D:1005
    eil::Array<size_t> get_vtx_id_list_for_cell(size_t i, size_t j,
                                                size_t k = 0) const override; // D:1011
    void copy_vtx_id_list_for_cell(std::array<size_t, 8>& vtx_list_copy, size_t& nvtx,
                                   size_t indx, size_t j,
                                   size_t k = 0) const override;                 // D:1021
    eil::Array<size_t> get_vtx_id_list_for_cell(size_t indx) const override;      // D:1028
    void copy_vtx_id_list_for_cell(std::array<size_t, 8>& vtx_list_copy, size_t& nvtx,
                                   size_t indx) const override;                    // D:1037
    eil::Array<size_t> get_connected_cell_ids_for_cell(size_t i);                  // D:1045
    std::shared_ptr<Grid> get_boundary_grid(size_t boundary_indx) override;         // D:1057
    eil::Array<size_t> get_list_of_boundary_cells(size_t boundary_indx) override;   // D:1106

    // ---- closure checks (D:1133-1174) ----
    Vector3 vectorAreaOfFacet(eil::Slice<size_t> vtx_id_list);  // D:1133
    Vector3 vectorAreaOfCell(const USGCell& c);                 // D:1167

    // ---- cell insertion (D:1179-1250) ----
    void splitTriangleCell(size_t id);                          // D:1179

    // ---- import (D:1258-1798) ----
    void link_faces_back_to_cells();                            // D:1258
    void read_from_gzip_file(const std::string& fileName, double scale = 1.0) override;      // D:1275
    void read_from_raw_binary_file(const std::string& fileName, double scale = 1.0) override; // D:1330
    void read_from_su2_file(const std::string& fileName, bool true_centroids, double scale = 1.0,
                            bool expect_gmsh_order_for_wedges = false);                      // D:1382
    void read_from_vtk_text_file(const std::string& fileName, double scale = 1.0);           // D:1785

    // ---- export (D:1800-2162) ----
    void write_to_gzip_file(const std::string& fileName) override;        // D:1800
    void write_to_raw_binary_file(const std::string& fileName) override;  // D:1823
    void write_to_vtk_file(const std::string& fileName) override;         // D:1850
    void write_to_su2_file(const std::string& fileName, double scale = 1.0,
                           bool use_gmsh_order_for_wedges = true) override; // D:1876
    void write_openFoam_polyMesh(const std::string& topLevelDir);         // D:1930

    // ---- merging and reporting (D:2164-2477) ----
    std::shared_ptr<UnstructuredGrid> joinGrid(
        const std::shared_ptr<UnstructuredGrid>& other, double relTol = 1.0e-6,
        double absTol = 1.0e-9, bool checkBoundariesOnly = true, bool openFoam = false,
        int dimensions = 3);                                              // D:2164
    void writeStats(double relTol = 1.0e-9, double absTol = 1.0e-9);      // D:2416
    eil::Array<size_t> getVertexIdsOnBoundaries() const;                  // D:2450
};

} // namespace geom
} // namespace eil
