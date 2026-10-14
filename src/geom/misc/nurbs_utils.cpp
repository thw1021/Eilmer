// geom/misc/nurbs_utils.d -> src/geom/misc/nurbs_utils.cpp
//
// Definitions in D source order (D:17, :52, :58, :75, :83, :98). Statements, operand
// order and loop bounds are copied from the D text; the arithmetic here is integer
// indexing, additions, subtractions and divisions only (this module calls no
// transcendental function), so there is no `^^`/pow question in it.
//
// See the header for the non-1:1 notes (convNURBSVolDataToVTK deferred to the volume
// stage, Slice<double> instead of Slice<const double>, the NaN-filled workspace
// elements, and ErrorBase vs ExceptionBase for the three throw sites).
#include "geom/misc/nurbs_utils.hpp"

#include <cstddef>
#include <string>

#include "eil/array.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/number.hpp"

namespace eil {
namespace geom {

// D:17-47
int findSpan(double u, int n, int p, eil::Slice<double> U) {
    // Returns the index of the given knot vector whose value is less than the given
    // parameter. This is algorithm A2.1 from Piegl and Tiller (1997) - 'The NURBS Book'.

    // check that given parameter is within given parameter range
    // adjust parameter value if slightly out of range
    double tol = 1.0e-12;
    if ((u < U[0]) && (U[0] - u < tol)) {
        u = U[0];
    } else if ((u > U[U.length() - 1]) && (u - U[U.length() - 1] < tol)) {
        u = U[U.length() - 1];
    } else if (u < U[0] || u > U[U.length() - 1]) {
        std::string errMsg = "Error in FindSpan.\n";
        errMsg += "The parameter value is not compatible with the chosen knot vector.\n";
        errMsg += eil::format("Supplied parameter value: %.18e\n", u);
        errMsg += eil::format("Valid parameter range: [%.18e, %.18e]", U[0], U[U.length() - 1]);
        // D:33 `throw new Exception(errMsg)` — an Exception, not an Error.
        throw eil::ExceptionBase(errMsg, __FILE__, __LINE__);
    }

    // computing span index
    if (u == U[n + 1]) return n;
    auto low = p;
    auto high = n + 1;
    auto mid = (low + high) / 2;
    while (u < U[mid] || u >= U[mid + 1]) {
        if (u < U[mid])
            high = mid;
        else
            low = mid;
        mid = (low + high) / 2;
    }
    return mid;
} // end findSpan()

// D:49-56
NURBSWorkspace::NURBSWorkspace(int p) {
    // D's `this.left.length = p+1` grows the array and leaves the new slots at
    // double.init == quiet NaN (measured: probe row "ws p=2 left=[nan, nan, nan]").
    // eil::Array::set_length value-initialises instead, so the NaN is restored here.
    left.set_length(p + 1);
    left.fill(eil::d_init<double>());
    right.set_length(p + 1);
    right.fill(eil::d_init<double>());
}

// D:58-73
void basisFuns(int i, double u, int p, eil::Slice<double> U, eil::Slice<double> N,
               NURBSWorkspace& nws) {
    // Fills in N array with all nonvanishing basis function terms.
    // This is algorithm A2.2 from Piegl and Tiller (1997) - 'The NURBS Book'.
    N[0] = 1.0;
    for (int j = 1; j < p + 1; ++j) {
        nws.left[j] = u - U[i + 1 - j];
        nws.right[j] = U[i + j] - u;
        double saved = 0.0;
        for (int r = 0; r < j; ++r) {
            auto temp = N[r] / (nws.right[r + 1] + nws.left[j - r]);
            N[r] = saved + nws.right[r + 1] * temp;
            saved = nws.left[j - r] * temp;
        }
        N[j] = saved;
    }
} // end basisFuns()

// D:75-81
void PwTest(eil::Slice<std::array<double, 4>> Pw) {
    if (Pw.length() < 2) {
        std::string errMsg = "NURBS() A NURBS path requires at least two control points.\n";
        errMsg += eil::format("Supplied number of control points: %s", Pw.length());
        // D:79 `throw new Error(text(errMsg))` — an Error. No caller in the tree
        // reaches this (grep), but the message is asserted by the probe replay.
        throw eil::ErrorBase(eil::text(errMsg), __FILE__, __LINE__);
    }
} // end PwTest()

// D:83-96
PwArray duplicatePw(const PwArray& Pw) {
    PwArray Pw_copy;
    Pw_copy.set_length(Pw.length());
    for (size_t i = 0; i < Pw.length(); ++i) {
        const auto& array_i = Pw[i];
        Pw_copy[i].set_length(array_i.length());
        for (size_t j = 0; j < array_i.length(); ++j) {
            const auto& array_j = array_i[j];
            Pw_copy[i][j].set_length(array_j.length());
            for (size_t k = 0; k < array_j.length(); ++k) {
                Pw_copy[i][j][k] = array_j[k];
            }
        }
    }
    return Pw_copy;
} // end duplicatePw()

// D:98-120
eil::Array<double> autoKnotVector(int N, int p) {
    // creates a knot vector this is clamped, uniform and normalised
    // ensure number of control points and degree are compatible
    int a = N - 1;
    if ((p > a) || (p < 1)) {
        // D:103 `throw new Error(...)` — an Error, not an Exception.
        throw eil::ErrorBase(
            "Number of control points is not compatible with degree (1<=p<=N+1).");
    }
    int q = a + p + 1;
    eil::Array<double> U;
    U.set_length(q + 1);
    for (int i = 0; i < q + 1; ++i) {
        // clamp start of curve
        if ((0 <= i) && (i <= p)) { U[i] = 0.0; }
        // fill internal knots
        if ((p < i) && (i <= q - p - 1)) { U[i] = i - p; }
        // clamp end of curve
        if ((q - p - 1 < i) && (i <= q)) { U[i] = q - 2 * p; }
    }
    // normalise knot vector
    double UMax = U[U.length() - 1];
    for (int i = 0; i < q + 1; ++i) { U[i] /= UMax; }
    return U;
} // end autoKnotVector()

} // namespace geom
} // namespace eil
