#pragma once
// geom/surface/nozzleexpansionpatch.d -> src/geom/surface/nozzleexpansionpatch.hpp
// (module geom.surface.nozzleexpansionpatch).
//
// Wilson Chan's nozzle-expansion patch: the north edge is the only user data (D:36) and
// every other point is a quadratic Bezier from the axis up to the wall, built so it
// leaves the axis horizontally and meets the wall normally (D:53-128).
//
// Namespace: eil::geom, flat. No collision (NozzleExpansionPatch is declared by no
// other geom module, grep over gdtk src/geom).
//
// Non-1:1 notes (all registered in provenance/fidelity.yml):
//   * adapted-handles: D:36 `Path cNorth` is a class handle -> std::shared_ptr<Path>;
//     the constructor dups it (D:40) and dup() (D:48-51) rebuilds through that
//     constructor, so a dup owns its own copy of the edge.
//   * D calls `std.math.atan` three times (D:83, :87, :91) and `std.math.tan` once
//     (D:114). Neither is the C library: std/math/trigonometry.d:676 routes atan to
//     atanImpl and :194 routes tan to tanImpl. atan is already mirrored by eil::atan
//     (substrate/include/eil/math.hpp) and is used here; for tan the substrate has NO
//     equivalent, so this TU carries a local d_tan that translates tanImpl!double
//     statement for statement (measured divergence from glibc: at x =
//     0.90000000000000002 D's tan is 1.260158217550339 / bits 3ff4299ba9c2a137 and
//     glibc's is 1.2601582175503392 / bits ...138, 1 ULP, and tan() feeds mid_pt_x
//     directly). The same rule now sits in the substrate for
//     asin/acos (substrate/include/eil/math.hpp, eil::asin/eil::acos); a local d_asin
//     replica used to live at src/gasdyn/idealgasflow.cpp:86 and was folded into those.
//     The remaining substrate gap is eil::tan.
//   * D:56 sets p.z = 0.0 BEFORE the branch chain, while p.x/p.y are still
//     Vector3.init (NaN, wave-02-brief section 5); the declaration order is preserved.
//   * the branch order is r==0.0, r==1.0, s==1.0, else interior (D:57-69), so the
//     corners (0,1) and (1,1) are end-edge rows rather than north-edge rows. The
//     comparisons are exact double == on the parameter, as D writes them.
//   * D:59-68 call cNorth(0.0) / cNorth(1.0) twice each (once for .x, once for .y). The
//     port keeps the repeated calls: the Paths are pure, so this cannot change a value,
//     and it keeps the statement sequence identical to D's.
//   * D:79 `double eps = 0.0001;` and D:80 `number wall_angle = 0.0;` are kept as
//     written; `number` is eil::number (double in this build).
//   * the two end tweaks (D:101-111) divide by (0.02*north_length) with no guard: for a
//     vertical or zero-length north edge that is a float division by zero, giving
//     inf/NaN, which is what the reference produces and what the corpus replays. There
//     is no enforce and no throw in this module (measured: neither word appears in the
//     135 lines), and no unittest block.
//   * NOT ported / not applicable: the dispatch brief anticipated ideal-gas relations
//     from src/gasdyn/idealgasflow.hpp here (Mach 1, sub/supersonic branches). Measured
//     against the reference: nozzleexpansionpatch.d imports only std.conv,
//     ntypes.complex, nm.number, std.math, geom.elements, geom.gpath and
//     geom.surface.parametricsurface (D:25-32) — there is no gas-dynamic quantity and no
//     Mach number anywhere in the file, so the Mach-1 coverage asked for does not exist
//     for this module. The singular-division edges above are the equivalent coverage.

#include <memory>
#include <string>

#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"
#include "geom/surface/parametricsurface.hpp"

namespace eil {
namespace geom {

// d_tan(): NOT a D symbol, and deliberately exported from this header for the same
// reason geom/gpath/path.hpp:101-154 exports d_tostr — it is the port's own renderer of
// a foreign behaviour, here std.math.tan(double) = Phobos' tanImpl!double
// (std/math/trigonometry.d:194 -> :326), which differs from libm by 1 ULP at arguments
// the nozzle reaches. Declared here so tests/test_geom_surface_nozzleexpansionpatch.cpp
// can replay the probe's kernel rows (nep.kern*, 41 rows) against the same function the
// patch calls, instead of against a second copy. NozzleExpansionPatch::operator() uses
// it for D:114.
double d_tan(double x);

class NozzleExpansionPatch : public ParametricSurface {
  public:
    PathPtr cNorth; // D:36 the north edge is the only user-supplied data

    explicit NozzleExpansionPatch(const PathPtr& cNorth);      // D:38-41
    NozzleExpansionPatch(const NozzleExpansionPatch& other);   // D:43-46

    std::shared_ptr<ParametricSurface> dup() const override; // D:48-51
    Vector3 operator()(double r, double s) const override;   // D:53-128 opCall
    std::string toString() const override;                   // D:130-133
};

} // namespace geom
} // namespace eil
