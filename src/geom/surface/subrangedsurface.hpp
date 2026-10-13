#pragma once
// geom/surface/subrangedsurface.d -> src/geom/surface/subrangedsurface.hpp
// (module geom.surface.subrangedsurface).
//
// A ParametricSurface adapter that re-maps the unit square into a sub-range
// [r0,r1] x [s0,s1] of an underlying surface (D:10-45).
//
// Namespace: eil::geom, flat. SubRangedSurface is declared by no other geom module
// (grep over gdtk src/geom), so no eil::geom::<module> nesting was needed.
//
// Non-1:1 notes (all registered in provenance/fidelity.yml):
//   * adapted-handles: D:12 `ParametricSurface underlying_surface` is a class handle,
//     so the member is a std::shared_ptr<ParametricSurface> and D:18's class
//     parameter `const ParametricSurface psurf` is a handle too. dup() (D:33-36)
//     returns std::shared_ptr<ParametricSurface> like every other node in this
//     hierarchy (wave-02-brief 4b item 1); D's covariant `ParametricSurface dup()`
//     was already the base type here, so nothing is lost.
//   * NO classString(): the dispatch brief lists classString() in the subclass
//     contract, but geom/surface/parametricsurface.d:34-68 declares only opCall, dup,
//     toString and area — there is no classString virtual in the surface hierarchy
//     (measured: `grep -n classString geom/surface/*.d` over the reference tree
//     returns nothing for the whole directory, and the ported base
//     src/geom/surface/parametricsurface.hpp:59-64 declares no such function). Adding
//     one here would create an API D does not have, so the five surface subclasses in
//     this stage implement the three virtuals the base actually declares.
//   * D:29-30 reassign the *parameters* r and s before forwarding, i.e. the mapping is
//     applied to copies of the arguments; the port does the same thing (the
//     parameters are by-value), so nothing observable differs.
//   * D:21 stores `psurf.dup()`, not the handle it was given: the adapter owns an
//     independent copy, so mutating the caller's surface later cannot reach it. Kept,
//     and dumped by the probe (srs.dflt.members). Because dup() rebuilds through this
//     constructor, a dup dups *again*: an N-deep chain stays N-deep after dup
//     (measured rows srs.chain.str / srs.chain.dup.str).
//   * D:40 `to!string(underlying_surface)` is to!string of a class handle, which
//     renders through the object's virtual toString(); a null handle would print
//     "null". A null member is unreachable: `new SubRangedSurface(null)` calls
//     psurf.dup() on null at D:21 and dies with SIGSEGV (exit status 139) rather than
//     throwing — measured by adding a `catch (Throwable)` around that call in the
//     probe: the handler never ran. So the port dereferences the shared_ptr without a
//     guard, exactly like D; it does not add an enforce D does not have.
//   * D's four double fields have no initialiser, so a bare SubRangedSurface's members
//     would be double.init (quiet NaN, wave-02-brief section 5). Every constructor
//     assigns all four (D:22-25), so the NaN is unobservable here; the members are
//     still default-initialised with eil::d_init<double>() so the object's state is
//     D's before the body runs.
//   * D:18-19 default arguments r0=0.0, r1=1.0, s0=0.0, s1=1.0 are literal doubles and
//     carry over unchanged.

#include <memory>
#include <string>

#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/surface/parametricsurface.hpp"

namespace eil {
namespace geom {

class SubRangedSurface : public ParametricSurface {
  public:
    ParametricSurface::Ptr underlying_surface; // D:12
    double r0 = eil::d_init<double>();        // D:13 subrange of r
    double r1 = eil::d_init<double>();        // D:14
    double s0 = eil::d_init<double>();        // D:15
    double s1 = eil::d_init<double>();        // D:16

    // D:18-26
    SubRangedSurface(const ParametricSurface::Ptr& psurf, double r0 = 0.0, double r1 = 1.0,
                     double s0 = 0.0, double s1 = 1.0);

    Vector3 operator()(double r, double s) const override;   // D:27-32 opCall
    std::shared_ptr<ParametricSurface> dup() const override; // D:33-36
    std::string toString() const override;                   // D:37-44
};

} // namespace geom
} // namespace eil
