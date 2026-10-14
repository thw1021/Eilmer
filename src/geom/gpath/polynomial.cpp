// geom/gpath/polynomial.d -> src/geom/gpath/polynomial.cpp
// (module geom.gpath.polynomial).

#include "geom/gpath/polynomial.hpp"

#include <cstddef>
#include <memory>

#include "eil/error.hpp"
#include "eil/math.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"
#include "nm/bbla.hpp"

namespace eil {
namespace geom {

// D:23-33
Polynomial::Polynomial(const eil::Array<Vector3>& P0) {
    if (P0.length() == 0) {
        throw eil::ErrorBase("Polynomial() No control points present.");
    } // end if
    if (P0.length() == 1) {
        throw eil::ErrorBase("Polynomial() Only one control point, not enough for a curve.");
    } // end if
    P = P0.dup();
    evaluate_coefficients();
} // end Polynomial()

// D:34-43
Polynomial::Polynomial(const eil::Array<number>& C0, number x0, number x1) {
    if (C0.length() == 0) {
        throw eil::ErrorBase("Polynomial() No coefficients provided.");
    } // end if
    C = C0.dup();
    P.set_length(2);
    P[0] = evaluate_polynomial(x0);
    P[1] = evaluate_polynomial(x1);
} // end Polynomial()

// D:44-56
Polynomial::Polynomial(const eil::Array<Vector3>& P0, const eil::Array<number>& C0) {
    if (C0.length() == 0) {
        throw eil::ErrorBase("Polynomial() No coefficients provided.");
    } // end if
    C = C0.dup();
    for (size_t ip = 0; ip < P0.length(); ++ip) { // D: foreach (point; P)
        const Vector3& point = P0[ip];
        if (point.y != evaluate_polynomial(point.x).y) {
            throw eil::ErrorBase("Polynomial() points and coefficients do not match.");
        } // end if
    } // end foreach
    P = P0.dup();
} // end Polynomial()

// D:57-61
Polynomial::Polynomial(const Polynomial& other) : P(other.P.dup()), C(other.C.dup()) {}

// D:62-65
std::shared_ptr<Path> Polynomial::dup() const { return std::make_shared<Polynomial>(P, C); }

// D:66-71
Vector3 Polynomial::operator()(double t) const {
    // Evaluate P(t)
    number xt = P[0].x + t * (P[P.length() - 1].x - P[0].x); // D: P[$-1]
    return evaluate_polynomial(xt);
} // end opCall()

// D:72-78
Vector3 Polynomial::dpdt(double t) const {
    // Evaluate P(t)
    number xt;
    xt = P[0].x + t * (P[P.length() - 1].x - P[0].x);
    return derivative_polynomial(xt);
}

// D:79-82
std::string Polynomial::toString() const { return "Polynomial(P=" + d_tostr(P) + ")"; }

// D:83-86
std::string Polynomial::classString() const { return "Polynomial"; }

// D:89-104
void Polynomial::evaluate_coefficients() {
    size_t n = P.length();
    eil::nm::bbla::Matrix<number> A = eil::nm::bbla::new_matrix<number>(n);
    eil::nm::bbla::Matrix<number> b = eil::nm::bbla::new_matrix<number>(n, 1);
    for (size_t i = 0; i < n; ++i) { // D: foreach (i; 0 .. n)
        b(i, 0) = P[i].y;
        for (size_t j = 0; j < n; ++j) { // D: foreach (j; 0 .. n)
            A(i, j) = eil::pow(P[i].x, static_cast<number>(j)); // D: P[i].x^^j
        } // end foreach
    } // end foreach
    eil::nm::bbla::Matrix<number> x = eil::nm::bbla::lsqsolve<number>(A, b);
    for (size_t i = 0; i < n; ++i) { // D: foreach (i; 0 .. n)
        C.append(x(i, 0));
    } // end foreach
} // end evaluate_coefficients()

// D:106-113
Vector3 Polynomial::evaluate_polynomial(number x) const {
    number y = 0.0;
    for (size_t i = 0; i < C.length(); ++i) { // D: foreach (i; 0 .. C.length)
        y += C[i] * eil::pow(x, static_cast<number>(i)); // D: C[i] * x^^i
    } // end foreach
    return Vector3(x, y);
} // end evaluate_polynomial()

// D:115-122. The i == 0 term is D's size_t wraparound (see the header note); it is
// kept because it is what the reference solver evaluates.
Vector3 Polynomial::derivative_polynomial(number x) const {
    number dy = 0.0;
    for (size_t i = 0; i < C.length(); ++i) { // D: foreach (i; 0 .. C.length)
        dy += C[i] * static_cast<number>(i) *
              eil::pow(x, static_cast<number>(i - 1)); // D: C[i] * i * x^^(i-1)
    } // end foreach
    return Vector3(number(1.0), dy);
} // end derivative_polynomial()

} // namespace geom
} // namespace eil
