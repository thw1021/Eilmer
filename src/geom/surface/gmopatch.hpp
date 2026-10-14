#pragma once
// geom/surface/gmopatch.d -> src/geom/surface/gmopatch.hpp
// (module geom.surface.gmopatch)
//
// GMOPatch: a grid-metric-optimised surface patch. It is a Coons blend whose internal
// (r,s) interpolation parameters are redistributed at construction time so that a test
// grid hung on the patch is as close to orthogonal as a Nelder-Mead run can make it
// (D:1-3, D:81-88). The 32 redistributed parameters ARE the object: opCall (D:110-135)
// re-derives a tensor-product cubic-Bezier remap from them and then blends the four
// boundaries.
//
// Measured against ldc2 1.40.0 at gdtk e587b235 through
// probes/geom/surface/probe_gmopatch.d (D sha256
// ba95d67bf561b72e5efd63eeb3fe3ca56ddd7e4a67d93a472d8eb5e134e8758f); its corpus
// baselines/e587b235/probes/geom_surface_gmopatch.txt (497 rows) is replayed row for row
// by tests/test_geom_surface_gmopatch.cpp. gmopatch.d's single `unittest` block
// (D:150-171, non-complex branch D:158-170) is carried in that corpus as the gmo.ut.*
// rows: the probe replays its statements rather than being compiled with -unittest, so
// the two writeln lines and both assert outcomes are data, not luck.
//
// Non-1:1 notes (registered in provenance/fidelity.yml):
//   * adapted-handles: D:22 declares `Path north, east, south, west;` — class handles —
//     so the port stores std::shared_ptr<Path> (PathPtr) and every construction site
//     dups, as D:52-55 does.
//   * D:26-33 `double[4][4] r_grid/s_grid` are static arrays with in-class initialisers,
//     ported as the RSGrid alias sgrid.hpp:93 already defines for the same D type
//     (`const(double[4][4])` in the StructuredGrid constructor they are handed to). The
//     initial values are D's literal expressions, 1.0/3 and 2.0/3 written as D writes
//     them. They are seeds only: sgrid.d:1498 list_to_rs_grids, which determine_rs_grids
//     calls first thing in its objective and again on the way out, assigns ALL 32 slots,
//     so no slot of a constructed patch still holds a seed. The seeds enter the result
//     through sgrid.d:1472 rs_grids_to_list, which reads 16 of them as the optimiser's
//     starting simplex — which is why T-909's folding rule is worth naming here: D folds
//     `1.0/3` in `real` and narrows once, and so does C++'s double division for this
//     particular value (both land on 0x3FD5555555555555), so the two agree by measurement
//     rather than by assumption (see the gmo.size* groups, whose every number is post-
//     optimiser).
//   * the members are MUTATED by the constructor: D:88 calls
//     my_grid.determine_rs_grids(my_patch, cf, r_grid, s_grid) and sgrid.d:1396 takes
//     those two grids `ref`, so the optimiser writes back into this object. That is why
//     the port passes its own members by non-const reference and why the temporary
//     StructuredGrid is a local that dies at the end of the constructor (D:87): nothing
//     of it survives except the 32 numbers.
//   * D:35-43 the four-corner constructor builds the bounding Lines (north = Line(p01,
//     p11), east = Line(p10, p11), south = Line(p00, p10), west = Line(p00, p01)) and
//     DELEGATES to the four-Path constructor with the niv/njv it was given; C++
//     delegating construction is used, as in coonspatch.cpp:26-33.
//   * D:45-46 the parameter ORDER is south, north, west, east — not the NESW order the
//     rest of the flow code uses; D's own comment (D:47-50) says it predates that. The
//     port keeps D's order, and corpus group gmo.lines.* says the two constructors agree
//     exactly on the same quad.
//   * the four corner cross-checks (D:65-80) run in the order p00, p10, p11, p01, so
//     only the first open corner is ever reported (gmo.open.all still names p00). Each
//     is `throw new Error(text(...))` — a bare object.Error, which by T-922 carries NO
//     site: `eil::ErrorBase(msg)` with no __FILE__/__LINE__, and the corpus rows state
//     file=[] line=0.
//   * D:91-103 the copy constructor re-runs NOTHING (no checks, no optimisation): it
//     dups the four boundaries, copies the four corners verbatim and copies both grids
//     BY VALUE (D:101-102 `r_grid[][] = other.r_grid[][]` over static arrays). Rows
//     gmo.copy.* / gmo.indep.* pin all of that, including that a later mutation of the
//     source's r_grid[1][2] does not move the copy.
//   * D:105-108 dup() re-enters the CHECKING constructor with the DEFAULT 11x11 test
//     grid, so a dup of a patch built at 5x5 has the 11x11 parameters, not its source's
//     (gmo.dup55 grids_eq_ut=true grids_eq_src55=false). It is not a copy.
//   * every `^^` in D:118-124 is eil::pow, none is expanded into a multiplication and
//     none has exponent 0.5 (so T-925's sqrt exception does not apply here) — the same
//     cubic-Bezier weights as sgrid.cpp:904-918's remap, written the same way.
//   * D:137-146 toString() renders the two grids with to!string over a double[4][4],
//     which is "[[0, 0, 0, 0], [...], ...]" at %.6g; d_tostr (geom/gpath/path.hpp:128-152)
//     already reproduces that nesting and precision for std::array, and the boundaries
//     render through their virtual toString().
//   * no classString() is declared in D (grep over geom/surface/gmopatch.d finds 0), so
//     the port's classString() is not an override; the `classname=` rows compare it with
//     the last dotted component of D's typeid(GMOPatch).name.
//   * D:16 `import geom.grid.sgrid;` is needed here for real (the StructuredGrid the
//     constructor builds) and D:17 `import geom.misc.univariatefunctions;` for the four
//     LinearFunctions of D:85-86; the port includes both headers, plus coonspatch.hpp for
//     D:15's CoonsPatch. std.conv's to!string is eil::to_string / d_tostr.
//   * `version (complex_numbers)` (D:153-157) is not compiled (backlog T-904) and D's own
//     comment there says the GMO patch is incompatible with complex numbers in the first
//     place, which is why the non-complex branch is the reference.

#include <memory>
#include <string>

#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"
#include "geom/grid/sgrid.hpp" // RSGrid (D:26-33) and StructuredGrid (D:87)
#include "geom/surface/parametricsurface.hpp"

namespace eil {
namespace geom {

class GMOPatch : public ParametricSurface {
  public:
    PathPtr north, east, south, west; // bounding paths (D:22)
    Vector3 p00, p10, p11, p01;       // corners (D:23)
    // The following initial values for the interior parameters
    // will be adjusted during construction of a new patch. (D:24-33) Two brace levels
    // because RSGrid is std::array<std::array<double,4>,4> and the outer braces stand
    // for the outer std::array itself.
    RSGrid r_grid{{
        {0.0, 1.0 / 3, 2.0 / 3, 1.0},
        {0.0, 1.0 / 3, 2.0 / 3, 1.0},
        {0.0, 1.0 / 3, 2.0 / 3, 1.0},
        {0.0, 1.0 / 3, 2.0 / 3, 1.0}}};
    RSGrid s_grid{{
        {0.0, 0.0, 0.0, 0.0},
        {1.0 / 3, 1.0 / 3, 1.0 / 3, 1.0 / 3},
        {2.0 / 3, 2.0 / 3, 2.0 / 3, 2.0 / 3},
        {1.0, 1.0, 1.0, 1.0}}};

    // D:35-43 — four corners, bounding Lines built from them, then delegate.
    GMOPatch(const Vector3& p00, const Vector3& p10, const Vector3& p11, const Vector3& p01,
             int niv = 11, int njv = 11);

    // D:45-89 — the checking and redistributing constructor. Parameter order is D's,
    // not NESW (see header note).
    GMOPatch(const PathPtr& south, const PathPtr& north, const PathPtr& west,
             const PathPtr& east, int niv = 11, int njv = 11);

    // D:91-103 (`this(ref const(GMOPatch) other)`) — no checks, no re-optimisation.
    GMOPatch(const GMOPatch& other);

    std::shared_ptr<ParametricSurface> dup() const override; // D:105-108
    Vector3 operator()(double r, double s) const override;   // D:110-135 opCall
    std::string toString() const override;                   // D:137-146
    std::string classString() const; // see header note: not a D symbol here
}; // end class GMOPatch

} // namespace geom
} // namespace eil
