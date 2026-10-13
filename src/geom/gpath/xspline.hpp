#pragma once
// geom/gpath/xspline.d -> src/geom/gpath/xspline.hpp (module geom.gpath.xspline).
//
// A Path that is the cubic-spline y(x) from nm/spline.d, swept over t in [0,1]
// mapped onto the spline's own x range.
//
// Non-1:1 notes:
//   * adapted-handles, of a kind: D:26 `CubicSpline spl` is a GC handle and every
//     constructor assigns `spl = new CubicSpline(...)`. The ported nm/spline.hpp
//     models CubicSpline as a value class because its only copy path
//     (spline.d:63-66) *rebuilds* from the knots, so `spl` is a by-value member and
//     each construction site does the same rebuild. Nothing aliases.
//   * The two D constructors `this(double[] xs, double[] ys)` and
//     `this(string fileName)` take a view and a path; they port to a const
//     reference to eil::Array<double> and a const std::string&, and the file
//     constructor's read loop (D:40-49) runs in a static helper before the
//     delegating call, exactly as D's body does before `spl = ...`.
//   * D:53 `this(const(XSpline) other)` is the copy constructor; it delegates to
//     CubicSpline's rebuild copy, so the C++ copy constructor is `spl(other.spl)`.
//   * XSpline does NOT override dpdt()/d2pdt2(), so those are Path's finite
//     differences through opCall — visible in the "xspline.dpdt" rows of the probe.
//   * D's unittest imports util.test_runner:skip and calls skip() only in the
//     version(complex_numbers) branch (D:87-89). The trunk branch is what is ported
//     as assertions; the complex branch is carried under EIL_COMPLEX_NUMBERS in the
//     test file, not compiled (brief section 2, T-904).

#include <memory>
#include <string>

#include "eil/array.hpp"
#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"
#include "nm/spline.hpp"

namespace eil {
namespace geom {

class XSpline : public Path {
    // A Path based the cubic-spline y(x).
  public:
    eil::nm::CubicSpline spl;

    // D:28-32. Constructs a spline through specified (x,y) points.
    XSpline(const eil::Array<double>& xs, const eil::Array<double>& ys);
    // D:34-51. Contructs a spline from a file containing x(,y) coordinates.
    explicit XSpline(const std::string& fileName);
    // D:53-56, `this(const(XSpline) other)`
    XSpline(const XSpline& other);

    std::shared_ptr<Path> dup() const override;
    Vector3 operator()(double t) const override;
    std::string toString() const override;
    std::string classString() const override;

  private:
    // The read loop of D:40-49. xs/ys are returned together because C++ has no
    // out-parameter convention here; D fills two locals and then builds the spline
    // from them, which is what the constructor body does.
    struct Sample {
        eil::Array<double> xs, ys;
    };
    static Sample samples_from_file(const std::string& fileName);
}; // end class XSpline

} // namespace geom
} // namespace eil
