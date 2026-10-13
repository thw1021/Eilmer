#pragma once
// geom/surface/coonspatch.d -> src/geom/surface/coonspatch.hpp
// (module geom.surface.coonspatch)
//
// A Coons surface blended from four bounding paths (D:26-112). The nomenclature is the
// base class' (parametricsurface.d:21-32): south runs along r at s=0, north along r at
// s=1, west along s at r=0, east along s at r=1.
//
// Non-1:1 notes (registered in provenance/fidelity.yml):
//   * adapted-handles: D:28 declares the boundaries as `Path north, east, south, west;`
//     — class handles — so the port stores std::shared_ptr<Path> (PathPtr) and every
//     construction site dups, exactly as D:45-48 does. Measured by the
//     cnp.bezier.aftermut / cnp.bezier.callerstr pair: mutating the caller's Bezier
//     after construction leaves the patch's boundary at z=0.1 while the caller's now
//     reads z=5.
//   * D:40 `this(in Path south, in Path north, in Path west, in Path east)` — the
//     parameter ORDER is south, north, west, east and does not match the member
//     declaration order (north, east, south, west). D's own comment (D:41-43) flags
//     that it predates the NESW order used elsewhere; the port keeps D's order.
//   * D:31-38 the four-corner constructor builds the four Lines and DELEGATES to the
//     four-Path constructor (D:37 `this(south, north, west, east)`), so it inherits the
//     corner checks. C++ delegating construction is used instead of calling this() at
//     the end of a body, which D performs before the delegating body's own effects.
//   * the corner checks (D:58-73) run in the order p00, p10, p11, p01 and each throws
//     `new Error` with a text(...) message, so only the first open corner is ever
//     reported — measured with cnp.open.all, which has all four open and still prints
//     the p00 message. D's Error branch is eil::ErrorBase here (wave-02-brief 1.1), and
//     ErrorBase/ExceptionBase are siblings, so a ported `catch (ExceptionBase)` does NOT
//     catch it, matching D.
//   * D:76-86 the copy constructor does not re-run the checks and copies p00..p11
//     verbatim, while D:88-91 dup() re-enters the checking constructor. Both are
//     measured (cnp.copy vs cnp.dup).
//   * D:26-112 declares no classString() (grep over gdtk/src/geom/surface finds 0), so
//     classString() is NOT marked override and has no D-side caller; the
//     `cnp.<tag> classname=` rows compare it with the last dotted component of D's
//     typeid(...).name.
//   * The four `import`s of D:17-23 collapse to what is actually used: geom.elements
//     (Vector3, approxEqualVectors), geom.gpath (Path, Line, Bezier via the probe) and
//     the base class. std.conv's to!string is eil::to_string / d_tostr (path.hpp:92-154).

#include <memory>
#include <string>

#include "eil/array.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"
#include "geom/surface/parametricsurface.hpp"

namespace eil {
namespace geom {

class CoonsPatch : public ParametricSurface {
  public:
    PathPtr north, east, south, west; // bounding paths (D:28)
    Vector3 p00, p10, p11, p01;      // corners (D:29)

    // D:31-38 — builds the four bounding Lines and delegates (see header note).
    CoonsPatch(const Vector3& p00, const Vector3& p10, const Vector3& p11, const Vector3& p01);

    // D:40-74 — the checking constructor. Parameter order is D's, not NESW.
    CoonsPatch(const PathPtr& south, const PathPtr& north, const PathPtr& west,
               const PathPtr& east);

    // D:76-86 (`this(ref const(CoonsPatch) other)`) — no corner checks.
    CoonsPatch(const CoonsPatch& other);

    std::shared_ptr<ParametricSurface> dup() const override;
    Vector3 operator()(double r, double s) const override; // D:93-102 opCall
    std::string toString() const override;
    std::string classString() const; // see header note: not a D symbol here
}; // end class CoonsPatch

} // namespace geom
} // namespace eil
