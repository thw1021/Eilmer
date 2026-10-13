#pragma once
// geom/gpath/bezier.d -> src/geom/gpath/bezier.hpp (module geom.gpath.bezier).
//
// Bezier curve through the control points B, evaluated by de Casteljau's
// algorithm. dpdt()/d2pdt2() are the *exact* derivative curves (Bezier.d:43-50),
// built once by set_deriv_control_points() and re-evaluated by the same
// deCasteljau() — so any drift in C/D is visible at every t, and elevateDegree()
// has to rebuild them (D:97).
//
// Non-1:1 notes:
//   * adapted-handles: D:34 `override Bezier dup() const` -> shared_ptr<Path>.
//     D's `dup()` copies the control-point array (`this.B = B.dup()` in the
//     constructor, D:27), so the ported Bezier owns an eil::Array<Vector3> rather
//     than an aliased view.
//   * D:19 `this(in Vector3[] B)` takes a D *view*; the port takes a const
//     reference to eil::Array<Vector3> and dups it, which is what D's body does.
//     This keeps D's `new Bezier([a, d, b])` literal form available as
//     `Bezier(Array<Vector3>{a, d, b})`.
//   * D:72 `if (newDegree < currentDegree)` compares an int against a size_t, so
//     D converts newDegree to the unsigned type; the cast in elevateDegree keeps
//     that (a negative newDegree wraps in both languages).
//   * D:115 `D = [Vector3(0)];` replaces the whole second-derivative array with a
//     single point (a linear curve has zero curvature); C++ assigns a fresh
//     one-element Array, same effect.
//   * Every `^^` in bezier.d is inside the `version(complex_numbers)` unittest
//     block (D:161-169: (1-t)^^3, 3*t*(1-t)^^2, 3*(1-t)*t^^2, t^^3) and is ported
//     through eil::pow, not expanded into multiplication (wave-02-brief section 5:
//     x^^3 != x*x*x in the last bit). The class body itself is linear, as D's is.
//   * D:22-25 and :108 `throw new Error(...)` -> eil::ErrorBase (D's Error branch).
//     D:73 `throw new GeometryException(...)` -> GeometryException, which derives
//     from eil::ExceptionBase, so the two are distinguishable by a ported catch
//     clause exactly as in D.

#include <cstddef>
#include <memory>
#include <string>

#include "eil/array.hpp"
#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"

namespace eil {
namespace geom {

class Bezier : public Path {
  public:
    eil::Array<Vector3> B; // collection of control points

    // D:19-29
    explicit Bezier(const eil::Array<Vector3>& B);
    // D:30-33, `this(ref const(Bezier) other)`
    Bezier(const Bezier& other);

    std::shared_ptr<Path> dup() const override;
    Vector3 operator()(double t) const override;
    Vector3 dpdt(double t) const override;
    Vector3 d2pdt2(double t) const override;
    std::string toString() const override;
    std::string classString() const override;
    void elevateDegree(int newDegree);

  protected:
    eil::Array<Vector3> C; // derivative curve
    eil::Array<Vector3> D; // second derivative curve
    void set_deriv_control_points();
    // D:123-135, `deCasteljau(ref const(Vector3[]) B, double t)`. The parameter is
    // named Bp because it shadows the member B in D; the shadowing is harmless and
    // is preserved in spirit by reading only the argument.
    Vector3 deCasteljau(const eil::Array<Vector3>& Bp, double t) const;
}; // end class Bezier

} // namespace geom
} // namespace eil
