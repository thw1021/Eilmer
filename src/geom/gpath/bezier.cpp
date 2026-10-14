// geom/gpath/bezier.d -> src/geom/gpath/bezier.cpp (module geom.gpath.bezier).

#include "geom/gpath/bezier.hpp"

#include <cstddef>
#include <memory>

#include "eil/error.hpp"
#include "eil/format.hpp"
#include "geom/geometry_exception.hpp"
#include "geom/gpath/path.hpp"

namespace eil {
namespace geom {

// D:19-29
Bezier::Bezier(const eil::Array<Vector3>& B0) {
    if (B0.length() == 0) {
        throw eil::ErrorBase("Bezier() No control points present.");
    }
    if (B0.length() == 1) {
        throw eil::ErrorBase("Bezier() Only one control point, not enough for a curve.");
    }
    B = B0.dup();
    set_deriv_control_points();
}

// D:30-33, `this(ref const(Bezier) other) { this(other.B); }`
Bezier::Bezier(const Bezier& other) : Bezier(other.B) {}

// D:34-37
std::shared_ptr<Path> Bezier::dup() const { return std::make_shared<Bezier>(B); }

// D:38-42
Vector3 Bezier::operator()(double t) const {
    // Evaluate B(t)
    return deCasteljau(B, t);
} // end opCall()

// D:43-46
Vector3 Bezier::dpdt(double t) const { return deCasteljau(C, t); }

// D:47-50
Vector3 Bezier::d2pdt2(double t) const { return deCasteljau(D, t); }

// D:51-54
std::string Bezier::toString() const { return "Bezier(B=" + d_tostr(B) + ")"; }

// D:55-58
std::string Bezier::classString() const { return "Bezier"; }

// D:59-98
void Bezier::elevateDegree(int newDegree) {
    /*
     * This algorithm is described in Section 2.5 of
     * Rogers (2001), An Introduction to NURBS: with historical perspective."
     *
     * Rogers cites Forrest for this procedure:
     * Forrest, A.R. (1972)
     * Interactive interpolation and approximation by Bezier polynomials.
     * Comp. J. 15:pp.71--79
     */
    size_t currentDegree = B.length() - 1;

    // D compares the int against the size_t, which makes it unsigned (see header).
    if (static_cast<size_t>(newDegree) < currentDegree) {
        EIL_THROW_GEOM("Desired Bezier degree elevation is less than current degree.");
    }

    if (currentDegree == static_cast<size_t>(newDegree)) return;

    size_t n_elevations = static_cast<size_t>(newDegree) - currentDegree;
    eil::Array<Vector3> B_star;
    B_star.set_length(B.length() + n_elevations);
    for (size_t j = 0; j < n_elevations; ++j) { // D: foreach (j; 0 .. n_elevations)
        size_t n = B.length() - 1;
        B_star[0] = B[0];
        // Blend internal points
        for (size_t i = 1; i <= n; ++i) { // D: foreach (i; 1 .. n+1)
            double alpha = static_cast<double>(i) / (n + 1);
            B_star[i] = alpha * B[i - 1] + (1.0 - alpha) * B[i];
        }
        B_star[n + 1] = B[n];
        // Get B ready for use, or prepare for next iteration
        B.set_length(B.length() + 1);
        for (size_t i = 0; i <= n + 1; ++i) B[i] = B_star[i]; // D: foreach (i; 0 .. n+2)
    }
    // Remember to reset control points for derivative
    // and second derivative curves.
    set_deriv_control_points();
}

// D:103-122
void Bezier::set_deriv_control_points() {
    size_t n = B.length() - 1;
    if (n == 0) {
        // shouldn't reach here due to check in constructor
        throw eil::ErrorBase("Bezier() Curve is a point, derivative not defined");
    }
    C.set_length(n);
    for (size_t i = 0; i < n; ++i) { // D: foreach (i; 0 .. n)
        C[i] = static_cast<number>(n) * (B[i + 1] - B[i]);
    }
    if (n == 1) {
        D = eil::Array<Vector3>{Vector3(0.0)};
        return;
    }
    D.set_length(n - 1);
    for (size_t i = 0; i < n - 1; ++i) { // D: foreach (i; 0 .. n-1)
        D[i] = static_cast<number>(n - 1) * (C[i + 1] - C[i]);
    }
}

// D:123-135
Vector3 Bezier::deCasteljau(const eil::Array<Vector3>& Bp, double t) const {
    if (Bp.length() == 1) return Bp[0];
    size_t n_order = Bp.length() - 1;
    // Apply de Casteljau's algorithm.
    eil::Array<Vector3> Q = Bp.dup(); // work array will be overwritten
    for (size_t k = 0; k < n_order; ++k) { // D: foreach (k; 0 .. n_order)
        for (size_t i = 0; i < n_order - k; ++i) { // D: foreach (i; 0 .. n_order-k)
            Q[i] = (1.0 - t) * Q[i] + t * Q[i + 1];
        }
    }
    return Q[0];
}

} // namespace geom
} // namespace eil
