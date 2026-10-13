#pragma once
// geom/gpath/arc.d -> src/geom/gpath/arc.hpp (module geom.gpath.arc).
//
// Circular arc through a start point, an end point and a centre of curvature
// (Arc), plus the three-point variant Arc3 that solves for the centre by
// least squares. Arc overrides length() with the geometric length, so it does NOT
// use Path::partial_length()'s 100-segment chord sum — the two differ in the
// 5th decimal for a quarter circle and the difference is visible in every Polyline
// breakpoint table (see the "arc.length" and "polyline.t_values" rows of
// baselines/e587b235/probes/geom_gpath_arc.txt).
//
// Non-1:1 notes:
//   * adapted-handles: D:30 `override Arc dup() const` -> shared_ptr<Path>.
//   * D:58 `evaluate_position_and_length(in double t, out Vector3 loc, out number L)`
//     has out parameters: D assigns their .init (Vector3(nan,nan,nan) and NaN) to
//     the caller's variables on ENTRY, which is observable on the three throw paths
//     where the body never reaches the assignment. The port reproduces that with an
//     explicit reset at the top (see arc.cpp).
//   * D:69, :78, :89 and :112-115, :137-139 `throw new Error(...)` -> eil::ErrorBase,
//     i.e. D's Error branch. A caller that ports `catch (Exception e)` must not see
//     these (wave-02-brief section 1 item 1).
//   * Arc3's D constructor computes the centre and validates the three points
//     BEFORE calling `super(a, b, c)` (D:141, legal because D defers the base
//     initialisation). C++ initialises the base in the mem-init list, so the same
//     statements run in the same order inside compute_centre(), which is what the
//     base is built from; the observable behaviour (which Error is thrown, with
//     which text, before any Arc3 object exists) is unchanged.

#include <memory>
#include <string>

#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"

namespace eil {
namespace geom {

class Arc : public Path {
  public:
    Vector3 a; // beginning point (t = 0)
    Vector3 b; // end point (t = 1)
    Vector3 c; // centre of curvature

    // D:22-25
    Arc(const Vector3& a, const Vector3& b, const Vector3& c);
    // D:26-29, `this(ref const(Arc) other)`
    Arc(const Arc& other);

    std::shared_ptr<Path> dup() const override;
    Vector3 operator()(double t) const override;
    std::string toString() const override;
    std::string classString() const override;
    number length() const override; // D:49-56, the geometric length

    // D:58-101
    void evaluate_position_and_length(double t, Vector3& loc, number& L) const;
}; // end class Arc

class Arc3 : public Arc {
    // Arc constructed from start-point a, end-point b and another intermediate
    // point m. Internally it is stored as an Arc object.
    //
    // D declares all of Arc3 in its default (module-private) section, which C++
    // cannot express; the ctors and m are public here so that callers in other
    // translation units (geom/grid/paver.d consumers and the tests) can build an
    // Arc3 the way D callers inside the package can. Nothing else changes.
  public:
    // D:108-143
    Arc3(const Vector3& a, const Vector3& m, const Vector3& b);
    // D:144-147, `this(ref const(Arc3) other)`
    Arc3(const Arc3& other);

    Vector3 m; // D:107

    std::shared_ptr<Path> dup() const override;
    std::string toString() const override;
    std::string classString() const override;

  private:
    // The part of D:110-140 that runs before `super(a, b, c)` (see the header note).
    static Vector3 compute_centre(const Vector3& a, const Vector3& m, const Vector3& b);
}; // end class Arc3

} // namespace geom
} // namespace eil
