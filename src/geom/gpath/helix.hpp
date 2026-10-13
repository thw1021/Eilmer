#pragma once
// geom/gpath/helix.d -> src/geom/gpath/helix.hpp (module geom.gpath.helix).
//
// A conical helix between two points on a local z-axis. Helix does NOT override
// dpdt()/d2pdt2(), so those come from Path's finite differences (dt = 0.001 with
// the boundary branches) — the probe dumps them at t = 0.0/1.0 to pin that down.
//
// Non-1:1 notes:
//   * adapted-handles: D:68 `override Helix dup() const` -> shared_ptr<Path>.
//   * D's two constructors take `in Vector3` parameters (a D `in` on a struct is a
//     const value); they port to const references, which is the same contract.
//   * D:54, :56, :59 read `.re` off number-valued expressions (`dot(b,xdsh).re`,
//     `geom.abs(a1c).re`, `a1c.y.re`). With number == double that is eil::re(), a
//     no-op numerically but kept so the port stays 1:1 with D's spellings.

#include <memory>
#include <string>

#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"

namespace eil {
namespace geom {

class Helix : public Path {
  public:
    Vector3 a0;   // beginning point on local z-axis (t = 0)
    Vector3 a1;   // end point on local z-axis (t = 1)
    Vector3 xdsh; // local x-axis, unit vector
    Vector3 ydsh; // local y-axis, unit vector
    Vector3 zdsh; // local z-axis, unit vector
    double r0 = eil::d_init<double>(), r1 = eil::d_init<double>(); // starting and ending radii
    // angle (in radians) from starting point to ending point,
    // assuming the right-hand screw convention.
    double theta01 = eil::d_init<double>();

    // D:27-37. Helix constructed from fundamental parameters.
    Helix(const Vector3& a0, const Vector3& a1, const Vector3& xlocal,
          double r0, double r1, double dtheta);
    // D:41-60. Helix constructed from point_start to point_end about an axis
    // from axis0 to axis1.
    Helix(const Vector3& point_start, const Vector3& point_end,
          const Vector3& axis0, const Vector3& axis1);
    // D:61-67, `this(ref const(Helix) other)`
    Helix(const Helix& other);

    std::shared_ptr<Path> dup() const override;
    Vector3 operator()(double t) const override;
    std::string toString() const override;
    std::string classString() const override;
}; // end class Helix

} // namespace geom
} // namespace eil
