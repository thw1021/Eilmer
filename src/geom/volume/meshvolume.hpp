#pragma once
// geom/volume/meshvolume.d -> src/geom/volume/meshvolume.hpp
// (module geom.volume.meshvolume)
//
// meshvolume.d is a placeholder: `class MeshVolume : ParametricVolume { // TODO some
// day... }` (D:12-14). Fourteen lines, one empty class, no members, no ctor, no
// overrides, no unittest block.
//
// Imports dropped, with the reason (D:5-9): `std.conv`, `geom.elements`, `geom.gpath`,
// `geom.surface` and `geom.volume.parametricvolume` are all unused by an empty class
// body — measured by grepping the module: D:12-14 reference no symbol. Only
// geom.volume.parametricvolume is needed here, for the base class. `geom.surface` is
// in any case the aggregate header with no C++ counterpart (wave-02-brief section 4
// item 5); the concrete surface headers a volume actually uses are included by the
// modules that need them, and meshvolume.d needs none.
//
// Non-1:1 notes (registered in provenance/fidelity.yml):
//   * No `.cpp`: the class declares nothing, so there is no TU-level entity and
//     nothing to link.
//   * Abstractness is kept the same way D keeps it — by declaring nothing. ldc2
//     rejects `new MeshVolume()` ("cannot create instance of abstract class
//     MeshVolume", naming toString/dup/opCall at parametricvolume.d:67/66/65);
//     g++ 11 rejects the same statement in the port (measured, text recorded in
//     tests/test_geom_volume_meshvolume.cpp). No default implementations are added,
//     because adding them would make the placeholder instantiable and change what
//     the hierarchy accepts.
//   * Namespace: flat eil::geom. `MeshVolume` is declared by no other geom module
//     (grep over gdtk src/geom), so the collision escape hatch is not needed.

#include "geom/volume/parametricvolume.hpp"

namespace eil {
namespace geom {

// D:12-14
class MeshVolume : public ParametricVolume {
    // TODO some day...
}; // end class MeshVolume

} // namespace geom
} // namespace eil
