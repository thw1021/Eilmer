#pragma once
// geom/grid/paver2d.d -> src/geom/grid/paver2d.hpp (module geom.grid.paver2d)
//
// The "cheat" interior filler for the 2D unstructured grid generator: it puts ONE
// vertex at the mean of the boundary, connects every boundary vertex to it and
// triangulates the ring. paver2d.d:19-21 says in its own words that this is a
// temporary arrangement, so the cheat and its two user-visible writeln lines are the
// contract here — they are transcribed, NOT improved. The comment at paver2d.d:19 is
// quoted in the header of tests/test_geom_grid_paver2d.cpp and the line itself is
// reproduced byte for byte by the module (it is what geom/grid/paver2d_demo.d prints
// on stdout, and that demo's 98-byte stdout baseline is exactly those two lines).
//
// Measured against ldc2 1.40.0 at gdtk e587b235 through
// probes/geom/grid/probe_paver2d.d; its captured corpus
// baselines/e587b235/probes/geom_grid_paver2d.txt (480 rows) is replayed row for row
// by tests/test_geom_grid_paver2d.cpp. paver2d.d declares no `unittest` block
//   grep -c unittest /home/tang/packages/gdtk/src/geom/grid/paver2d.d  ->  0
// so that replay is the whole gate.
//
// MEASURED DEPENDENCY (this corrects reports/wave-02-grid-brief.md section 0, which
// lists G3 as depending on G1 only). paver2d.d does NOT name PavedGrid or
// POINT_LIST/FACE_LIST/CELL_LIST at all:
//   grep -n "PavedGrid\|POINT_LIST\|FACE_LIST\|CELL_LIST" geom/grid/paver2d.d -> empty
// What fill_interior actually needs comes from geom/grid/usgrid.d (stage G5), which is
// NOT ported yet:
//   USGFace        paver2d.d:15,33         <- usgrid.d:154 (ctor usgrid.d:168)
//   USGCell        paver2d.d:17,44         <- usgrid.d:218 (ctor usgrid.d:225)
//   USGCell_type   paver2d.d:44            <- usgrid.d:40-56, enumerator triangle = 1
//   makeFaceTag    paver2d.d:31,40,41,42   <- usgrid.d:112-132
// Only G1 (geom/elements/vector3, geom/grid/grid) is available today, so this header
// forward-declares the two usgrid.d names it must mention and takes the two container
// types as template parameters. That is the one deliberate non-1:1 shape change in
// this module, and it is what makes the arithmetic testable at all before G5 lands:
//   * the body is the same statement-for-statement port;
//   * it instantiates for either spelling G5 chooses for D's `USGFace[]` / `USGCell[]`
//     class arrays (std::shared_ptr<T>, the house convention per
//     src/geom/surface/bezierpatch.hpp:83, or a raw T*), because
//     detail::make_grid_handle covers both;
//   * it does not couple this header to any particular USGFace/USGCell definition —
//     they are only named at instantiation, in the caller's translation unit.
// When src/geom/grid/usgrid.hpp lands, G5's `paver2d::fill_interior(vertices, faces,
// faceIndices, cells, bndry_vtx_ids)` call site (D usgrid.d:502) compiles unchanged,
// and the fixture in tests/test_geom_grid_paver2d.cpp (which transcribes usgrid.d's
// constructors for the parts fill_interior touches) should be deleted so the test
// drives the real types. No other change is expected.
//
// Other non-1:1 notes (registered in provenance/fidelity.yml):
//   * `enum NodeType {side, rowend, corner, reversal}` (D:12) becomes `enum class
//     NodeType`, matching the house port of an D enum (src/geom/grid/grid.hpp:98
//     Grid_t, src/geom/grid/paver.hpp:112 CellType). Its values, and therefore its
//     sizeof (measured: 4) and its relational ordering, are unchanged.
//     D generates a toString for an enum; the port renders it through nodeTypeName(),
//     declared here and defined in paver2d.cpp. The out-of-range fallback is MEASURED,
//     not guessed: ldc2 prints `cast(NodeType)7`, `cast(NodeType)-1` and
//     `cast(NodeType)2147483647` (corpus rows nt.oor7, nt.oor7.tostring, nt.oorminus1,
//     nt.oormax) — i.e. the string form of the cast expression, not the number.
//     NodeType is otherwise dead code in the D tree (grep -rn "NodeType" gdtk/src
//     --include=*.d matches only this declaration outside extern/D-YAML), and
//     paver2d.d itself never uses it, so the probe is its only constraint.
//   * `ref size_t[] bndry_vtx_ids` (D:17) is a reference parameter that the function
//     never writes; it is ported as a non-const reference anyway, so the D call site
//     shape is preserved.
//   * D's array literals (`size_t[] vtx_id_list = [id, id_mid]`, `[1,1,1]` at D:44)
//     become eil::Array temporaries; `[1,1,1]` is `int[]` in D and is `Array<int>`
//     here, so the USGCell constructor receives the same element type it does in D.
//   * `mid /= n` (D:25) divides by a size_t count. D converts the count to `number`
//     (= double) implicitly, and the port writes that conversion out; the rounding of
//     the division itself is the IEEE operation and is part of the contract, which is
//     why every coordinate in the corpus is printed at %.17g (e.g. the five-point
//     square gives 0.40000000000000002, and the two-point case gives the sum-then-
//     divide result 0.15000000000000002, not a naive mean).
//   * `foreach (j; 0 .. n-1)` (D:36) with n == 0 underflows the size_t. The port keeps
//     the underflow (`j < n - 1`) rather than guarding it, because that is what the
//     reference does: corpus row `case=fi.n0 THROW` shows D indexing bndry_vtx_ids[0]
//     on an empty array and dying with core.exception.ArrayIndexError after the mid
//     vertex was already appended (nvertices goes 1 -> 2). eil::Array::operator[]
//     throws eil::RangeError with the identical text.
//   * `faceIndices[makeFaceTag([id_j, id_jp1])]` (D:40) reads an associative-array
//     key that the caller may not have inserted. In D that read is a RangeError with
//     msg "Range violation" (corpus row `case=fi.miss THROW`), and the module's second
//     writeln at D:47 is then NOT printed — both are in the replay. eil::AA::get
//     throws eil::RangeViolation with the same text, which is why the reads go
//     through get() and not through an inserting accessor.
//   * The `version(complex_numbers)` axis is not compiled (backlog T-904); `number` is
//     `double`, so no `.re` appears in this module.

#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "eil/aa.hpp"
#include "eil/array.hpp"
#include "eil/format.hpp"
#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"

namespace eil {
namespace geom {

// D:12 `enum NodeType {side, rowend, corner, reversal}`
enum class NodeType { side, rowend, corner, reversal };

// D's generated enum member name, rendered for the port. Out-of-range values print
// the way ldc2 prints them (measured: "cast(NodeType)7"). Defined in paver2d.cpp.
std::string nodeTypeName(NodeType nt);

// ---- the two names paver2d.d takes from geom/grid/usgrid.d (stage G5) ----
// Forward declarations only: this module must not define them (they belong to
// usgrid.d) and must not need their definitions, which are required only where the
// template below is instantiated. `: int` matches D's default enum base and the house
// spelling (src/geom/grid/paver.hpp:112 `enum class CellType : int`).
enum class USGCell_type : int;
std::string makeFaceTag(eil::Slice<size_t> vtx_id_list);

namespace detail {

// The element type of a D class array container (`USGFace*` or
// std::shared_ptr<USGFace>), and the D `new USGFace(...)` / `new USGCell(...)` that
// goes into it. Only used by fill_interior below.
template <class Handle> struct grid_handle_pointee {
    using type = typename Handle::element_type;
};
template <class T> struct grid_handle_pointee<T*> { using type = T; };

template <class Handle, class... A>
inline Handle make_grid_handle(A&&... a) {
    using T = typename grid_handle_pointee<Handle>::type;
    if constexpr (std::is_pointer_v<Handle>) {
        return new T(std::forward<A>(a)...);
    } else {
        return std::make_shared<T>(std::forward<A>(a)...);
    }
}

template <class Container>
using container_element =
    std::remove_reference_t<decltype(*std::declval<Container&>().data())>;

} // namespace detail

// D:14-52 void fill_interior(ref Vector3[] vertices, ref USGFace[] faces,
//   ref size_t[string] faceIndices, ref USGCell[] cells, ref size_t[] bndry_vtx_ids)
//
// UsgCellType exists only to make the enumerator name DEPENDENT. (It is NOT spelled
// `CellType`, because eil::geom::CellType is already paver.d's own enum, imported by
// usgrid.d — a template parameter of that name would shadow it here.) Two-phase lookup binds
// a non-dependent name at the template definition, and `enum class USGCell_type : int;`
// is opaque here (its enumerators belong to usgrid.d), so naming
// USGCell_type::triangle directly would not compile (measured with g++ 11.4:
// "'triangle' is not a member of 'eil::geom::USGCell_type'"). Naming it through a
// defaulted template parameter defers the lookup to instantiation, where the real (or
// fixture) enum is complete. It defaults to USGCell_type, so every call site — D's
// usgrid.d:502 included — is unchanged and none should pass it.
template <class FaceArray, class CellArray, class UsgCellType = USGCell_type>
void fill_interior(eil::Array<Vector3>& vertices, FaceArray& faces,
                   eil::AA<std::string, size_t>& faceIndices, CellArray& cells,
                   eil::Array<size_t>& bndry_vtx_ids)
{
    using FaceHandle = detail::container_element<FaceArray>;
    using CellHandle = detail::container_element<CellArray>;

    eil::writeln("Cheat by making all triangle cells connected to the mid-point.");
    // This is just a temporary arrangement, so that we can generate
    // any sort of grid while we get to understand Heather's code.
    size_t n = bndry_vtx_ids.length();
    Vector3 mid = Vector3(0, 0);
    for (size_t k = 0; k < n; ++k) {
        size_t id = bndry_vtx_ids[k];
        mid += vertices[id];
    }
    mid /= static_cast<number>(n); // D: `mid /= n`, n being a size_t count
    size_t id_mid = vertices.length();
    vertices.append(mid);
    // Make all of the interior faces.
    for (size_t k = 0; k < n; ++k) {
        size_t id = bndry_vtx_ids[k];
        eil::Array<size_t> vtx_id_list = {id, id_mid};
        std::string faceTag = makeFaceTag(vtx_id_list.all());
        faceIndices.set(faceTag, faces.length());
        faces.append(detail::make_grid_handle<FaceHandle>(vtx_id_list.all()));
    }
    // Make the actual cells as triangles with all faces pointing out.
    for (size_t j = 0; j < n - 1; ++j) {
        size_t id_j = bndry_vtx_ids[j];
        size_t id_jp1 = (j + 1 < n) ? bndry_vtx_ids[j + 1] : bndry_vtx_ids[0];
        eil::Array<size_t> vtx_id_list = {id_j, id_jp1, id_mid};
        eil::Array<size_t> fj = {id_j, id_jp1};
        eil::Array<size_t> fjj = {id_jp1, id_mid};
        eil::Array<size_t> fjf = {id_mid, id_j};
        eil::Array<size_t> face_id_list = {faceIndices.get(makeFaceTag(fj.all())),
                                           faceIndices.get(makeFaceTag(fjj.all())),
                                           faceIndices.get(makeFaceTag(fjf.all()))};
        eil::Array<int> outsign_list = {1, 1, 1}; // D's [1,1,1] is int[]
        size_t id = cells.length();
        cells.append(detail::make_grid_handle<CellHandle>(UsgCellType::triangle, id,
                                                          vtx_id_list.all(),
                                                          face_id_list.all(),
                                                          outsign_list.all()));
    }

    eil::writeln("[TODO] Write the actual paver code");
    // Generate quadrilateral cells for a closed region.
    // The discretized boundary is already stored in the vertices array and
    // the faces on the boundary are already present in the faces array.
    // Ther are no cells at this point.
} // end fill_interior()

} // namespace geom
} // namespace eil
