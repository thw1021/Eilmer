// geom/gpath/line.d -> src/geom/gpath/line.cpp (module geom.gpath.line).

#include "geom/gpath/line.hpp"

#include <cmath>

#include "geom/gpath/path.hpp"

namespace eil {
namespace geom {

// D:19-22. The parameters shadow the members exactly as in D; the assignments go
// through `this.` so the destination is unambiguous.
Line::Line(const Vector3& p0_, const Vector3& p1_) : p0(p0_), p1(p1_) {}

// D:23-26
Line::Line(const Line& other) : p0(other.p0), p1(other.p1) {}

// D:27-30
std::shared_ptr<Path> Line::dup() const { return std::make_shared<Line>(p0, p1); }

// D:31-34
Vector3 Line::operator()(double t) const { return (1.0 - t) * p0 + t * p1; }

// D:35-38
Vector3 Line::dpdt(double /*t*/) const { return p1 - p0; }

// D:39-42
Vector3 Line::d2pdt2(double /*t*/) const { return Vector3(0.0, 0.0, 0.0); }

// D:43-46
std::string Line::toString() const {
    return "Line(p0=" + d_tostr(p0) + ", p1=" + d_tostr(p1) + ")";
}

// D:47-50
std::string Line::classString() const { return "Line"; }

// D:51-55
number Line::partial_length(double ta, double tb) const {
    Vector3 dp = p1 - p0;
    return std::fabs(tb - ta) * eil::geom::abs(dp);
}

// D:56-61
Vector3 Line::point_from_length(number length, double& t) const {
    Vector3 dp = p1 - p0;
    t = eil::re(length / eil::geom::abs(dp));
    return (*this)(t);
}

} // namespace geom
} // namespace eil
