#pragma once
// geom/surface/meshpatch.d -> src/geom/surface/meshpatch.hpp
// (module geom.surface.meshpatch)
//
// MeshPatch: a ParametricSurface that interpolates bilinearly inside a pre-computed
// background mesh (D:16-100). Two constructors build the mesh — from a StructuredGrid
// (D:23-39) and from a Vector3[][] lattice (D:41-53) — plus a copy constructor (D:55-67)
// and dup() (D:69-72).
//
// Measured against ldc2 1.40.0 at gdtk e587b235 through
// probes/geom/surface/probe_meshpatch.d (D sha256
// e8418bd6df549a844f9d0a7a8fb69d795211efc74fb76454816acde91b23e906); its corpus
// baselines/e587b235/probes/geom_surface_meshpatch.txt (303 rows) is replayed row for row
// by tests/test_geom_surface_meshpatch.cpp. meshpatch.d has no `unittest` block, so that
// corpus is the module's entire reference.
//
// Non-1:1 notes (registered in provenance/fidelity.yml):
//   * D:23 `this(ref StructuredGrid grid)` is a mutable reference to a CLASS HANDLE, so
//     the port takes `StructuredGrid&`. It is not const because opIndex is not const
//     (grid.d's abstract table, sgrid.hpp:130), which is also why the header only
//     forward-declares StructuredGrid and the .cpp includes sgrid.hpp — the same
//     arrangement sgrid.hpp:88 and usgrid.hpp:88 use for each other.
//   * D:20 `Vector3[][] mesh` is an array of dynamic arrays, ported as
//     eil::Array<eil::Array<Vector3>>. D:31 `mesh.length = niv` gives niv EMPTY inner
//     arrays and D:33 `mesh[i] ~= ...` then grows each one; set_length(niv) plus
//     append() reproduces that exactly (substrate/include/eil/array.hpp:162 value-
//     initialises the grown tail, which for an Array<Vector3> is length 0). Nothing
//     here relies on an unwritten slot, so T-929's T.init caveat does not bite.
//   * the corners come from the ARRAY, not from the grid or a surface evaluation:
//     p00 = mesh[0][0], p10 = mesh[niv-1][0], p01 = mesh[0][njv-1], p11 =
//     mesh[niv-1][njv-1] (D:35-38 and D:49-52). Corpus rows mp.grid.niv0 /
//     mp.grid.njv0 pin the consequence — a lattice with no vertices dies inside this
//     assignment, at D:35, and not somewhere in the grid.
//   * njv is taken from row 0 ALONE (D:44), so a ragged lattice is sized by its first
//     row: a longer second row is truncated (mp.v.raggedlong), a shorter one raises
//     ArrayIndexError inside the copy loop (mp.v.raggedshort).
//   * D:25-27 `if (grid.nkv != 1) throw new Error(...)` is a bare object.Error, so by
//     T-922 it carries NO site: `eil::ErrorBase(msg)` with no __FILE__/__LINE__.
//     Corpus rows mp.grid.nkv2 / mp.slab.nkv2 state file=[] line=0.
//   * D:81-82 `int i = to!int(r/dr); i = max(0,i); i = min(i, niv-2);` needs two
//     measured rules, not a reading (see the .cpp): Phobos' float->int raises rather
//     than wraps outside [INT_MIN, INT_MAX], and `min(i, niv-2)` compares UNSIGNED
//     because niv is size_t — measured with niv=1, where the limit wraps to
//     18446744073709551614 and the index therefore stays put. A signed
//     `std::min(i, (int)niv-2)` would clamp to -1 and move the reported failure from
//     D:89 to D:87.
//   * D:79-80 `dr = 1.0/(niv-1)` divides by zero for a one-vertex-wide lattice; that is
//     not an error in D, it is an Infinity that the clamp then swallows. Kept.
//   * D:94-99 `toString()` builds "MeshPatch(" then appends ")" and so is the constant
//     "MeshPatch()" — the mesh is deliberately not rendered. Kept verbatim; the two
//     `str=` rows per object in the corpus are what pin it.
//   * no classString() is declared in D (grep over geom/surface/meshpatch.d finds 0), so
//     the port's classString() is not an override and has no D-side caller; the
//     `classname=` rows compare it with the last dotted component of D's
//     typeid(MeshPatch).name, exactly as coonspatch.hpp:53-56 documents for CoonsPatch.
//   * D:10 `import geom.grid;` (the directory package) narrows to geom/grid/sgrid.hpp:
//     the only symbol meshpatch.d uses from it is StructuredGrid.

#include <cstddef>
#include <memory>
#include <string>

#include "eil/array.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/surface/parametricsurface.hpp"

namespace eil {
namespace geom {

class StructuredGrid; // D:23; defined in geom/grid/sgrid.hpp (see header note)

class MeshPatch : public ParametricSurface {
  public:
    size_t niv = 0; // D:18 (size_t.init is 0; both constructors assign before use)
    size_t njv = 0; // D:19
    eil::Array<eil::Array<Vector3>> mesh; // D:20
    Vector3 p00, p10, p11, p01;           // D:21 corners

    // D:23-39 (`this(ref StructuredGrid grid)`).
    explicit MeshPatch(StructuredGrid& grid);

    // D:41-53 (`this(ref const(Vector3[][]) grid)`).
    explicit MeshPatch(const eil::Array<eil::Array<Vector3>>& grid);

    // D:55-67 (`this(ref const(MeshPatch) other)`) — corners copied verbatim.
    MeshPatch(const MeshPatch& other);

    std::shared_ptr<ParametricSurface> dup() const override; // D:69-72
    Vector3 operator()(double r, double s) const override;   // D:74-92 opCall
    std::string toString() const override;                   // D:94-99
    std::string classString() const; // see header note: not a D symbol here
}; // end class MeshPatch

} // namespace geom
} // namespace eil
