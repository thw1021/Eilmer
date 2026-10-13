#pragma once
// geom/surface/channelpatch.d -> src/geom/surface/channelpatch.hpp
// (module geom.surface.channelpatch)
//
// A patch that spans two "channel" boundaries: at every r the surface is swept along a
// bridging path joining cSouth(r) to cNorth(r) — a straight Line when ruled, otherwise a
// cubic Bezier whose inner control points lie along the inward normals of the two
// boundaries (D:64-104).
//
// Non-1:1 notes (registered in provenance/fidelity.yml):
//   * adapted-handles: D:15-16 hold `Path cSouth; Path cNorth;` as class handles, so the
//     port stores PathPtr and dups on construction (D:23-24), on copy (D:35-36) and in
//     dup() (D:47). Measured by the chp.aftermut / chp.callerstr pair: mutating the
//     caller's Line afterwards leaves the patch's boundary at its old end point.
//   * D:21 `this(const Path cSouth, const Path cNorth, bool ruled=false, bool pure2D=false)`
//     — both flags default to false, so the plain two-argument construction of
//     surface_demo.d:40 is the NON-ruled, non-2D case (rows chp.demo.*).
//   * make_bridging_path returns a Path handle in D (D:64); the port returns PathPtr. It
//     is public in D and stays public, and its control points are directly observable,
//     which is where the normal construction (oopvA/oopvB/nA/nB, D:93-97) shows up.
//   * D:96-97 compute the two normals with DIFFERENT cross-product operand orders:
//     nA = cross(oopvA, tangentA) but nB = cross(tangentB, oopvB). That asymmetry is what
//     makes both normals face into the channel; it is preserved verbatim and is measured
//     by the chp.arc bridge B[1]/B[2] rows.
//   * D:89 `double L = abs(pBminuspA).re;` reads the real part of the number the vector
//     abs returns; eil::re() is the port's spelling of `.re` (wave-02-brief section 3).
//   * There is no throw/enforce/assert anywhere in channelpatch.d (measured: 0 matches for
//     'throw|enforce|assert(' over its 105 lines). Its failure modes are (a) NaN/zero
//     propagation through Vector3::normalize, which sets the vector to zero rather than
//     NaN when the magnitude is 0 (geom/elements/vector3.d:362-370, measured at rows
//     chp.parallel bridge B[0..3] — the bridge degenerates to [pA, pA, pB, pB], not to
//     NaN), and (b) an exception escaping from a boundary Path that the constructor
//     evaluates four times (D:27-30): rows chp.badarc.radii / chp.badarc.collinear /
//     chp.badboundary all record Error text from geom/gpath, not from this module.
//   * D:13-105 declares no classString() (0 matches over gdtk/src/geom/surface), so
//     classString() is not marked override; the chp.<tag> classname= rows compare it with
//     the last dotted component of D's typeid(...).name.
//   * The corners p00..p11 (D:27-30) are NOT cross-checked between the two boundaries the
//     way CoonsPatch does, so a channel with open corners constructs happily; they are
//     dumped for every construction.

#include <memory>
#include <string>

#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"
#include "geom/surface/parametricsurface.hpp"

namespace eil {
namespace geom {

class ChannelPatch : public ParametricSurface {
  public:
    PathPtr cSouth;
    PathPtr cNorth;
    bool ruled;
    bool pure2D;
    Vector3 p00, p10, p11, p01;

    // D:21-31
    ChannelPatch(const PathPtr& cSouth, const PathPtr& cNorth, bool ruled = false,
                 bool pure2D = false);
    // D:33-43 (`this(ref const(ChannelPatch) other)`)
    ChannelPatch(const ChannelPatch& other);

    std::shared_ptr<ParametricSurface> dup() const override;
    Vector3 operator()(double r, double s) const override; // D:50-56 opCall
    std::string toString() const override;
    std::string classString() const; // see header note: not a D symbol here

    // D:64-104
    PathPtr make_bridging_path(double r) const;
}; // end class ChannelPatch

} // namespace geom
} // namespace eil
