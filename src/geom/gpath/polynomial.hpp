#pragma once
// geom/gpath/polynomial.d -> src/geom/gpath/polynomial.hpp
// (module geom.gpath.polynomial).
//
// A Path that interpolates its control points with y = sum(C_i * x^^i), the
// coefficients coming from a least-squares solve (nm/bbla.d). Momar Hughes,
// 4th-year thesis, October 2015.
//
// Non-1:1 notes:
//   * adapted-handles: D:62 `override Polynomial dup() const` -> shared_ptr<Path>.
//   * D:115 `derivative_polynomial` has the wraparound the brief warns about: with
//     the loop index i of type size_t, the i == 0 term evaluates
//     `C[0] * 0 * x^^(0-1)`, where 0-1 wraps to 18446744073709551615 and is widened
//     to 1.8446744073709552e19. For |x| > 1 std::pow returns +inf, so the term is
//     `0 * inf` = -NaN, and dpdt() comes out NaN for any polynomial whose x range
//     exceeds 1. Measured against ldc2 in probes/geom/gpath/probe_polynomial.d
//     (rows poly.deriv.*), reproduced here statement for statement: the port does
//     NOT skip the i == 0 term. Fixing it belongs in reports/backlog.md, not here.
//   * D:97 and :110 use `^^` with a size_t exponent. Measured (ldc2, /tmp case
//     x=1.2345 exponent 3 as size_t/int/double): all three give the same bits as
//     std::pow with a double exponent, and all differ from repeated
//     multiplication in the last bit, so each becomes eil::pow().
//   * D:26, :29, :37, :47 `throw new Error(...)` -> eil::ErrorBase.
//   * The `in`/`ref` parameter spellings of D's four constructors port to const
//     references to eil::Array; each body still dups what it stores, as D does.

#include <cstddef>
#include <memory>
#include <string>

#include "eil/array.hpp"
#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"

namespace eil {
namespace geom {

class Polynomial : public Path {
    // polynomial in the form y = sum(c_i * x^^i)|i=0..n
    // Momar Hughes, 4th-year thesis, October 2015
  public:
    eil::Array<Vector3> P; // array of control points to interpolate
    eil::Array<number> C;  // array of coefficients

    // D:23-33
    explicit Polynomial(const eil::Array<Vector3>& P);
    // D:34-43
    Polynomial(const eil::Array<number>& C, number x0, number x1);
    // D:44-56
    Polynomial(const eil::Array<Vector3>& P, const eil::Array<number>& C);
    // D:57-61, `this(ref const(Polynomial) other)`
    Polynomial(const Polynomial& other);

    std::shared_ptr<Path> dup() const override;
    Vector3 operator()(double t) const override;
    Vector3 dpdt(double t) const override;
    std::string toString() const override;
    std::string classString() const override;

  protected:
    void evaluate_coefficients();
    Vector3 evaluate_polynomial(number x) const;
    Vector3 derivative_polynomial(number x) const;
}; // end class Polynomial

} // namespace geom
} // namespace eil
