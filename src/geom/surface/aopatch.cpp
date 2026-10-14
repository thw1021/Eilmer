// geom/surface/aopatch.d -> src/geom/surface/aopatch.cpp
// (module geom.surface.aopatch)

#include "geom/surface/aopatch.hpp"

#include <cmath>
#include <memory>
#include <string>

#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/line.hpp"
#include "geom/gpath/path.hpp"
#include "geom/surface/coonspatch.hpp"

namespace eil {
namespace geom {

// D:36-44. D builds the four Lines then delegates (D:43), so the same four Lines reach
// the checking constructor (D:46-88) and inherit its corner cross-checks. The pairing is
// D's, verbatim: north = Line(p01,p11), east = Line(p10,p11), south = Line(p00,p10),
// west = Line(p00,p01).
AOPatch::AOPatch(const Vector3& p00, const Vector3& p10, const Vector3& p11,
                 const Vector3& p01, int nx, int ny)
    : AOPatch(std::make_shared<Line>(p00, p10), std::make_shared<Line>(p01, p11),
              std::make_shared<Line>(p00, p01), std::make_shared<Line>(p10, p11), nx, ny) {}

// D:46-88. Parameter order is south, north, west, east.
AOPatch::AOPatch(const PathPtr& south, const PathPtr& north, const PathPtr& west,
                 const PathPtr& east, int nx, int ny) {
    this->north = north->dup();
    this->east = east->dup();
    this->south = south->dup();
    this->west = west->dup();
    _nx = nx; _ny = ny;
    // Set up internal representation.
    p00 = (*(this->south))(0.0);
    p10 = (*(this->south))(1.0);
    p01 = (*(this->north))(0.0);
    p11 = (*(this->north))(1.0);
    // Check alternate evaluation of corners for consistency.
    Vector3 p00_alt = (*(this->west))(0.0);
    Vector3 p10_alt = (*(this->east))(0.0);
    Vector3 p01_alt = (*(this->west))(1.0);
    Vector3 p11_alt = (*(this->east))(1.0);
    // D:64-79 — four `throw new Error(text(...))` guards in the order p00, p10, p11, p01,
    // so only the first open corner reports (measured: ao.open.all has all four open and
    // still prints the p00 message). Error here is eil::ErrorBase, the sibling of
    // ExceptionBase.
    if (!approxEqualVectors(p00, p00_alt)) {
        throw eil::ErrorBase(eil::text("AOPatch open corner p00= ", d_tostr(p00),
                                        " p00_alt= ", d_tostr(p00_alt)));
    }
    if (!approxEqualVectors(p10, p10_alt)) {
        throw eil::ErrorBase(eil::text("AOPatch open corner p10= ", d_tostr(p10),
                                        " p10_alt= ", d_tostr(p10_alt)));
    }
    if (!approxEqualVectors(p11, p11_alt)) {
        throw eil::ErrorBase(eil::text("AOPatch open corner p11= ", d_tostr(p11),
                                        " p11_alt= ", d_tostr(p11_alt)));
    }
    if (!approxEqualVectors(p01, p01_alt)) {
        throw eil::ErrorBase(eil::text("AOPatch open corner p01= ", d_tostr(p01),
                                        " p01_alt= ", d_tostr(p01_alt)));
    }
    // The TFI grid is a fall-back way of evaluating a point on
    // the surface and will be used when the parameter values
    // place us very close to a boundary.
    // The coarse background mesh tends to cut across
    // curved boundaries and so should not be used as our
    // guide near those boundaries.
    tfi_surface = std::make_shared<CoonsPatch>(south, north, west, east);
    compute_background_mesh();
} // end constructor()

// D:90-102 — note that D does NOT assign _nx/_ny here, so they remain int.init == 0 and
// compute_background_mesh() runs on a 1x1 mesh built from +inf spacings (see the header
// note; measured by the ao.copy.* rows, including the ArrayIndexError row).
AOPatch::AOPatch(const AOPatch& other) {
    this->north = other.north->dup();
    this->east = other.east->dup();
    this->south = other.south->dup();
    this->west = other.west->dup();
    p00 = other.p00;
    p10 = other.p10;
    p01 = other.p01;
    p11 = other.p11;
    // D:100 is D's covariant CoonsPatch::dup(); the port's dup() returns the base handle
    // (wave-02-brief 4b), so the derived type comes back through dynamic_pointer_cast —
    // the same run-time type test D resolves statically.
    tfi_surface = std::dynamic_pointer_cast<CoonsPatch>(other.tfi_surface->dup());
    compute_background_mesh();
}

// D:104-108 — back through the checking constructor, with _nx/_ny preserved.
std::shared_ptr<ParametricSurface> AOPatch::dup() const {
    return std::make_shared<AOPatch>(south, north, west, east, _nx, _ny);
}

// D:110-135
Vector3 AOPatch::operator()(double r, double s) const {
    // Use TFI close to the boundaries.
    // The background mesh is usually pretty coarse.
    double tol = 1.0e-4;
    if (r < tol || s < tol || r > 1.0 - tol || s > 1.0 - tol) {
        Vector3 p = (*tfi_surface)(r, s);
        return p;
    }
    // Interpolate within the AO background mesh.
    // This involves finding the relevant coarse cell and
    // using a bilinear interpolation within that cell.
    double dr = 1.0 / _nx;
    double ds = 1.0 / _ny;
    int ix_coarse = static_cast<int>(r / dr); // D:124 to!int, truncates toward zero
    int iy_coarse = static_cast<int>(s / ds); // D:125
    // Parametric coordinate within the coarse cell.
    double local_r = (r - dr * ix_coarse) / dr;
    double local_s = (s - ds * iy_coarse) / ds;
    // BiLinear interpolation for each component.
    Vector3 p = (1.0 - local_r) * (1.0 - local_s) * _bgmesh[ix_coarse][iy_coarse] +
                (1.0 - local_r) * local_s * _bgmesh[ix_coarse][iy_coarse + 1] +
                local_r * (1.0 - local_s) * _bgmesh[ix_coarse + 1][iy_coarse] +
                local_r * local_s * _bgmesh[ix_coarse + 1][iy_coarse + 1];
    return p;
} // end opCall()

// D:137-145
std::string AOPatch::toString() const {
    return "AOPatch(south=" + d_tostr(south) + ", north=" + d_tostr(north) + ", west=" +
           d_tostr(west) + ", east=" + d_tostr(east) + ", nx=" + eil::to_string(_nx) +
           ", ny=" + eil::to_string(_ny) + ")";
} // end toString()

// Not a D symbol (see header note).
std::string AOPatch::classString() const { return "AOPatch"; }

// D:148-227
void AOPatch::compute_background_mesh() {
    // Inflate array for holding nicely orthogonal mesh.
    _bgmesh.set_length(_nx + 1);
    for (int ix = 0; ix < _nx + 1; ++ix) _bgmesh[ix].set_length(_ny + 1); // D:152
    // Initial positions of the mesh points are just TFI locations.
    double dXi = 1.0 / _nx;
    double dEta = 1.0 / _ny;
    for (int ix = 0; ix < _nx + 1; ++ix) {
        for (int iy = 0; iy < _ny + 1; ++iy) {
            double r = dXi * ix;
            double s = dEta * iy;
            _bgmesh[ix][iy] = (*tfi_surface)(r, s);
        }
    }
    // Now, adjust the mesh point locations.
    // Note that the current formulation is for the (x,y)-plane only.
    // z-components remain as the TFI value.
    double dXi2 = dXi * dXi;
    double dEta2 = dEta * dEta;
    double x_tol = 1.0e-6;
    double y_tol = 1.0e-6;
    double largest_x_move;
    double largest_y_move;
    int max_count = 500;
    int count;
    for (count = 1; count <= max_count; ++count) {
        largest_x_move = 0.0;
        largest_y_move = 0.0;
        // Adjust the internal points only.
        for (int ix = 1; ix < _nx; ++ix) {
            for (int iy = 1; iy < _ny; ++iy) {
                // Save the old position.
                double x_old = eil::re(_bgmesh[ix][iy].x);
                double y_old = eil::re(_bgmesh[ix][iy].y);
                // Calculate the partial derivatives.
                double dxdXi = (eil::re(_bgmesh[ix + 1][iy].x) -
                                eil::re(_bgmesh[ix - 1][iy].x)) / (2.0 * dXi);
                double dxdEta = (eil::re(_bgmesh[ix][iy + 1].x) -
                                 eil::re(_bgmesh[ix][iy - 1].x)) / (2.0 * dEta);
                double d2xdXidEta = ((eil::re(_bgmesh[ix + 1][iy + 1].x) -
                                      eil::re(_bgmesh[ix - 1][iy + 1].x)) -
                                     (eil::re(_bgmesh[ix + 1][iy - 1].x) -
                                      eil::re(_bgmesh[ix - 1][iy - 1].x))) /
                    (4.0 * dXi * dEta);
                double dydXi = (eil::re(_bgmesh[ix + 1][iy].y) -
                                eil::re(_bgmesh[ix - 1][iy].y)) / (2.0 * dXi);
                double dydEta = (eil::re(_bgmesh[ix][iy + 1].y) -
                                 eil::re(_bgmesh[ix][iy - 1].y)) / (2.0 * dEta);
                double d2ydXidEta = ((eil::re(_bgmesh[ix + 1][iy + 1].y) -
                                      eil::re(_bgmesh[ix - 1][iy + 1].y)) -
                                     (eil::re(_bgmesh[ix + 1][iy - 1].y) -
                                      eil::re(_bgmesh[ix - 1][iy - 1].y))) /
                    (4.0 * dXi * dEta);
                // Calculate intermediate quantities
                double B = dxdXi * dydEta + dxdEta * dydXi;
                double Ax = (4.0 * dxdXi * dxdEta * d2xdXidEta + 2.0 * B * d2ydXidEta) *
                            (dXi2 * dEta2);
                double Ay = (4.0 * dydXi * dydEta * d2ydXidEta + 2.0 * B * d2xdXidEta) *
                            (dXi2 * dEta2);
                double g11 = dxdXi * dxdXi + dydXi * dydXi;
                double g22 = dxdEta * dxdEta + dydEta * dydEta;
                // Update the node position.
                double numer = Ax + g22 * dEta2 *
                                 (eil::re(_bgmesh[ix + 1][iy].x) + eil::re(_bgmesh[ix - 1][iy].x)) +
                             g11 * dXi2 *
                                 (eil::re(_bgmesh[ix][iy + 1].x) + eil::re(_bgmesh[ix][iy - 1].x));
                double denom = 2.0 * (g22 * dEta2 + g11 * dXi2);
                _bgmesh[ix][iy].x = numer / denom;

                numer = Ay + g22 * dEta2 *
                          (eil::re(_bgmesh[ix + 1][iy].y) + eil::re(_bgmesh[ix - 1][iy].y)) +
                        g11 * dXi2 *
                          (eil::re(_bgmesh[ix][iy + 1].y) + eil::re(_bgmesh[ix][iy - 1].y));
                denom = 2.0 * (g22 * dEta2 + g11 * dXi2);
                _bgmesh[ix][iy].y = numer / denom;
                // D:212-213 fabs only clears the sign bit, so Phobos and glibc cannot
                // differ in bits; a NaN move compares false, leaving the largest untouched.
                double dx = std::fabs(_bgmesh[ix][iy].x - x_old);
                double dy = std::fabs(_bgmesh[ix][iy].y - y_old);
                if (dx > largest_x_move) largest_x_move = dx;
                if (dy > largest_y_move) largest_y_move = dy;
                // writefln("Iteration %d node[%d, %d] x,y(%f, %f)\n",
                //          count, ix, iy, _bgmesh[ix][iy].x, _bgmesh[ix][iy].y);
            } // iy loop
        } // ix loop
        // Check for convergence
        if (largest_x_move <= x_tol && largest_y_move <= y_tol) break;
    } // count loop
    if (count > max_count) {
        // D:224 — stdout, during construction. Measured at corpus rows 700 and 727.
        eil::writeln("Warning: AO iteration did not converge.");
    }
    return;
} // end compute_background_mesh()

} // namespace geom
} // namespace eil
