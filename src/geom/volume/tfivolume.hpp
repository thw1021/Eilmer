#pragma once
// geom/volume/tfivolume.d -> src/geom/volume/tfivolume.hpp
// (module geom.volume.tfivolume)
//
// TFIVolume is the transfinite-interpolation hexahedron: six boundary surfaces plus the
// eight corners, and an opCall (D:118-138) that blends the six face evaluations against
// the trilinear corner interpolation BigC —
// p_rst = 0.5*(omr*pW + r*pE + oms*pS + s*pN + omt*pB + t*pT) - 0.5*BigC.
//
// Imports carried, with the substitutions (D:5-14): `std.conv` (D:5) is the to!string
// used by toString() (D:142-143); `std.format` (D:6) is the format("%g") of
// checkVolume's message (D:158); `ntypes.complex`/`nm.number` (D:7-8) supply `number`,
// which is eil::number (substrate/include/eil/number.hpp:75). `geom.geometry_exception`
// (D:10) is src/geom/geometry_exception.hpp; `geom.elements` (D:11) supplies Vector3,
// approxEqualVectors (D:32 etc.) and hex_cell_properties (D:155), i.e.
// src/geom/elements/{vector3,properties,nomenclature}.hpp. D:12 `import geom.gpath` and
// D:13 `import geom.surface` are aggregate headers with no C++ counterpart
// (wave-02-brief section 4 item 5): the symbol tfivolume.d actually takes from
// geom.surface is ParametricSurface (D:19, the faces member), and from geom.gpath only
// the commented-out `Path[12] edges` of D:21 — so the port includes
// geom/surface/parametricsurface.hpp and nothing from gpath in the header.
// geom/gpath/path.hpp is included by tfivolume.cpp for d_tostr(), which is what renders
// the Vector3 and Vector3[8] payloads of the eight corner messages.
//
// Namespace: eil::geom, flat. TFIVolume is declared by no other geom module (grep over
// gdtk src/geom), so the eil::geom::<module> escape hatch was not needed.
//
// Non-1:1 notes (all registered in provenance/fidelity.yml):
//   * NO classString(). The dispatch brief originally listed classString() in the
//     surface/volume subclass contract; it is not there — measured: `grep -c classString
//     gdtk/src/geom/surface/*.d gdtk/src/geom/volume/*.d` is 0 for every file, and the
//     only classString in geom/ is Path's (geom/gpath/path.d:123). The ported base
//     (src/geom/volume/parametricvolume.hpp:48-50) declares opCall, dup and toString
//     only, so this subclass overrides exactly those three and nothing else.
//   * adapted-handles: D:19 `ParametricSurface[6] faces` is a static array of *class
//     handles*, so the member is std::array<ParametricSurface::Ptr, 6> — each entry a
//     shared_ptr, duplicated on construction (D:27, D:109) exactly as D does. dup()
//     (D:113-116) returns std::shared_ptr<ParametricVolume>: D's covariant
//     `TFIVolume dup()` cannot be spelled through a shared_ptr (wave-02-brief 4b item 1)
//     and callers that want the concrete type use dynamic_pointer_cast, the same
//     run-time type test D's `cast(TFIVolume)` performs.
//   * D:24 `this(in ParametricSurface[] faceArray)` → eil::Slice<ParametricSurface::Ptr>
//     rather than Slice<const ...>, because Slice<const T> is not constructible from
//     Array<T>/Slice<T> in the substrate (T-907; same reading as
//     src/geom/misc/nurbs_utils.hpp:28-31). D's `in` is a const view of the caller's
//     storage; the ported body only reads, so the effect is the same.
//   * D:91 `this(in Vector3[] p)` → eil::Slice<Vector3>, and the parameter keeps D's
//     name `p`, which SHADOWS the member `p` (D:20) exactly as in D:91-102. D:93 writes
//     `this.p[i] = p[i].dup()` (member on the left, parameter on the right) and D:94-99
//     read the *parameter* — `new CoonsPatch(p[3], p[2], ...)` with no `this.`. The
//     port keeps both spellings. They agree element-wise for i<8 because D:93 copied
//     them, which is why the shadowing is not observable here; the corpus nevertheless
//     reads the parameter, as D does.
//   * The two arrays have DIFFERENT lengths and D's bounds checks decide which
//     constructor throws first: D:27 loops `0 .. 6` over the face slice, so a 5-face
//     slice raises core.exception.ArrayIndexError "index [5] is out of bounds for array
//     of length 5" before any corner is computed, while D:93 loops `0 .. 8`, so a
//     7-point slice raises "index [7] is out of bounds for array of length 7". Both
//     text and index come from eil::Slice::operator[] -> eil::RangeError (an Error
//     sibling), and a LONGER slice than the loop is silently ignored because D's loop
//     bound is the literal 6/8, not `$` — measured in cti.corner.shortArray /
//     .shortPts / .longArray rows.
//   * D:21 `// Path[12] edges;` is commented out in the reference. It stays commented
//     out here; there is no edges member, and the probe/corpus therefore has no edges
//     rows to replay.
//   * Six of the eight corner guards print the corner (D:33, :40, :61, :68, :75, :82)
//     and TWO print the whole `p` array (D:47 corner 2, D:54 corner 3). At those points
//     only p[0..2] / p[0..3] have been assigned, so the rest of the array is
//     Vector3.init — NaN per field (wave-02-brief section 5). The messages are
//     transcribed as D writes them (rule 1), and their bytes are in the corpus
//     (tvi.corner[2] / tvi.corner[3]), which is the only place this module's `.init`
//     is observable.
//   * `throw new Error(text(...))` (D:34 etc.) is a D **Error**, so the port throws
//     eil::ErrorBase; checkVolume's GeometryException (D:159) is an **Exception**
//     (geom/geometry_exception.d:9). Exception and Error are siblings in this port
//     (wave-02-brief section 1 item 1), so a ported `catch (ExceptionBase)` does not
//     catch the open-corner throws and does catch the GeometryException — measured by
//     the tvi.catch.flat.asException / tvi.catch.corner.asError pair.
//   * The corner test is approxEqualVectors (D:32 etc.) with its D defaults
//     maxRelDiff=1.0e-2, maxAbsDiff=1.0e-5 and isClose's `<=` semantics
//     (src/geom/elements/vector3.hpp:145), not a tolerance the port chooses.
//   * checkVolume() (D:149-161) is private and NOT const in D (it mutates nothing but
//     carries no const qualifier), so it is private and non-const here; both
//     constructors that validate call it (D:85, D:101), the copy constructor does not.
//   * D's locals `number volume, iLength, jLength, kLength;` (D:153) are number.init,
//     i.e. NaN; they are written by hex_cell_properties before use, but the port still
//     initialises them with eil::d_init<number>() so an unread path would show the same
//     NaN instead of an indeterminate value. `Vector3 centroid` (D:154) is likewise
//     Vector3.init until hex_cell_properties assigns it.
//   * hex_cell_properties (D:155-156) is called with the eight corners POSITIONALLY, as
//     D writes it — not through properties.d:675-682's array overload — so the port uses
//     the explicit-argument form at src/geom/elements/properties.hpp:187-190. (That
//     matters: the array overload is constrained on `length()`, which std::array does not
//     have, so D's own spelling is also the only one that compiles here.)
//   * toString() (D:140-146) builds in three steps: an initial `~` concatenation, a
//     `foreach(i; 1 .. 6)` of `repr ~= ", " ~ to!string(faces[i])`, then `repr ~= "])"`.
//     Note it indexes faces[0..6] positionally, so it prints in FACE INDEX ORDER
//     (west, east, south, north, bottom, top — geom/elements/nomenclature.d:12-19),
//     which is not the order the box constructor assigns them.

#include <array>
#include <memory>
#include <string>

#include "eil/array.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/surface/parametricsurface.hpp"
#include "geom/volume/parametricvolume.hpp"

namespace eil {
namespace geom {

class TFIVolume : public ParametricVolume {
  public:
    std::array<ParametricSurface::Ptr, 6> faces; // D:19 ParametricSurface[6] faces
    std::array<Vector3, 8> p;                    // D:20 Vector3[8] p
    // D:21: // Path[12] edges;

    using FaceSlice = eil::Slice<ParametricSurface::Ptr>;
    using PointSlice = eil::Slice<Vector3>;

    // Generic, 6-face constructor. D:24-86: dups each face, cross-checks the eight
    // corners and throws an eil::ErrorBase (a D Error) at the first open one, then runs
    // checkVolume().
    explicit TFIVolume(FaceSlice faceArray);

    // Wire-Frame constructor TODO (D:88) — not in the reference, so not here.

    // Simple-Box constructor. D:91-102: copies the eight points, builds the six
    // CoonsPatch faces from them, skips the corner checks ("Don't bother testing for
    // open corners", D:100) and runs checkVolume(). Parameter `p` shadows the member.
    explicit TFIVolume(PointSlice p);

    // Copy constructor. D:107-111 — dup()s every face and every corner, and does NOT
    // re-run checkVolume().
    TFIVolume(const TFIVolume& other);

    // D:113-116. Back through the VALIDATING 6-face constructor, so a volume that the
    // box constructor accepted is corner-checked again on every dup.
    std::shared_ptr<ParametricVolume> dup() const override;

    // D:118-138
    Vector3 operator()(double r, double s, double t) const override;

    std::string toString() const override; // D:140-146

  private:
    // D:149-161
    void checkVolume();
};

} // namespace geom
} // namespace eil
