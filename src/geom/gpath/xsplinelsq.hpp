#pragma once
// geom/gpath/xsplinelsq.d -> src/geom/gpath/xsplinelsq.hpp
// (module geom.gpath.xsplinelsq).
//
// A Path that is a least-squares-fitted cubic spline y(x) (nm/splinelsq.d), swept
// over t mapped onto the fitted x range. Same shape as XSpline, but the fitting
// runs nm/nelmin inside CubicSplineLsq's constructor, so the number of function
// evaluations and restarts is part of what a probe has to compare (see
// probes/geom/gpath/probe_xsplinelsq.d and its replay in
// tests/test_geom_gpath_xsplinelsq.cpp).
//
// Non-1:1 notes:
//   * adapted-handles / value class: see the note in xspline.hpp; `spl` is a
//     by-value eil::nm::CubicSplineLsq, whose ported copy path also rebuilds.
//   * D:35 `this(string fileName, double[] xs, int nseg)` mutates the *parameter*
//     xs with `xs.length = nseg+1` and then fills it, when the caller supplied no
//     knots. In D that resizes only the callee's view of the array (the caller's
//     length field is a separate struct), and nothing after the constructor reads
//     it — measured with ldc2 (probe row "filector.caller.xs.length.still=0"). The
//     port therefore builds a local eil::Array<double> and passes that, which is
//     the same observable behaviour.
//   * D:53, :56 `assert (nseg > 2, "Too few segments specified.")` and
//     `assert(xs.length == nseg+1, "Incorrect number of knots specified.")` become
//     eil_assert with D's message in a comment (eil_assert carries only the
//     condition; same convention as nm/bbla.hpp:90-92).
//   * XSplineLsq does not override dpdt()/d2pdt2() either.

#include <memory>
#include <string>
#include <vector>

#include "eil/array.hpp"
#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"
#include "nm/splinelsq.hpp"

namespace eil {
namespace geom {

class XSplineLsq : public Path {
    // A Path based the cubic-spline y(x).
  public:
    eil::nm::CubicSplineLsq spl;

    // D:28-33
    XSplineLsq(const eil::Array<double>& xd, const eil::Array<double>& yd,
               const eil::Array<double>& wd, const eil::Array<double>& xs, int nseg);
    // D:35-66. Contructs a spline from a file containing x(,y(,w)) coordinates.
    XSplineLsq(const std::string& fileName, const eil::Array<double>& xs, int nseg);
    // D:68-71, `this(const(XSplineLsq) other)`
    XSplineLsq(const XSplineLsq& other);

    std::shared_ptr<Path> dup() const override;
    Vector3 operator()(double t) const override;
    std::string toString() const override;
    std::string classString() const override;

  private:
    // The read loop of D:43-51 (xd, yd, wd from the file columns).
    struct Data {
        eil::Array<double> xd, yd, wd;
    };
    static Data data_from_file(const std::string& fileName);
}; // end class XSplineLsq

} // namespace geom
} // namespace eil
