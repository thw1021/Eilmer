#pragma once
// geom/elements/properties.d -> src/geom/elements/properties.hpp
// (module geom.elements.properties).
//
// Cell and point geometric properties used by the finite-volume code: triangle and
// quad centroid/normal/tangents/area, xy-plane areas and cell widths, tetrahedron,
// pyramid, wedge and hexahedron volumes and centroids, and the point-in-cell tests.
//
// Non-1:1 notes (registered in provenance/fidelity.yml):
//   * properties.d declares **no classes** — every entry point is a free `@nogc`
//     function taking `ref` parameters, so there is no virtual dispatch to preserve
//     and no reference-semantics type that needs std::shared_ptr. The wave-02 brief
//     anticipated D classes here; the measured tree has none, and this port adds no
//     class hierarchy that D does not have. `ref` in/out parameters map to C++
//     references, which keeps D's in-place write-through behaviour.
//   * D's `ref const(Vector3)[] p` array overloads (properties.d:142, :319, :348,
//     :432, :492, :531, :583, :676) are ported as constrained templates over any
//     indexable range with length(), because eil::Slice<const Vector3> is not
//     constructible from eil::Array<Vector3>/eil::Slice<Vector3> in the substrate
//     (substrate gap, reported). The templates forward to the explicit-argument form
//     in D's order, exactly as D's one-line bodies do.
//   * `smallButSignificantVolume` and `verySmallVolume` (properties.d:358-359) are
//     mutable module globals in D, not constants, so they are extern globals here
//     (writable, as D leaves them).
//   * `tol` is a declared-but-unused parameter of triangle_properties (D:53),
//     quad_properties (D:102, :145) and their array overloads. It is kept in the
//     signature because D's own call sites pass it positionally; the body ignores it
//     exactly as D's does.
//   * In xyplane_quad_cell_properties (D:177) and xyplane_quad_squareness (D:240) the
//     local/parameter is *named* `xyplane_area`, shadowing the module function
//     xyplane_area (D:151, :164). D resolves those uses to the variable, and so does
//     C++; the shadowing name is kept rather than renamed.
//   * D writes the squares as `x ^^ 2` (11 lines, 19 reachable sites in this module).
//     Those are eil::sqr — one multiply: measured on 2,222 operands, D's `x ^^ 2` equals
//     `x*x` on every one of them, while glibc pow(x,2.0) (what a port `eil::pow(x,2)`
//     becomes) differs on 3 (backlog T-931). `pow(volume, third)` (D:336, :478, :567) is
//     eil::pow too, because a non-integer exponent really is Phobos' pow.
//   * For an inverted cell, `pow(volume, 1/3)` is a NaN and D's `fmin` swallows it
//     (measured: probes/geom/elements/probe_properties.d row "nan.cbrt" prints
//     pow3=nan but fmin=0.4082). std::fmin/std::fmax reproduce that, so L_min keeps
//     coming out of the face-distance term for negative volumes.

#include <cmath>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

#include "eil/array.hpp"
#include "eil/math.hpp"
#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"

namespace eil {
namespace geom {

namespace detail {

// Any D `T[]`-shaped read range: has length() and operator[](size_t) yielding
// something convertible to `const Vector3&`.
template <typename C, typename = void> struct is_vertex_range : std::false_type {};
template <typename C>
struct is_vertex_range<
    C, std::void_t<decltype(std::declval<const C&>().length()),
                   std::enable_if_t<std::is_convertible<
                       decltype(std::declval<const C&>()[std::size_t(0)]),
                       const Vector3&>::value>>> : std::true_type {};

} // namespace detail

// D:14-31
bool inside_bounding_box(const Vector3& p, const Vector3& bb0, const Vector3& bb1,
                         int dimensions);

// D:47-86
void triangle_properties(const Vector3& p0, const Vector3& p1, const Vector3& p2,
                         Vector3& centroid, Vector3& n, Vector3& t1, Vector3& t2, number& area,
                         double tol = 1.0e-12, double area_tol = 1.0e-20);

// D:96-139
void quad_properties(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3,
                     Vector3& centroid, Vector3& n, Vector3& t1, Vector3& t2, number& area,
                     double tol = 1.0e-12, double area_tol = 1.0e-20);

// D:141-148, `ref const(Vector3)[] p`
template <typename C, typename = std::enable_if_t<detail::is_vertex_range<C>::value>>
void quad_properties(const C& p, Vector3& centroid, Vector3& n, Vector3& t1, Vector3& t2,
                     number& area, double tol = 1.0e-12, double area_tol = 1.0e-20) {
    quad_properties(p[0], p[1], p[2], p[3], centroid, n, t1, t2, area, tol, area_tol);
}

// D:150-161 and D:163-174
number xyplane_area(const Vector3& p0, const Vector3& p1, const Vector3& p2);
number xyplane_area(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3);

// D:176-237
void xyplane_quad_cell_properties(const Vector3& p0, const Vector3& p1, const Vector3& p2,
                                  const Vector3& p3, Vector3& centroid, number& xyplane_area,
                                  number& iLen, number& jLen, number& minLen);

// D:239-249
number xyplane_quad_squareness(const Vector3& p0, const Vector3& p1, const Vector3& p2,
                               const Vector3& p3);

// D:251-286
void xyplane_triangle_cell_properties(const Vector3& p0, const Vector3& p1, const Vector3& p2,
                                      Vector3& centroid, number& xyplane_area, number& iLen,
                                      number& jLen, number& minLen);

// D:292-304
number tetrahedron_volume(const Vector3& p0, const Vector3& p1, const Vector3& p2,
                          const Vector3& p3);

// D:306-316, variant without L_min
void tetrahedron_properties(const Vector3& p0, const Vector3& p1, const Vector3& p2,
                            const Vector3& p3, Vector3& centroid, number& volume);

// D:318-323
template <typename C, typename = std::enable_if_t<detail::is_vertex_range<C>::value>>
void tetrahedron_properties(const C& p, Vector3& centroid, number& volume) {
    tetrahedron_properties(p[0], p[1], p[2], p[3], centroid, volume);
}

// D:325-345, variant including L_min
void tetrahedron_properties(const Vector3& p0, const Vector3& p1, const Vector3& p2,
                            const Vector3& p3, Vector3& centroid, number& volume, number& L_min);

// D:347-352
template <typename C, typename = std::enable_if_t<detail::is_vertex_range<C>::value>>
void tetrahedron_properties(const C& p, Vector3& centroid, number& volume, number& L_min) {
    tetrahedron_properties(p[0], p[1], p[2], p[3], centroid, volume, L_min);
}

// D:354-359 — mutable module globals (see the header note).
extern double smallButSignificantVolume;
extern double verySmallVolume;

// D:362-387
number tetragonal_dipyramid_volume(const Vector3& p0, const Vector3& p1, const Vector3& p2,
                                   const Vector3& p3, const Vector3& pb, const Vector3& pc);

// D:389-429
void pyramid_properties(const Vector3& p0, const Vector3& p1, const Vector3& p2,
                        const Vector3& p3, const Vector3& p4, bool true_centroid,
                        Vector3& centroid, number& volume);

// D:431-436
template <typename C, typename = std::enable_if_t<detail::is_vertex_range<C>::value>>
void pyramid_properties(const C& p, bool true_centroid, Vector3& centroid, number& volume) {
    pyramid_properties(p[0], p[1], p[2], p[3], p[4], true_centroid, centroid, volume);
}

// D:438-489
void pyramid_properties(const Vector3& p0, const Vector3& p1, const Vector3& p2,
                        const Vector3& p3, const Vector3& p4, bool true_centroid,
                        Vector3& centroid, number& volume, number& L_min);

// D:491-496
template <typename C, typename = std::enable_if_t<detail::is_vertex_range<C>::value>>
void pyramid_properties(const C& p, bool true_centroid, Vector3& centroid, number& volume,
                        number& L_min) {
    pyramid_properties(p[0], p[1], p[2], p[3], p[4], true_centroid, centroid, volume, L_min);
}

// D:498-528
void wedge_properties(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3,
                      const Vector3& p4, const Vector3& p5, bool true_centroid,
                      Vector3& centroid, number& volume);

// D:530-535
template <typename C, typename = std::enable_if_t<detail::is_vertex_range<C>::value>>
void wedge_properties(const C& p, bool true_centroid, Vector3& centroid, number& volume) {
    wedge_properties(p[0], p[1], p[2], p[3], p[4], p[5], true_centroid, centroid, volume);
}

// D:537-580
void wedge_properties(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3,
                      const Vector3& p4, const Vector3& p5, bool true_centroid,
                      Vector3& centroid, number& volume, number& L_min);

// D:582-587
template <typename C, typename = std::enable_if_t<detail::is_vertex_range<C>::value>>
void wedge_properties(const C& p, bool true_centroid, Vector3& centroid, number& volume,
                      number& L_min) {
    wedge_properties(p[0], p[1], p[2], p[3], p[4], p[5], true_centroid, centroid, volume, L_min);
}

// D:589-673
void hex_cell_properties(const Vector3& p0, const Vector3& p1, const Vector3& p2,
                         const Vector3& p3, const Vector3& p4, const Vector3& p5,
                         const Vector3& p6, const Vector3& p7, bool true_centroid,
                         Vector3& centroid, number& volume, number& iLen, number& jLen,
                         number& kLen);

// D:675-682
template <typename C, typename = std::enable_if_t<detail::is_vertex_range<C>::value>>
void hex_cell_properties(const C& p, bool true_centroid, Vector3& centroid, number& volume,
                         number& iLen, number& jLen, number& kLen) {
    hex_cell_properties(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], true_centroid, centroid,
                        volume, iLen, jLen, kLen);
}

// D:688-716
int inside_triangle(const Vector3& p, const Vector3& a, const Vector3& b, const Vector3& c);

// D:724-731
bool on_left_of_xy_line(const Vector3& a, const Vector3& b, const Vector3& p);

// D:733-744, `ref const(Vector3[]) vtx`
template <typename C, typename = std::enable_if_t<detail::is_vertex_range<C>::value>>
bool inside_xy_polygon(const C& vtx, const Vector3& p) {
    unsigned count_on_left = 0;
    for (size_t i = 0; i < vtx.length(); ++i) {
        size_t ip1 = i + 1;
        if (ip1 == vtx.length()) ip1 = 0; // wrap around
        if (on_left_of_xy_line(vtx[i], vtx[ip1], p)) count_on_left += 1;
    }
    return (count_on_left == vtx.length());
} // end inside_xy_polygon()

// D:746-759
bool inside_xy_triangle(const Vector3& p0, const Vector3& p1, const Vector3& p2,
                        const Vector3& p);

// D:761-773
bool inside_xy_quad(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3,
                    const Vector3& p);

// D:775-795. D's parameter is a mutable `Vector3[]`, but the body only reads through
// the element pointers, so the ported form takes any read range.
template <typename C, typename = std::enable_if_t<detail::is_vertex_range<C>::value>>
int winding_number(const C& vertices, const Vector3& p) {
    int wn = 0;
    Vector3 pr = Vector3(p.x + 1.0, p.y);
    const size_t nv = vertices.length();
    for (size_t i = 0; i < nv; ++i) {
        // Look at each segment of the polygon.
        const Vector3* a = (i == 0) ? &(vertices[nv - 1]) : &(vertices[i - 1]);
        const Vector3* b = &(vertices[i]);
        if (on_left_of_xy_line(p, pr, *b) && !on_left_of_xy_line(p, pr, *a) &&
            on_left_of_xy_line(*a, *b, p)) {
            ++wn;
        }
        if (!on_left_of_xy_line(p, pr, *b) && on_left_of_xy_line(p, pr, *a) &&
            !on_left_of_xy_line(*a, *b, p)) {
            --wn;
        }
    }
    return wn;
} // end winding_number()

// D:799-811
bool inside_tetrahedron(const Vector3& p0, const Vector3& p1, const Vector3& p2,
                        const Vector3& p3, const Vector3& p);

// D:813-841
bool inside_pyramid(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3,
                    const Vector3& p4, const Vector3& p);

// D:843-872
bool inside_wedge(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3,
                  const Vector3& p4, const Vector3& p5, const Vector3& p);

// D:874-907
bool inside_hexahedron(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3,
                       const Vector3& p4, const Vector3& p5, const Vector3& p6, const Vector3& p7,
                       const Vector3& p);

} // namespace geom
} // namespace eil
