#pragma once
// geom/volume/subrangedvolume.d -> src/geom/volume/subrangedvolume.hpp
// (module geom.volume.subrangedvolume)
//
// A ParametricVolume adapter that re-maps the unit cube into a sub-range
// [r0,r1] x [s0,s1] x [t0,t1] of an underlying volume (D:12-56). The volume twin of
// geom/surface/subrangedsurface.d, and the same three-parameter case of
// geom/gpath/modifiedpath.d:150-190 (SubRangedPath).
//
// Imports carried, with the reason (D:5-9): `std.conv` is the to!string used by
// toString() (D:49-54); `geom.elements` supplies Vector3 (D:35);
// `geom.volume.parametricvolume` supplies the base class and the handle type. D:8
// `import geom.surface` is the aggregate header, which has no C++ counterpart
// (wave-02-brief section 4 item 5); subrangedvolume.d uses no surface symbol at all
// (grep over its 56 lines: no ParametricSurface/Patch appears), so nothing is
// substituted here. D:7 `import geom.gpath` is likewise unused by this module.
//
// Namespace: eil::geom, flat. SubRangedVolume is declared by no other geom module
// (grep over gdtk src/geom), so the eil::geom::<module> escape hatch was not needed.
//
// Non-1:1 notes (all registered in provenance/fidelity.yml):
//   * adapted-handles: D:14 `ParametricVolume underlying_volume` is a class handle,
//     so the member is std::shared_ptr<ParametricVolume>, and D:22's class parameter
//     `const ParametricVolume pvolume` is a handle too. dup() (D:42-46) returns
//     std::shared_ptr<ParametricVolume> like every other node of this hierarchy
//     (wave-02-brief 4b item 1); D's override already returned the base type, so no
//     static type is lost.
//   * NO classString(): the dispatch brief lists classString() in the subclass
//     contract, but geom/volume/parametricvolume.d:63-270 declares only opCall, dup,
//     toString and the nine finite differences — measured: `grep -rn classString
//     geom/volume/*.d` over the reference tree returns nothing, and the only
//     classString in geom/ is Path's (geom/gpath/path.d:123). The ported base
//     (src/geom/volume/parametricvolume.hpp:48-50) declares no such virtual, so this
//     subclass implements exactly the three virtuals the base declares.
//   * D:37-39 reassign the *parameters* r, s, t before forwarding; the parameters are
//     by value in both languages, so the port does the same and nothing observable
//     differs. The mapping is applied per parameter, so the finite differences the
//     base class computes see adapter-space coordinates and scale by (r1-r0),
//     (s1-s0), (t1-t0) — dumped at 216 sweep points in the probe.
//   * D:27 stores `pvolume.dup()`, not the handle it was given: the adapter owns an
//     independent copy, so mutating the caller's volume later cannot reach it
//     (measured rows srf.A.ptUnchanged / srf.A.textUnchanged). Because dup() (D:44)
//     rebuilds through this constructor, which dups again, a dup of an adapter stays
//     one wrapper deep, while an adapter around an adapter renders nested (measured
//     rows srf.A.dup.str, srf.nest.str, srf.nest.dup.str).
//   * D:50 `to!string(underlying_volume)` is to!string of a class handle, which
//     renders through the object's virtual toString() (measured: "PV()" style text,
//     not a type name). A null handle would print "null" (measured in a scratch D
//     program: `to!string(pv)` with pv null gives "null"), and the ported spelling
//     below is that same two-way choice.
//   * There is no guard on a null `pvolume`, because D has none: `new
//     SubRangedVolume(null)` reaches `pvolume.dup()` (D:27), a virtual call through a
//     null handle, and ldc2 1.40 inserts no null check — measured: the process dies
//     with "Segmentation fault (core dumped)", exit status 139, and `catch
//     (Throwable e)` never runs. The port dereferences the shared_ptr the same way
//     rather than adding an enforce D does not have.
//   * D's six double fields have no initialiser, so an unwritten one would be
//     double.init (quiet NaN, wave-02-brief section 5). Every constructor assigns all
//     six (D:28-33), so the NaN is unobservable here; the members still default to
//     eil::d_init<double>() so the object's state before the body runs is D's.
//   * D:23-25 default arguments r0=0.0, r1=1.0, s0=0.0, s1=1.0, t0=0.0, t1=1.0 are
//     literal doubles and carry over unchanged.

#include <memory>
#include <string>

#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/volume/parametricvolume.hpp"

namespace eil {
namespace geom {

class SubRangedVolume : public ParametricVolume {
  public:
    ParametricVolume::Ptr underlying_volume; // D:14

    double r0 = eil::d_init<double>(); // D:15 subrange of r
    double r1 = eil::d_init<double>(); // D:16
    double s0 = eil::d_init<double>(); // D:17
    double s1 = eil::d_init<double>(); // D:18
    double t0 = eil::d_init<double>(); // D:19
    double t1 = eil::d_init<double>(); // D:20

    // D:22-34
    SubRangedVolume(const ParametricVolume::Ptr& pvolume, double r0 = 0.0, double r1 = 1.0,
                    double s0 = 0.0, double s1 = 1.0, double t0 = 0.0, double t1 = 1.0);

    Vector3 operator()(double r, double s, double t) const override; // D:35-41 opCall
    std::shared_ptr<ParametricVolume> dup() const override;          // D:42-46
    std::string toString() const override;                           // D:47-55
};

} // namespace geom
} // namespace eil
