// geom/elements/projection.d -> src/geom/elements/projection.cpp
//
// Definitions in D source order: intersect2D (D:15-37), project_onto_plane
// (D:45-72), map_neutral_plane_to_cylinder (D:74-85), barycentricCoords 3-point
// (D:97-111), is_outside_triangle (D:113-123), barycentricCoords 4-point
// (D:136-171), is_outside_quad (D:173-183).

#include "geom/elements/projection.hpp"

#include <array>
#include <cmath>
#include "eil/math.hpp"
#include <limits>

#include "eil/error.hpp"
#include "eil/number.hpp"
#include "geom/geometry_exception.hpp"
#include "geom/elements/properties.hpp"
#include "geom/elements/vector3.hpp"

namespace eil {
namespace geom {

// D:15-37
bool intersect2D(const Vector3& p0, const Vector3& p1, const Vector3& ps, const Vector3& d,
                 double& t) {
    // D's `out double t` assigns double.init to t when the callee is entered, and
    // double.init is NaN (measured: probe_projection "parallel.found=false t17=nan"
    // from a caller that passed 7.0, and again from one that passed 0.0).
    t = std::numeric_limits<double>::quiet_NaN();
    number delx = p1.x - p0.x;
    number dely = p1.y - p0.y;
    number denom = delx * d.y - dely * d.x;
    number segmentSize = std::fabs(delx) + std::fabs(dely);
    if (std::fabs(denom) < 1.0e-16 * segmentSize) { return false; } // d is parallel to line segment
    t = eil::re((ps.x - p0.x) * d.y - (ps.y - p0.y) * d.x) / eil::re(denom);
    return true;
} // end intersect2D()

//------------------------------------------------------------------------
// Geometry functions projection and mapping.

// D:45-72
int project_onto_plane(Vector3& q, const Vector3& qr, const Vector3& a, const Vector3& b,
                       const Vector3& c) {
    // See Section 7.2 in
    // J. O'Rourke (1998)
    // Computational Geometry in C (2nd Ed.)
    // Cambridge Uni Press
    // See 3D CFD workbook p17, 25-Jan-2006

    // Define a plane Ax + By + Cz = D using the corners of the triangle abc.
    Vector3 N = cross(a - c, b - c); // N = Vector3(A, B, C)
    number D = dot(a, N);

    number numer = D - dot(q, N);
    number denom = dot(qr, N);

    double tol = 1.0e-12; // floating point tolerance
    if (std::fabs(denom) < tol) {
        if (std::fabs(numer) < tol) {
            return 1; // qr is parallel to the plane and q is on the plane
        } else {
            return 2; // qr is parallel to the plane and q is off the plane
        }
    } else {
        q = q + (numer / denom) * qr;
        return 0; // point q has been projected onto the plane.
    }
} // end project_onto_plane()

// D:74-85
void map_neutral_plane_to_cylinder(Vector3& p, number H) {
    // The axis of the hypothetical cylinder coincides with the x-axis thus
    // H is also the distance of the neutral plane above the x-axis.
    // For Hannes Wojciak and Paul Petrie-Repar's turbomachinery grids.
    if (H > 0.0) {
        number theta = p.y / H;
        p.set(p.x, p.z * eil::sin(theta), p.z * eil::cos(theta));
    }
    return;
}

/**
 * Find Barycentric Coordinates of point p in triangle,
 * considering (x,y)-plane components, only.
 *  p2
 *   |\
 *   | \
 *   |  \
 *   |   \
 *  p0----p1
 */
// D:97-111
std::array<number, 3> barycentricCoords(const Vector3& p, const Vector3& p0, const Vector3& p1,
                                        const Vector3& p2) {
    // Transcribe equations from Wikipedia article "Barycentric coordinate system",
    // subsection on "Conversion between barycentric and Cartesian coordinates".
    std::array<number, 3> lmbda{};
    number numer0 = (p1.y - p2.y) * (p.x - p2.x) + (p2.x - p1.x) * (p.y - p2.y);
    number denom = (p1.y - p2.y) * (p0.x - p2.x) + (p2.x - p1.x) * (p0.y - p2.y);
    lmbda[0] = numer0 / denom;
    number numer1 = (p2.y - p0.y) * (p.x - p2.x) + (p0.x - p2.x) * (p.y - p2.y);
    lmbda[1] = numer1 / denom;
    lmbda[2] = 1.0 - lmbda[0] - lmbda[1];
    return lmbda;
} // end barycentricCoords()

// D:113-123
bool is_outside_triangle(std::array<number, 3> lmbda, double tol) {
    bool is_outside = false;
    if (lmbda[0] < -tol) { is_outside = true; }
    if (lmbda[1] < -tol) { is_outside = true; }
    if (lmbda[2] < -tol) { is_outside = true; }
    // Now, we have determined that the coordinates are for a point
    // inside or on an edge or at a vertex, to within the tolerance.
    return is_outside;
} // end is_outside_triangle()

/**
 * Find Barycentric Coordinates of point p in a quadrilateral polygon,
 * considering (x,y)-plane components, only.
 *  p3-----p2
 *   |\   /|
 *   | \ / |
 *   |  p  |
 *   | / \ |
 *   |/   \|
 *  p0-----p1
 */
// D:136-171
std::array<number, 4> barycentricCoords(const Vector3& p, const Vector3& p0, const Vector3& p1,
                                        const Vector3& p2, const Vector3& p3) {
    // Compute the normalized barycentric coordinates for a point within
    // a convex quadrilateral polygon, as described in Michael Floater's
    // 2003 paper "Mean Value Coordinates" in the journal
    // Computer Aided Geometric Design Vol 20, issue 1, pages 19-27.
    //
    // Signed areas of triangular corners.
    std::array<number, 4> Acorner{};
    Acorner[0] = xyplane_area(p3, p0, p1);
    Acorner[1] = xyplane_area(p0, p1, p2);
    Acorner[2] = xyplane_area(p1, p2, p3);
    Acorner[3] = xyplane_area(p2, p3, p0);
    if ((Acorner[0] < 0.0) || (Acorner[1] < 0.0) ||
        (Acorner[2] < 0.0) || (Acorner[3] < 0.0)) {
        // D:154 `throw new GeometryException(...)`, whose __FILE__/__LINE__ defaults
        // name this throw site (measured with ldc2 against the reference tree).
        EIL_THROW_GEOM("Quadrilateral is not convex.");
    }
    // Signed areas of triangles associated with edges and point p.
    std::array<number, 4> Amid{};
    Amid[0] = xyplane_area(p0, p1, p);
    Amid[1] = xyplane_area(p1, p2, p);
    Amid[2] = xyplane_area(p2, p3, p);
    Amid[3] = xyplane_area(p3, p0, p);
    // Wachpress coordinates computed as per the final remarks in Floater's paper.
    std::array<number, 4> lmbda{};
    lmbda[0] = Acorner[0] * Amid[1] * Amid[2];
    lmbda[1] = Acorner[1] * Amid[2] * Amid[3];
    lmbda[2] = Acorner[2] * Amid[3] * Amid[0];
    lmbda[3] = Acorner[3] * Amid[0] * Amid[1];
    number scale = 1.0 / (lmbda[0] + lmbda[1] + lmbda[2] + lmbda[3]);
    for (size_t i = 0; i < 4; ++i) { lmbda[i] *= scale; }
    return lmbda;
} // end barycentricCoords()

// D:173-183
bool is_outside_quad(std::array<number, 4> lmbda, double tol) {
    bool is_outside = false;
    for (size_t i = 0; i < 4; ++i) {
        if (lmbda[i] < -tol) { is_outside = true; }
    }
    // Now, we have determined that the coordinates are for a point
    // inside or on an edge or at a vertex, to within the tolerance.
    return is_outside;
} // end is_outside_quad()

} // namespace geom
} // namespace eil
