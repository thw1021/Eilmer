#pragma once
// geom/volume/sweptsurfacevolume.d -> src/geom/volume/sweptsurfacevolume.hpp
// (module geom.volume.sweptsurfacevolume)
//
// A bottom parametric surface translated along a path: opCall (D:46-57) is
// edge04(t) + face0123(r,s) - face0123(0.0,0.0), so the face is anchored so that its
// (0,0) corner sits where edge04(0.0) is (the comment at D:16-17 says exactly that).
//
// Imports carried, with the substitutions (D:5-9): `std.conv` is the to!string used by
// toString() (D:61-62); `geom.elements` (D:6) supplies Vector3 (D:18) and
// approxEqualVectors (D:82, the unittest); `geom.gpath` (D:7) is the aggregate header
// for the path hierarchy — no C++ counterpart (wave-02-brief section 4 item 5) — and
// the concrete symbol actually used is the Path base class (D:15, D:20), so
// geom/gpath/path.hpp is included in its place. `geom.surface` (D:8) likewise has no
// C++ counterpart; the concrete symbol is ParametricSurface (D:14, D:20), so
// geom/surface/parametricsurface.hpp is included. The unittest's own CoonsPatch and
// Line are supplied by the test, which includes those concrete headers.
//
// Namespace: eil::geom, flat. SweptSurfaceVolume is declared by no other geom module
// (grep over gdtk src/geom), so the eil::geom::<module> escape hatch was not needed.
//
// Non-1:1 notes (all registered in provenance/fidelity.yml):
//   * adapted-handles: D:14-15 `ParametricSurface face0123` / `Path edge04` are class
//     handles, so they are ParametricSurface::Ptr / PathPtr, the constructor parameters
//     (D:20) are handles too, and dup() (D:41-44) returns
//     std::shared_ptr<ParametricVolume> rather than D's covariant
//     `SweptSurfaceVolume dup()` (wave-02-brief 4b item 1).
//   * NO classString(): the dispatch brief lists classString() in the subclass
//     contract, but geom/volume/parametricvolume.d declares only opCall, dup, toString
//     and the nine finite differences — measured: `grep -rn classString
//     geom/volume/*.d` over the reference tree returns nothing, and the only
//     classString in geom/ is Path's (geom/gpath/path.d:123). The ported base
//     (src/geom/volume/parametricvolume.hpp:48-50) declares no such virtual, so this
//     subclass implements exactly the three virtuals the base declares. Path, on the
//     other hand, does require classString(), which is why the probe's ProbePath
//     implements one and SweptSurfaceVolume does not.
//   * THE p[0] ANCHOR IS NOT THE SAME EXPRESSION AS opCall (D:24 vs D:55). p[0] is
//     edge04(0.0) ALONE, while opCall(0,0,0) evaluates
//     edge04(0.0) + face0123(0,0) - face0123(0,0). The port keeps both spellings
//     separately rather than factoring a helper, because folding the subtraction into
//     the corner (or out of opCall) would change the arithmetic. Measured with ldc2 on
//     the probe's sheared face, the eight corner rows ssv.anchor.equal[0..7] are all
//     true — i.e. (b + a) - a == b exactly for that particular a — which is a property
//     of the numbers, not of the code, and is why the corpus dumps both sides instead
//     of asserting they are the same expression.
//   * dup() (D:43) rebuilds through the two-argument constructor, which RE-DERIVES the
//     eight corners, while the copy constructor (D:34-39) copies the stored p[]
//     (`foreach(i; 0 .. 8) this.p[i] = other.p[i].dup()`). Both are therefore ported,
//     and the divergence after p[] is overwritten is measured (ssv.mu.copyKeepsMutation
//     and ssv.mu.dupReDerived are both true).
//   * D:18 `Vector3[8] p` is a static array of structs: default-initialised to
//     Vector3.init (NaN per field, wave-02-brief section 5). std::array<Vector3,8>
//     default-constructs each element through Vector3's own d_init members, matching
//     it. Both constructors assign all eight entries, so the NaN is unobservable in
//     this module.
//   * toString() (D:59-64) puts both members through to!string, which renders through
//     their own virtual toString(); a null handle prints "null" (measured row
//     ssv.nullface.str = "SweptSurfaceVolume(face0123=null, edge04=null)", reachable
//     because both fields are public in D:14-15). The port uses the same two-way choice
//     as src/geom/volume/twosurfacevolume.cpp:22-24.
//   * There is no guard/enforce/throw in this module (grep over its 83 lines: the only
//     assert is D:82, inside the unittest), so nothing is added. A null constructor
//     argument reaches `face0123.dup()` (D:22), a virtual call through a null handle,
//     and ldc2 1.40 gives no catchable throwable there — measured in a scratch program:
//     "Segmentation fault (core dumped)", exit status 139, `catch (Throwable)` never
//     runs. Same finding as src/geom/volume/subrangedvolume.hpp:51-56. The port
//     dereferences the shared_ptr the same way rather than inventing an enforce.

#include <array>
#include <memory>
#include <string>

#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"
#include "geom/surface/parametricsurface.hpp"
#include "geom/volume/parametricvolume.hpp"

namespace eil {
namespace geom {

class SweptSurfaceVolume : public ParametricVolume {
  public:
    ParametricSurface::Ptr face0123; // D:14 The bottom surface.
    PathPtr edge04;                  // D:15 The path points are swept along.
    std::array<Vector3, 8> p;        // D:18 Corner points for the defined volume.

    // D:20-32
    SweptSurfaceVolume(const ParametricSurface::Ptr& face0123, const PathPtr& edge04);
    // D:34-39, `this(ref const(SweptSurfaceVolume) other)`
    SweptSurfaceVolume(const SweptSurfaceVolume& other);

    std::shared_ptr<ParametricVolume> dup() const override;         // D:41-44
    Vector3 operator()(double r, double s, double t) const override; // D:46-57
    std::string toString() const override;                           // D:59-64
};

} // namespace geom
} // namespace eil
