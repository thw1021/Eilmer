#pragma once
// geom/surface/sweptpathpatch.d -> src/geom/surface/sweptpathpatch.hpp
// (module geom.surface.sweptpathpatch)
//
// cA swept along cB: opCall(r,s) = cB(r) + cA(s) - cA(0.0) (D:42-46). The simplest patch
// in the hierarchy, with no unittest block and no throw of its own.
//
// Non-1:1 notes (registered in provenance/fidelity.yml):
//   * adapted-handles: D:13-14 hold the boundaries as class handles, so the port stores
//     PathPtr. They stay PUBLIC because D's are (the corpus assigns one, see the
//     spp.opcall.boundarythrows row), and a port that made them private would break the
//     same call sites D allows.
//   * D:19-20 dup() both boundaries on construction, and D:37-40 dup() again in
//     SweptPathPatch::dup(). Measured by spp.demo.aftermut / spp.demo.callerstr: after the
//     probe moves the caller's Line end point to y=7, the patch built earlier still reports
//     the original boundary, while patches built later see 7.
//   * the corner assignments (D:21-24) take p00/p10 from cB and p01/p11 as vector sums:
//     p01 = cB(0.0) + cA(1.0) - cA(0.0), p11 = cB(1.0) + cA(1.0) - cA(0.0). The association
//     is D's left-to-right ((cB + cA(1)) - cA(0)) and is preserved, because with an Arc or
//     Polyline boundary the two groupings are not the same double.
//   * the copy constructor (D:27-35) copies p00..p11 verbatim while dup() (D:37-40)
//     re-derives them. spp.corrupt.copy keeps (11,12,13)/(14,15,16)/(17,18,19)/(20,21,22),
//     spp.corrupt.dup recomputes (1,0.25,0)/(2,0.25,0)/(2,7.25,0)/(1,7.25,0) from the
//     mutated boundaries — the two constructors are genuinely different and neither is
//     "fixed" here.
//   * no throw/enforce/assert exists in sweptpathpatch.d (measured: 0 matches over its 54
//     lines). Every failure recorded in the corpus is thrown by a boundary Path reached
//     through the constructor's four evaluations (D:21-24) or through opCall: Arc's two
//     evaluate Errors, Bezier's control-point Errors, and Polyline's RangeError on an
//     empty point list.
//   * a null Path argument SEGFAULTS in D rather than raising anything catchable. Measured:
//     this probe exits 139 (core dumped) on `new SweptPathPatch(null, cB)`, with no Error
//     and no Exception object. No guard is added in the port — D has none, and the C++
//     null shared_ptr dereference is the same undefined behaviour. The row is therefore
//     documented here instead of replayed (see the group H comment in
//     probes/geom/surface/probe_sweptpathpatch.d).
//   * D:26-112 declares no classString() (0 matches over gdtk/src/geom/surface), so
//     classString() is not marked override; the spp.<tag> classname= rows compare it with
//     the last dotted component of D's typeid(...).name.
//   * D:54's trailing comment `// [TODO] version(sweptpathpatch)` names a D version
//     identifier that is not enabled anywhere in the tree; carried as a comment at the
//     bottom of the .cpp, per wave-02-brief section 2 (variants are kept, not deleted).
//   * toString() interpolates the two boundaries with to!string, which for a class handle
//     is that Path's own toString() — so the patch's text embeds the full boundary text
//     (d_tostr, geom/gpath/path.hpp:92-154).

#include <memory>
#include <string>

#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"
#include "geom/surface/parametricsurface.hpp"

namespace eil {
namespace geom {

class SweptPathPatch : public ParametricSurface {
  public:
    PathPtr cA; // Path to sweep (think of it as a west edge)
    PathPtr cB; // Path along which cA is swept (think of it as a south edge)
    Vector3 p00, p10, p11, p01;

    // D:17-25
    SweptPathPatch(const PathPtr& cA, const PathPtr& cB);
    // D:27-35 (`this(ref const(SweptPathPatch) other)`)
    SweptPathPatch(const SweptPathPatch& other);

    std::shared_ptr<ParametricSurface> dup() const override;
    Vector3 operator()(double r, double s) const override; // D:42-46 opCall
    std::string toString() const override;
    std::string classString() const; // see header note: not a D symbol here
}; // end class SweptPathPatch

} // namespace geom
} // namespace eil
