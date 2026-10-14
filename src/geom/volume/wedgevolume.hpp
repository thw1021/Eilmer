#pragma once
// geom/volume/wedgevolume.d -> src/geom/volume/wedgevolume.hpp
// (module geom.volume.wedgevolume)
//
// A bottom parametric surface swept about the x-axis through an angle dtheta: opCall
// (D:49-61) is sweep_through_arc(face0123(r,s), t*dtheta), and the constructor derives
// p[0..3] from the face and p[4..7] by sweeping those four (D:27-34).
//
// Imports carried, with the substitutions (D:5-14): `std.conv` (D:5) is the to!string
// of toString() (D:65-66) and the to!number of the unittest (D:92); `std.math` (D:6)
// supplies sqrt/atan2/cos/sin for sweep_through_arc (D:76-79); `std.stdio` (D:7) is
// used only by the unittest's writeln (D:84, D:94) and reaches the port through
// eil::writeln in the test, not in this class; `ntypes.complex` (D:8) and `nm.number`
// (D:9) supply the `number` of dtheta (D:20) — in the trunk variant number is double
// (substrate/include/eil/number.hpp), and the complex step block at D:99-117 is the
// only place the D module needs the complex type; `geom.elements` (D:11) supplies
// Vector3; `geom.volume.parametricvolume` (D:14) the base class.
//   D:12 `import geom.gpath` is a DEAD IMPORT and is not carried — measured by
// grepping wedgevolume.d's 118 lines, no Path/Line/Arc symbol appears.
//   D:13 `import geom.surface` is the aggregate header with no C++ counterpart
// (wave-02-brief section 4 item 5); the concrete symbol actually used is
// ParametricSurface (D:19, D:23), so geom/surface/parametricsurface.hpp is included in
// its place. The unittest's own CoonsPatch is supplied by the test.
//
// Namespace: eil::geom, flat. WedgeVolume is declared by no other geom module (grep
// over gdtk src/geom), so the eil::geom::<module> escape hatch was not needed.
//
// Non-1:1 notes (all registered in provenance/fidelity.yml):
//   * adapted-handles: D:19 `ParametricSurface face0123` is a class handle, so it is
//     ParametricSurface::Ptr and the constructor parameter (D:23) is a handle too;
//     dup() (D:44-47) returns std::shared_ptr<ParametricVolume> rather than D's
//     covariant `WedgeVolume dup()` (wave-02-brief 4b item 1).
//   * NO classString(): same finding as src/geom/volume/subrangedvolume.hpp:28-34 —
//     geom/volume/parametricvolume.d declares only opCall, dup, toString and the nine
//     finite differences (measured: `grep -rn classString geom/volume/*.d` over the
//     reference tree is empty; the only classString in geom/ is Path's,
//     geom/gpath/path.d:123), so this subclass implements the three virtuals the base
//     actually declares.
//   * atan2 is the substrate's, not libm's (D:77). wave-02-brief section 5 and T-910:
//     std/math/trigonometry.d:945 routes atan2(double,double) to Phobos' own
//     atan2Impl!double, and std::atan2 differs by 1 ULP near the axes, which this
//     module then multiplies by r and feeds to Vector3 fields. Written eil::atan2
//     (substrate/include/eil/math.hpp:149). sqrt (D:76) is eil::sqrt so a domain error
//     keeps D's positive-NaN printing; cos/sin (D:79) are std::cos/std::sin, which is
//     correct here and was MEASURED before choosing it rather than inferred: D's
//     sin(double)/cos(double) are core.math intrinsics (trigonometry.d:129-135) and
//     matched glibc bit-for-bit on 12 of 12 sampled arguments (value plus %016x bit
//     pattern, arguments spanning 0.05, PI/4, PI/2, PI, 3, -2, -0.75, 1e-20, 1/3,
//     1.2345678901234567).
//   * `(p0.y)^^2` (D:76) is eil::pow(p0.y, 2), never an expanded product
//     (wave-02-brief section 5). Measured before writing: for 8 sampled doubles D's
//     `x^^2`, x*x and std::pow(x,2.0) produced identical bit patterns 8 of 8, so the
//     spelling is behaviour-neutral here — it is kept because the contract says so.
//   * sweep_through_arc is private in D (D:70-80) and stays private; it is observable
//     only through p[4..7] and opCall, which is how the corpus reaches it (including
//     the signed-zero battery that pins atan2's quadrant choice).
//   * dtheta = 0.0 is NOT an identity on the face point: p[4] comes out of
//     (x, r*cos(theta0), r*sin(theta0)) with r = sqrt(y^^2 + z^^2) and
//     theta0 = atan2(z,y), so it is a round trip, not a copy. Measured with ldc2: for
//     the probe's face, wv.d0.p[1+4].equals.p[1] is false (p[1].y =
//     -0.75 vs p[5].y = -0.74999999999999989) while wv.d0.p[0+4].equals.p[0] is true,
//     and wv.d0.identityAtT0 is false over a 16-point (r,s) grid. No special case is
//     added for theta == 0.
//   * D:20 `number dtheta` has no initialiser, so an unwritten one would be
//     double.init (quiet NaN, wave-02-brief section 5). The members default to
//     eil::d_init<number>() so the object's state before the constructor body runs is
//     D's; both constructors assign dtheta, but a wedge built from a NaN dtheta prints
//     dtheta=nan with p[4..7] = nan (measured row wv.th[6].dtheta, bits
//     7ff8000000000000 — positive NaN).
//   * D:34 `Vector3[8] p` static array → std::array<Vector3,8>, each element through
//     Vector3's own d_init members. All eight are assigned by both constructors.
//   * The copy constructor (D:37-42) copies the stored p[] while dup() (D:44-47)
//     re-runs the constructor and re-derives it; both are ported for that reason.
//   * toString() (D:63-68) puts face0123 through to!string (its virtual toString(); a
//     null handle prints "null", measured row wv.nullface.str) and dtheta through
//     to!string, which is %.6g (wave-02-brief section 5) — while the corpus prints
//     dtheta again at %.17g and as a bit pattern.
//   * No throw path exists in this module: grep over wedgevolume.d's 118 lines finds
//     no `throw`, no `enforce`, no guard outside the unittest, so none is added.

#include <array>
#include <memory>
#include <string>

#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/surface/parametricsurface.hpp"
#include "geom/volume/parametricvolume.hpp"

namespace eil {
namespace geom {

class WedgeVolume : public ParametricVolume {
  public:
    ParametricSurface::Ptr face0123; // D:19 The bottom surface.
    number dtheta = eil::d_init<number>(); // D:20 angle swept about the x-axis
    std::array<Vector3, 8> p;              // D:21 Corner points for the defined volume.

    // D:23-35
    WedgeVolume(const ParametricSurface::Ptr& face0123, number dtheta);
    // D:37-42, `this(ref const(WedgeVolume) other)`
    WedgeVolume(const WedgeVolume& other);

    std::shared_ptr<ParametricVolume> dup() const override;         // D:44-47
    Vector3 operator()(double r, double s, double t) const override; // D:49-61
    std::string toString() const override;                          // D:63-68

  private:
    // D:71-80
    Vector3 sweep_through_arc(const Vector3& p0, number theta) const;
};

} // namespace geom
} // namespace eil
