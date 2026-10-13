#pragma once
// geom/volume/parametricvolume.d -> src/geom/volume/parametricvolume.hpp
// (module geom.volume.parametricvolume)
//
// The abstract base of the three-parameter volume hierarchy: 8 D subclasses, and the
// type geom/grid/sgrid.d's volume builders return. Same shape as ParametricSurface
// (wave-02-brief section 4b), with one extra family: nine finite-difference
// accessors (dVdr/dVds/dVdt, d2Vdr2/d2Vds2/d2Vdt2, d2Vdrds/d2Vdrdt/d2Vdsdt).
//
// Non-1:1 notes (registered in provenance/fidelity.yml):
//   * adapted-handles: D:66 `abstract ParametricVolume dup() const` → shared_ptr base
//     handle.
//   * The nine finite-difference bodies are written out separately in D — they are
//     NOT one shared helper — and the mixed ones (d2Vdrds, d2Vdrdt, d2Vdsdt) differ
//     from the pure second derivatives: at the far boundary they use a *first*-order
//     one-sided difference of the first derivative ((f0 - f-1)/d, D:214, :235, :256),
//     not the three-point (f0 - 2f-1 + f-2)/(d*d) that d2Vdr2 uses (D:144). They are
//     transcribed individually rather than factored, because factoring them would
//     change the solver's output at r/s/t > 0.999.
//   * dt = dr = ds = 0.001 in every one of them, and the boundary tests are
//     `x + d > 1.0` / `x - d < 0.0` — the same pair Path.dpdt uses
//     (geom/gpath/path.d:40-53), measured to be `>` not `>=` because
//     0.999 + 0.001 rounds to exactly 1.0.
//   * `2.0*dr` in the central branch (D:86, :108, :130) is a single multiply, while
//     the far/one-sided branches divide by `dr` alone and the pure-second branches
//     divide by `dr*dr`. Order is preserved.
//   * D's locals `Vector3 V0 = this.opCall(...)` and an uninitialised
//     `Vector3 derivative;` — the latter is Vector3.init (NaN per field,
//     wave-02-brief section 5), so a port that zero-fills it would hide a NaN on any
//     path that returns without assigning. Every branch does assign, but the
//     declaration keeps eil::d_init semantics by being default-constructed.
//   * writeVolumeAsVtkXml (D:272-304) prints `p.x.re/p.y.re/p.z.re` under
//     version(complex_numbers) and the bare numbers otherwise; eil::re() is the
//     single spelling that is correct for both variant states.

#include <memory>
#include <string>

#include "geom/elements/vector3.hpp"

namespace eil {
namespace geom {

class ParametricVolume {
  public:
    virtual ~ParametricVolume() = default;

    virtual Vector3 operator()(double r, double s, double t) const = 0; // D:65 opCall
    virtual std::shared_ptr<ParametricVolume> dup() const = 0;
    virtual std::string toString() const = 0;

    // D:68-89
    Vector3 dVdr(double r, double s, double t) const;
    // D:90-111
    Vector3 dVds(double r, double s, double t) const;
    // D:112-133
    Vector3 dVdt(double r, double s, double t) const;
    // D:134-157
    Vector3 d2Vdr2(double r, double s, double t) const;
    // D:158-181
    Vector3 d2Vds2(double r, double s, double t) const;
    // D:182-205
    Vector3 d2Vdt2(double r, double s, double t) const;
    // D:206-226
    Vector3 d2Vdrds(double r, double s, double t) const;
    // D:227-247
    Vector3 d2Vdrdt(double r, double s, double t) const;
    // D:248-268
    Vector3 d2Vdsdt(double r, double s, double t) const;

    using Ptr = std::shared_ptr<ParametricVolume>;
};

// D:272-304
void write_volume_as_vtk_xml(const ParametricVolume& vol, const std::string& fileName, int nrPts,
                             int nsPts, int ntPts);

} // namespace geom
} // namespace eil
