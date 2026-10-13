#pragma once
// geom/volume/slabvolume.d -> src/geom/volume/slabvolume.hpp
// (module geom.volume.slabvolume)
//
// A SlabVolume is a SweptSurfaceVolume whose sweep path is the straight line through
// the bottom face's own first corner: `super(face0123, new Line(p0, p0+dz))` with
// `p0 = face0123(0.0,0.0)` (D:25-26). The module's own content is one field (dz,
// D:15), two constructors, dup() and toString(), plus one unittest (D:49-65).
//
// Imports carried, with the substitutions (D:5-10): `std.conv` is the to!string used
// by toString() (D:43-44); `geom.elements` supplies Vector3; `geom.gpath` supplies
// the Line built on D:26 (src/geom/gpath/line.hpp);
// `geom.volume.sweptsurfacevolume` supplies the base class
// (src/geom/volume/sweptsurfacevolume.hpp) and `geom.volume.parametricvolume` the
// virtual contract. D:8 `import geom.surface` is the aggregate header with no C++
// counterpart (wave-02-brief section 4 item 5); the concrete symbol slabvolume.d
// actually needs from it is ParametricSurface, the type of its constructor argument
// and of the inherited face0123 member, so geom/surface/parametricsurface.hpp is
// what is included in its place.
//
// Namespace: eil::geom, flat. SlabVolume is declared by no other geom module (grep
// over gdtk src/geom), so no eil::geom::<module> nesting was needed.
//
// Non-1:1 notes (all registered in provenance/fidelity.yml):
//   * NO classString(): the dispatch brief lists it in the subclass contract, but
//     ParametricVolume (geom/volume/parametricvolume.d:63-270) declares only opCall,
//     dup, toString and the nine finite differences — measured: `grep -rn
//     classString geom/volume/*.d` over the reference tree is empty, and the ported
//     base src/geom/volume/parametricvolume.hpp:48-50 declares no such virtual. Only
//     Path has one (geom/gpath/path.d:123).
//   * adapted-handles: D:17's `const ParametricSurface face0123` is a class handle,
//     so the argument is ParametricSurface::Ptr (std::shared_ptr), and dup() (D:36-39)
//     returns std::shared_ptr<ParametricVolume>: D's covariant `SlabVolume dup()`
//     cannot be spelled through a shared_ptr (wave-02-brief 4b item 1), and callers
//     that want the concrete type use dynamic_pointer_cast, the same run-time test
//     D's `cast(SlabVolume)` performs.
//   * Construction order. D assigns `this.dz` (D:24), evaluates p0 (D:25) and only
//     then runs super() (D:26). C++ initialises the base subobject before any derived
//     member, so the port evaluates p0 in the mem-initializer list (through the
//     file-local make_edge04 in slabvolume.cpp, which keeps D's single evaluation of
//     face0123(0.0,0.0)) and sets dz right after. The reordering is unobservable:
//     SweptSurfaceVolume's constructor (sweptsurfacevolume.d:20-32) reads only its
//     two arguments and never touches the derived field, and no virtual call reaches
//     the derived object during base construction. The corpus pins the result.
//   * D:29-34 `this(ref const(SlabVolume) other)` is NOT ported; it is deleted here,
//     and that is the closest faithful transcription available. Its body reads
//     `face0123(0.0,0.0)` on the *member* (D:32) before super() (D:33) has assigned
//     it, i.e. a virtual call through a null handle. Measured with ldc2 1.40
//     (probes/geom/volume/probe_slabvolume_copyctor.d): the process dies with
//     "Segmentation fault (core dumped)", exit status 139, and `catch (Throwable e)`
//     never runs. C++ gives no way to write that statement, because the base
//     subobject is already constructed by the time a derived body runs — the same
//     expression would read a valid face and produce a working copy, which is a
//     behaviour change (rule 1 of wave-02-brief forbids it). Deleting the overload
//     keeps the one true observable — "you cannot build a SlabVolume from another
//     SlabVolume" — and moves it from run time to compile time, where it fails
//     loudly. dup() does NOT use it (D:38 calls the two-argument constructor).
//   * dz is a Vector3 field with no initialiser in D, so an unwritten one would be
//     Vector3.init (NaN per field, wave-02-brief section 5); Vector3's default
//     members already carry eil::d_init, so the port needs nothing extra.
//   * toString() (D:41-46) concatenates in two statements (`repr = ...; repr ~= ...`)
//     and uses to!string on a surface handle (virtual toString of the face) and on a
//     Vector3 ("Vector3(x, y, z)" with %.6g fields).

#include <memory>
#include <string>

#include "geom/elements/vector3.hpp"
#include "geom/surface/parametricsurface.hpp"
#include "geom/volume/sweptsurfacevolume.hpp"

namespace eil {
namespace geom {

class SlabVolume : public SweptSurfaceVolume {
  public:
    Vector3 dz; // D:15 slab thickness

    // D:17-27
    SlabVolume(const ParametricSurface::Ptr& face0123, const Vector3& dz);

    // D:29-34 — deleted, see the header note (D's body is a null-handle virtual
    // call: measured SIGSEGV, exit status 139).
    SlabVolume(const SlabVolume& other) = delete;

    std::shared_ptr<ParametricVolume> dup() const override; // D:36-39
    std::string toString() const override;                  // D:41-46
};

} // namespace geom
} // namespace eil
