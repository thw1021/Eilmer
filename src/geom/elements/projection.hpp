#pragma once
// geom/elements/projection.d -> src/geom/elements/projection.hpp
// (module geom.elements.projection).
//
// Projections and mappings in Vector3 space: line/segment intersection, projection
// of a point onto a plane along a direction, wrapping a neutral plane onto a
// cylinder, and barycentric (mean-value) coordinates in triangles and quads.
//
// Non-1:1 notes (registered in provenance/fidelity.yml):
//   * `intersect2D(..., out double t)` (D:16-17) maps to `double& t`, as briefed.
//     Measured against ldc2 (probes/geom/elements/probe_projection.d, rows
//     "parallel.*"): D's `out` parameter is assigned the type's .init on entry, and
//     `double.init` is NaN, so the parallel path returns with t = NaN even though
//     the caller passed 7.0 or 0.0. The port therefore resets t to NaN on entry;
//     leaving the caller's value in place would not match D.
//   * D's Vector3 parameters are `const Vector3` by value (D:16); the port binds them
//     as `const Vector3&`. Nothing is mutated, and D's copy of a struct is a value
//     copy, so the observable behaviour is the same with fewer copies.
//   * `number[3]`/`number[4] barycentricCoords` return D *static* arrays (D:98, :137)
//     -> std::array<number,3>/<4>. The three- and four-point overloads stay
//     distinguishable at call sites by arity (4 arguments vs 5) as well as by return
//     type, exactly as in D.
//   * The four-point form calls xyplane_area from properties.d (D:148-151) and throws
//     GeometryException("Quadrilateral is not convex.") when a corner triangle has
//     negative signed area (D:152-155). The throw goes through EIL_THROW_GEOM so the
//     file/line recorded is this module's throw site, matching what ldc2 reports for
//     D (projection.d, not geometry_exception.d).
//   * projection.d:136 marks that four-point function `@nogc` while D:154 throws
//     `new GeometryException(...)` inside it. ldc2 1.40.0 rejects that combination
//     whenever the body is analysed (-i, or -unittest), so the module's own unittest
//     block does not compile in the reference tree; the measurement was taken against
//     a shadow copy with that single token deleted (see the probe header). C++ has no
//     @nogc, so the port compiles as written and the attribute is not carried over.
//   * `version(complex_numbers)` blocks stay uncompiled under EIL_COMPLEX_NUMBERS
//     (backlog T-904), in the ported unittest as in tests/test_geom_elements_projection.cpp.

#include <array>
#include <cstddef>

#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"

namespace eil {
namespace geom {

// D:15-37. Determine the intersection of a projected line with a line segment.
// Input:
//     p0->p1 line segment
//     ps starting point for projected line
//     d direction of projected line
// Output:
//     t parametric position of intersection along the line segment
// Returns:
//     true, if the lines cross;
//     false, if the lines are parallel
bool intersect2D(const Vector3& p0, const Vector3& p1, const Vector3& ps, const Vector3& d,
                 double& t);

// D:45-72. Project the point q onto the plane, along the vector qr.
// Returns 0 when q has been projected, 1 when qr is parallel to the plane and q is
// on it, 2 when qr is parallel and q is off it.
int project_onto_plane(Vector3& q, const Vector3& qr, const Vector3& a, const Vector3& b,
                       const Vector3& c);

// D:74-85. Map space so that a neutral plane wraps onto a cylinder of radius H.
void map_neutral_plane_to_cylinder(Vector3& p, number H);

// D:87-111. Barycentric coordinates of point p in a triangle, considering the
// (x,y)-plane components only.
std::array<number, 3> barycentricCoords(const Vector3& p, const Vector3& p0, const Vector3& p1,
                                        const Vector3& p2);

// D:113-123. True if the coordinates are outside the triangle to within tol.
bool is_outside_triangle(std::array<number, 3> lmbda, double tol = 1.0e-10);

// D:125-171. Mean-value (Wachpress) coordinates of point p in a convex quadrilateral,
// considering the (x,y)-plane components only. Throws GeometryException if the quad is
// not convex.
std::array<number, 4> barycentricCoords(const Vector3& p, const Vector3& p0, const Vector3& p1,
                                        const Vector3& p2, const Vector3& p3);

// D:173-183. True if the coordinates are outside the quad to within tol.
bool is_outside_quad(std::array<number, 4> lmbda, double tol = 1.0e-10);

} // namespace geom
} // namespace eil
