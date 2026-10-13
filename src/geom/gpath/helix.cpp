// geom/gpath/helix.d -> src/geom/gpath/helix.cpp (module geom.gpath.helix).

#include "geom/gpath/helix.hpp"

#include <cmath>
#include <memory>

#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"

namespace eil {
namespace geom {

// D:27-37
Helix::Helix(const Vector3& va0, const Vector3& va1, const Vector3& xlocal,
             double vr0, double vr1, double dtheta) {
    a0 = va0; a1 = va1;
    r0 = vr0; r1 = vr1;
    theta01 = dtheta;
    // set up local unit vectors at p0
    xdsh = unit(xlocal);
    zdsh = a1 - a0; zdsh.normalize(); // along the axis of the helix
    ydsh = cross(zdsh, xdsh);        // normal to both
}

// D:41-60
Helix::Helix(const Vector3& point_start, const Vector3& point_end,
             const Vector3& axis0, const Vector3& axis1) {
    // Local vectors relative to axis0.
    Vector3 a = axis1 - axis0;
    Vector3 b = point_start - axis0;
    Vector3 c = point_end - axis0;
    zdsh = unit(a);
    Vector3 a0b = b - dot(b, a) * zdsh;
    xdsh = unit(a0b);
    ydsh = cross(zdsh, xdsh);
    a0 = axis0 + dot(b, zdsh) * zdsh;
    a1 = axis0 + dot(c, zdsh) * zdsh;
    r0 = eil::re(dot(b, xdsh));
    Vector3 a1c = c - dot(c, zdsh) * zdsh;
    r1 = eil::re(eil::geom::abs(a1c));
    Vector3 origin = Vector3(0.0, 0.0, 0.0);
    a1c.transform_to_local_frame(xdsh, ydsh, zdsh, origin);
    theta01 = std::atan2(eil::re(a1c.y), eil::re(a1c.x));
}

// D:61-67
Helix::Helix(const Helix& other)
    : a0(other.a0), a1(other.a1), xdsh(other.xdsh), ydsh(other.ydsh), zdsh(other.zdsh),
      r0(other.r0), r1(other.r1), theta01(other.theta01) {}

// D:68-71
std::shared_ptr<Path> Helix::dup() const {
    return std::make_shared<Helix>(a0, a1, xdsh, r0, r1, theta01);
}

// D:72-78
Vector3 Helix::operator()(double t) const {
    double r = r0 * (1.0 - t) + r1 * t;
    double theta = theta01 * t;
    Vector3 p = r * std::cos(theta) * xdsh + r * std::sin(theta) * ydsh + a0 * (1.0 - t) + a1 * t;
    return p;
}

// D:79-88
std::string Helix::toString() const {
    return "Helix(a0=" + d_tostr(a0) +
        ", a1=" + d_tostr(a1) +
        ", xdsh=" + d_tostr(xdsh) +
        ", r0=" + d_tostr(r0) +
        ", r1=" + d_tostr(r1) +
        ", theta01=" + d_tostr(theta01) +
        ")";
}

// D:89-92
std::string Helix::classString() const { return "Helix"; }

} // namespace geom
} // namespace eil
