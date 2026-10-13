#pragma once
// geom/gpath/line.d -> src/geom/gpath/line.hpp (module geom.gpath.line).
//
// The straight-line Path. Its dpdt()/d2pdt2()/partial_length()/point_from_length()
// overrides replace the base class finite differences with exact expressions, so
// the t=0.0/1.0 boundary branches of Path never run for a Line — a port that
// dropped any override would silently fall back to the differences and still look
// right to two decimals. See baselines/e587b235/probes/geom_gpath_line.txt.
//
// Non-1:1 notes:
//   * adapted-handles: D:27 `override Line dup() const` returns the concrete
//     handle; here it returns std::shared_ptr<Path> (see path.hpp).
//   * D:19 `this(in Vector3 p0, in Vector3 p1)` shadows the members p0/p1; the
//     C++ parameters are named the same way and the bodies assign through `this.`
//     so the shadowing reads exactly as D does.

#include <memory>
#include <string>

#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"

namespace eil {
namespace geom {

class Line : public Path {
  public:
    Vector3 p0; // end-point at t=0
    Vector3 p1; // end-point at t=1

    // D:19-22
    Line(const Vector3& p0, const Vector3& p1);
    // D:23-26, `this(ref const(Line) other)`
    Line(const Line& other);

    std::shared_ptr<Path> dup() const override;
    Vector3 operator()(double t) const override;
    Vector3 dpdt(double t) const override;
    Vector3 d2pdt2(double t) const override;
    std::string toString() const override;
    std::string classString() const override;
    number partial_length(double ta, double tb) const override;
    Vector3 point_from_length(number length, double& t) const override;
}; // end class Line

} // namespace geom
} // namespace eil
