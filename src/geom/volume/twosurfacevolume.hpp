#pragma once
// geom/volume/twosurfacevolume.d -> src/geom/volume/twosurfacevolume.hpp
// (module geom.volume.twosurfacevolume)
//
// A "ruled" volume between two parametric surfaces: a point inside is an
// interpolation between corresponding points of the two faces (D:1-134). It stores
// dups of both faces, the one-character ruled direction, and the eight corner points
// the constructor derives (D:56-80).
//
// Imports carried, with the substitutions (D:37-41): `std.conv` is the to!string used
// by toString() (D:128-131); `geom.elements` supplies Vector3 and
// approxEqualVectors (the unittest); `geom.volume.parametricvolume` supplies the base
// class. D:39 `import geom.gpath` is unused by this module (grep over its 153 lines:
// no Path/Line symbol appears), and D:40 `import geom.surface` is the aggregate
// header, which has no C++ counterpart (wave-02-brief section 4 item 5); the concrete
// symbol actually needed is ParametricSurface — the type of both stored faces and of
// the constructor arguments — so geom/surface/parametricsurface.hpp is included in
// its place.
//
// Namespace: eil::geom, flat. TwoSurfaceVolume is declared by no other geom module
// (grep over gdtk src/geom), so no eil::geom::<module> nesting was needed.
//
// Non-1:1 notes (all registered in provenance/fidelity.yml):
//   * adapted-handles: D:46-47 `ParametricSurface face0, face1` are class handles, so
//     they are std::shared_ptr<ParametricSurface> (ParametricSurface::Ptr), and dup()
//     (D:91-94) returns std::shared_ptr<ParametricVolume> rather than D's covariant
//     `TwoSurfaceVolume dup()` (wave-02-brief 4b item 1).
//   * D:56-80 and D:107-122 switch on a string. C++ cannot switch on a string, so both
//     become if-chains in exactly D's case order ("r","i" / "s","j" / "t","k" /
//     default). The fall-through pairing of two case labels per group is written as an
//     explicit `||`, which is what D's shared branch body means.
//   * THE `1.0-1` QUIRK IS KEPT (D:110). In the "r"/"i" branch the scalar applied to
//     face0 is the literal expression `1.0-1`, i.e. exactly 0.0 — not (1.0-r). So that
//     branch discards face0 completely: opCall(r,s,t) == face1(s,t).scale(r), and the
//     volume collapses to the origin at r=0. Measured, not inferred: corpus rows
//     tsv.i.discards_face0, tsv.i.collapses_at_r0 and tsv.i.face0_unused are all true
//     (the last one replaces face0 with a totally different surface and every "i"
//     point is unchanged), while tsv.k.face0_used is true for the "t"/"k" branch. The
//     port writes `1.0 - 1` verbatim, with the parentheses D has; "fixing" it would
//     change the geometry of every i-ruled volume (wave-02-brief rule 1).
//   * toString() (D:126-133) appends ruled_direction RAW (D:130, no to!string), so the
//     text carries no quotes — and because the field keeps the spelling the caller
//     used, the alias pairs ("t" vs "k", "s" vs "j", "r" vs "i") have DIFFERENT
//     toString() but identical geometry (measured: tsv.alias.t_is_k=false,
//     tsv.alias.pointsAllSame=true). A port that normalised the direction string would
//     change three corpus rows.
//   * the two throw paths (D:79 constructor default, D:121 opCall default) throw
//     `new Exception(...)`, which ldc2 reports as classinfo.name "object.Exception";
//     the port throws eil::ExceptionBase with the call site's __FILE__/__LINE__
//     (wave-02-brief section 3). The opCall path is reachable because ruled_direction
//     is a public field that can be reassigned after construction — that is how the
//     probe exercises it. dup() re-enters the constructor, so a corrupted direction
//     makes dup() throw too (row tsv.dupThrow.asException).
//   * D:49 `Vector3[8] p` is a static array of structs: default-initialised to
//     Vector3.init (NaN per field, wave-02-brief section 5). std::array<Vector3,8>
//     default-constructs each element through Vector3's own d_init members, so no
//     extra spelling is needed. Both constructors assign all eight entries, so the
//     NaN is unobservable — there is no path in this module that leaves a corner
//     unset (the constructor either assigns all eight or throws).
//   * D:83-89 `this(ref const(TwoSurfaceVolume) other)` is a real, working copy
//     constructor (unlike slabvolume.d:29-34, which is a null-handle virtual call) and
//     ports to TwoSurfaceVolume(const TwoSurfaceVolume&): it dups both faces and
//     COPIES the stored p array rather than re-deriving it, which is observable — dup()
//     goes through the three-argument constructor and recomputes the corners from the
//     faces, so the two differ if the faces move later.

#include <array>
#include <memory>
#include <string>

#include "geom/elements/vector3.hpp"
#include "geom/surface/parametricsurface.hpp"
#include "geom/volume/parametricvolume.hpp"

namespace eil {
namespace geom {

class TwoSurfaceVolume : public ParametricVolume {
  public:
    ParametricSurface::Ptr face0; // D:46 The bottom surface.
    ParametricSurface::Ptr face1; // D:47 The top surface.
    std::string ruled_direction;  // D:48 one of "i", "j", "k", "r", "s", "t"
    std::array<Vector3, 8> p;     // D:49 Corner points for the defined volume.

    // D:51-81
    TwoSurfaceVolume(const ParametricSurface::Ptr& face0, const ParametricSurface::Ptr& face1,
                     const std::string& ruled_direction = "t");
    // D:83-89, `this(ref const(TwoSurfaceVolume) other)`
    TwoSurfaceVolume(const TwoSurfaceVolume& other);

    std::shared_ptr<ParametricVolume> dup() const override;               // D:91-94
    Vector3 operator()(double r, double s, double t) const override;       // D:96-124
    std::string toString() const override;                                 // D:126-133
};

} // namespace geom
} // namespace eil
