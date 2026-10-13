// tests/test_geom_elements_properties.cpp
//
// Two layers, both required:
//
//  A. the `unittest` block of geom/elements/properties.d (D:909-1168) ported
//     assertion-for-assertion, in source order, with D's own tolerance arguments
//     (approxEqualNumbers defaults 1.0e-2 relative / 1.0e-5 absolute, isClose default
//     1.0e-8). The `version (complex_numbers)` blocks (D:960-988, :1005-1025,
//     :1041-1056, :1071-1085, :1098-1109, :1127-1142) are carried under
//     EIL_COMPLEX_NUMBERS and are not compiled in the trunk build (backlog T-904).
//
//  B. a line-for-line replay of probes/geom/elements/probe_properties.d against
//     baselines/e587b235/probes/geom_elements_properties.txt: 77 lines covering every
//     public function of the module over 8 cell shapes (2D quad, 3D hex, tet, wedge,
//     pyramid, triangle, degenerate and collapsed variants), all printed at %.17g.
//     The summary line reports how many printed quantities are bit-identical and the
//     worst residual in ULPs.
//
// Run: ./test_geom_elements_properties ../baselines/e587b235/probes/geom_elements_properties.txt

#include <array>
#include <cctype>
#include <cstdarg>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "geom/elements/nomenclature.hpp"
#include "geom/elements/properties.hpp"
#include "geom/elements/vector3.hpp"
#include "nm/number.hpp"

using eil::geom::Vector3;
using eil::number;
using eil::geom::inside_bounding_box;
using eil::geom::triangle_properties;
using eil::geom::quad_properties;
using eil::geom::xyplane_area;
using eil::geom::tetrahedron_volume;
using eil::geom::inside_triangle;

namespace {

int failures = 0;
int checks = 0;

void check(bool cond, const char* what, int line) {
    ++checks;
    if (!cond) {
        ++failures;
        std::printf("FAIL line %d: %s\n", line, what);
    }
}

#define CHECK(c, msg) check((c), msg, __LINE__)

std::vector<std::string> produced;

void emitf(const char* fmt, ...) {
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    produced.emplace_back(buf);
}

std::vector<double> numbers_in(const std::string& s) {
    std::vector<double> out;
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] != '=') { ++i; continue; }
        size_t b = i + 1;
        size_t e = s.find(' ', b);
        if (e == std::string::npos) e = s.size();
        const std::string tok = s.substr(b, e - b);
        if (!tok.empty()) {
            const char c = tok[0];
            if (c == '-' || c == '+' || std::isdigit(static_cast<unsigned char>(c)) || c == 'n' ||
                c == 'i') {
                char* endp = nullptr;
                const double v = std::strtod(tok.c_str(), &endp);
                if (endp == tok.c_str() + tok.size()) out.push_back(v);
            }
        }
        i = b;
    }
    return out;
}

long long ulp_gap(double a, double b) {
    int64_t ia, ib;
    std::memcpy(&ia, &a, 8);
    std::memcpy(&ib, &b, 8);
    if (ia < 0) ia = INT64_MIN - ia;
    if (ib < 0) ib = INT64_MIN - ib;
    return ia > ib ? ia - ib : ib - ia;
}

// centroid/n/t1/t2/area row, matching emit_cell() in the probe.
void emit_cell(const char* tag, const Vector3& c, const Vector3& n, const Vector3& t1,
               const Vector3& t2, number area) {
    emitf("%s cx=%.17g cy=%.17g cz=%.17g nx=%.17g ny=%.17g nz=%.17g t1x=%.17g t1y=%.17g "
          "t1z=%.17g t2x=%.17g t2y=%.17g t2z=%.17g area=%.17g",
          tag, c.x, c.y, c.z, n.x, n.y, n.z, t1.x, t1.y, t1.z, t2.x, t2.y, t2.z, area);
}

// ============================================================================
// A. the D unittest block, assertion for assertion, in D's order.
// ============================================================================
void port_d_unittest() {
    Vector3 bb0 = Vector3(1.0, 1.0, 1.0);
    Vector3 bb1 = Vector3(2.0, 2.0, 2.0);
    Vector3 p = 0.5 * (bb0 + bb1); // D: to!number(0.5) * (bb0 + bb1)
    CHECK(inside_bounding_box(p, bb0, bb1, 3), "D:913 inside_bounding_box center");
    p += Vector3(0.0, 0.0, 1.0);
    CHECK(!inside_bounding_box(p, bb0, bb1, 3), "D:915 outside in 3D");
    CHECK(inside_bounding_box(p, bb0, bb1, 2), "D:916 still inside in 2D");

    Vector3 a = Vector3(1.0, 0.0, 0.0); // plane through a,b,c
    Vector3 b = Vector3(1.0, 1.0, 0.0);
    Vector3 c = Vector3(0.5, 0.0, 0.0);
    Vector3 d = Vector3(0.65, 0.0, 0.0);
    CHECK(inside_triangle(d, a, b, c) > 0, "D:922 inside_triangle d");
    Vector3 e = Vector3(0.65, -0.1, 0.0);
    CHECK(!inside_triangle(e, a, b, c), "D:924 inside_triangle e");

    Vector3 p0 = Vector3(0.0, 0.0);
    Vector3 p1 = Vector3(1.0, -0.1);
    Vector3 p2 = Vector3(1.0, 1.0);
    Vector3 p3 = Vector3(0.0, 1.0);
    eil::Array<Vector3> poly = { p0, p1, p2, p3 };
    CHECK(eil::geom::inside_xy_polygon(poly, d), "D:931 inside_xy_polygon d");
    CHECK(!eil::geom::inside_xy_polygon(poly, e), "D:932 inside_xy_polygon e");
    CHECK(eil::geom::inside_xy_triangle(p0, p1, p2, d), "D:933 inside_xy_triangle d");
    CHECK(!eil::geom::inside_xy_triangle(p0, p1, p2, e), "D:934 inside_xy_triangle e");
    CHECK(eil::geom::inside_xy_quad(p0, p1, p2, p3, d), "D:935 inside_xy_quad d");
    CHECK(eil::geom::winding_number(poly, d) != 0, "D:936 winding_number d");
    CHECK(!eil::geom::inside_xy_quad(p0, p1, p2, p3, e), "D:937 inside_xy_quad e");
    CHECK(eil::geom::winding_number(poly, e) == 0, "D:938 winding_number e");

    // Set up a simple triangle for which we can easily compute
    // the properties by hand.
    //      p2
    //    / |
    //   /  |
    // p0---p1
    double alpha = 1.0;
    p0 = Vector3(0.0, 0.0, 1.0);
    p1 = Vector3(alpha, 0.0, 1.0);
    p2 = Vector3(alpha, alpha, 1.0);
    Vector3 centroid, n, t1, t2;
    number area = 0.0;
    triangle_properties(p0, p1, p2, centroid, n, t1, t2, area);
    CHECK(eil::nm::approxEqualNumbers(area, number(0.5)), "D:953 triangle area");
    CHECK(eil::nm::approxEqualNumbers(xyplane_area(p0, p1, p2), number(0.5)), "D:954 xyplane_area");
    CHECK(eil::geom::approxEqualVectors(centroid, Vector3(2.0 / 3, 1.0 / 3, 1.0)),
          "D:955 triangle centroid");
    CHECK(eil::geom::approxEqualVectors(n, Vector3(0.0, 0.0, 1.0)), "D:956 triangle normal");
    CHECK(eil::geom::approxEqualVectors(t1, Vector3(1.0, 0.0, 0.0)), "D:957 triangle t1");
    CHECK(eil::geom::approxEqualVectors(t2, Vector3(0.0, 1.0, 0.0)), "D:958 triangle t2");

#ifdef EIL_COMPLEX_NUMBERS
    // D:960-988 — triangle properties, complex step derivative test. Not compiled in
    // the trunk build (backlog T-904); kept verbatim so the wave-12 port can use it.
    {
        double h = 1.0e-20;
        number ih = eil::Complex<double>(0.0, h);
        number zero = 0.0;
        number one = 1.0;
        Vector3 p0_dash = Vector3(zero, zero, one);
        Vector3 p1_dash = Vector3(alpha + ih, zero, one);
        Vector3 p2_dash = Vector3(alpha + ih, alpha + ih, one);
        Vector3 centroid_dash, n_dash, t1_dash, t2_dash;
        number area_dash;
        triangle_properties(p0_dash, p1_dash, p2_dash, centroid_dash, n_dash, t1_dash, t2_dash,
                            area_dash);
        double darea_da = im(area_dash) / h;
        Vector3 dcentroid_da = Vector3(im(centroid_dash.x) / h, im(centroid_dash.y) / h,
                                       im(centroid_dash.z) / h);
        Vector3 dn_da = Vector3(im(n_dash.x) / h, im(n_dash.y) / h, im(n_dash.z) / h);
        Vector3 dt1_da = Vector3(im(t1_dash.x) / h, im(t1_dash.y) / h, im(t1_dash.z) / h);
        Vector3 dt2_da = Vector3(im(t2_dash.x) / h, im(t2_dash.y) / h, im(t2_dash.z) / h);
        CHECK(eil::geom::approxEqualVectors(dcentroid_da,
                                          Vector3(2.0 / 3.0 * alpha, 1.0 / 3.0 * alpha, 0.0)),
              "D:978 complex triangle centroid");
        CHECK(eil::geom::approxEqualVectors(dn_da, Vector3(0.0, 0.0, 0.0)),
              "D:983 complex triangle normal");
        CHECK(eil::geom::approxEqualVectors(dt1_da, Vector3(0.0, 0.0, 0.0)), "D:984 complex t1");
        CHECK(eil::geom::approxEqualVectors(dt2_da, Vector3(0.0, 0.0, 0.0)), "D:985 complex t2");
        CHECK(eil::isClose(darea_da, alpha), "D:987 complex darea/da = alpha");
    }
#endif

    // Set up a simple quadrilateral for which we can easily compute
    // the properties by hand.
    // p3---p2
    // |    |
    // |    |
    // p0---p1
    // For the quadrilateral, we reuse the triangle points but need a fourth point.
    p3 = Vector3(0.0, alpha, 1.0);
    quad_properties(p0, p1, p2, p3, centroid, n, t1, t2, area);
    CHECK(eil::nm::approxEqualNumbers(area, number(1.0)), "D:999 quad area");
    CHECK(eil::geom::approxEqualVectors(centroid, Vector3(0.5, 0.5, 1.0)), "D:1000 quad centroid");
    CHECK(eil::geom::approxEqualVectors(n, Vector3(0.0, 0.0, 1.0)), "D:1001 quad normal");
    CHECK(eil::geom::approxEqualVectors(t1, Vector3(1.0, 0.0, 0.0)), "D:1002 quad t1");
    CHECK(eil::geom::approxEqualVectors(t2, Vector3(0.0, 1.0, 0.0)), "D:1003 quad t2");

#ifdef EIL_COMPLEX_NUMBERS
    // D:1005-1025 — quad properties, complex step derivative test.
    {
        Vector3 p3_dash = Vector3(zero, alpha + ih, one);
        quad_properties(p0_dash, p1_dash, p2_dash, p3_dash, centroid_dash, n_dash, t1_dash,
                        t2_dash, area_dash);
        darea_da = im(area_dash) / h;
        dcentroid_da = Vector3(im(centroid_dash.x) / h, im(centroid_dash.y) / h,
                               im(centroid_dash.z) / h);
        dn_da = Vector3(im(n_dash.x) / h, im(n_dash.y) / h, im(n_dash.z) / h);
        dt1_da = Vector3(im(t1_dash.x) / h, im(t1_dash.y) / h, im(t1_dash.z) / h);
        dt2_da = Vector3(im(t2_dash.x) / h, im(t2_dash.y) / h, im(t2_dash.z) / h);
        CHECK(eil::geom::approxEqualVectors(dcentroid_da,
                                          Vector3(0.5 * alpha, 0.5 * alpha, 0.0)),
              "D:1015 complex quad centroid");
        CHECK(eil::geom::approxEqualVectors(dn_da, Vector3(0.0, 0.0, 0.0)), "D:1020 complex n");
        CHECK(eil::geom::approxEqualVectors(dt1_da, Vector3(0.0, 0.0, 0.0)), "D:1021 complex t1");
        CHECK(eil::geom::approxEqualVectors(dt2_da, Vector3(0.0, 0.0, 0.0)), "D:1022 complex t2");
        CHECK(eil::isClose(darea_da, 2.0 * alpha), "D:1024 complex darea/da = 2*alpha");
    }
#endif

    // Build tetrahedron with equilateral triangle (side 1.0) base on xy plane.
    p0 = Vector3(0, 0, 0);
    p1 = Vector3(std::cos(eil::geom::radians(30)), std::sin(eil::geom::radians(30)), 0.0);
    p2 = Vector3(0.0, 1.0, 0.0);
    double dx = 0.5 * std::tan(eil::geom::radians(30));
    double dL = std::cos(eil::geom::radians(30));
    double dz = eil::sqrt(dL * dL - dx * dx);
    p3 = Vector3(dx, 0.5, dz);
    number volume = 0.0;
    tetrahedron_properties(p0, p1, p2, p3, centroid, volume);
    CHECK(eil::geom::approxEqualVectors(centroid, Vector3(dx, 0.5, 0.25 * dz)), "D:1037 tet centroid");
    CHECK(eil::nm::approxEqualNumbers(volume, number(std::cos(eil::geom::radians(30)) * 0.5 * dz / 3)),
          "D:1038 tet volume");

#ifdef EIL_COMPLEX_NUMBERS
    // D:1041-1056 — tetrahedron properties, complex step derivative test.
    {
        number volume_dash;
        p0_dash = Vector3(zero, zero, zero);
        p1_dash = Vector3(alpha + ih, zero, zero);
        p2_dash = Vector3((alpha + ih) * std::cos(eil::geom::radians(60)),
                          (alpha + ih) * std::sin(eil::geom::radians(60)), zero);
        p3_dash = Vector3(0.5 * (alpha + ih), 0.5 * (alpha + ih) * std::tan(number(eil::geom::radians(30))),
                          (alpha + ih) * (eil::sqrt(6.0) / 3.0));
        tetrahedron_properties(p0_dash, p1_dash, p2_dash, p3_dash, centroid_dash, volume_dash);
        double dvol_da = im(volume_dash) / h;
        dcentroid_da = Vector3(im(centroid_dash.x) / h, im(centroid_dash.y) / h,
                               im(centroid_dash.z) / h);
        CHECK(eil::isClose(dvol_da, eil::pow(alpha, 2) / (2.0 * eil::sqrt(2.0))), "D:1052 tet dvol");
        CHECK(eil::geom::approxEqualVectors(dcentroid_da,
                                          Vector3(3.0 / 8.0 + 0.25 * std::cos(eil::geom::radians(60)),
                                                  1.0 / 4.0 * std::sin(eil::geom::radians(60)) +
                                                      1.0 / 8.0 * std::tan(eil::geom::radians(30)),
                                                  eil::sqrt(6.0) / 12.0)),
              "D:1053 tet dcentroid");
    }
#endif

    // Build a wedge with the same equilateral-triangle base.
    p0 = Vector3(0, 0, 0);
    p1 = Vector3(std::cos(eil::geom::radians(30)), std::sin(eil::geom::radians(30)), 0.0);
    p2 = Vector3(0.0, 1.0, 0.0);
    Vector3 incz = Vector3(0, 0, -1.0);
    p3 = p0 + incz;
    Vector3 p4 = p1 + incz;
    Vector3 p5 = p2 + incz;
    eil::geom::wedge_properties(p0, p1, p2, p3, p4, p5, true, centroid, volume);
    CHECK(eil::geom::approxEqualVectors(centroid, Vector3(dx, 0.5, -0.5)), "D:1067 wedge centroid");
    CHECK(eil::nm::approxEqualNumbers(volume, number(std::cos(eil::geom::radians(30)) * 0.5 * 1.0)),
          "D:1068 wedge volume");

#ifdef EIL_COMPLEX_NUMBERS
    // D:1071-1085 — wedge properties, complex step derivative test.
    {
        p0_dash = Vector3(zero, zero, zero);
        p1_dash = Vector3(alpha + ih, zero, zero);
        p2_dash = Vector3((alpha + ih) * std::cos(eil::geom::radians(60)),
                          (alpha + ih) * std::sin(eil::geom::radians(60)), zero);
        Vector3 p3_dash = p0_dash + incz;
        Vector3 p4_dash = p1_dash + incz;
        Vector3 p5_dash = p2_dash + incz;
        wedge_properties(p0_dash, p1_dash, p2_dash, p3_dash, p4_dash, p5_dash, true, centroid_dash,
                         volume_dash);
        dvol_da = im(volume_dash) / h;
        dcentroid_da = Vector3(im(centroid_dash.x) / h, im(centroid_dash.y) / h,
                               im(centroid_dash.z) / h);
        CHECK(eil::isClose(dvol_da, alpha * std::sin(eil::geom::radians(60))), "D:1082 wedge dvol");
        CHECK(eil::geom::approxEqualVectors(dcentroid_da,
                                          Vector3((1.0 / 3.0) * (1.0 + std::cos(eil::geom::radians(60))),
                                                  (1.0 / 3.0) * std::sin(eil::geom::radians(60)), 0.0)),
              "D:1083 wedge dcentroid");
    }

    // Pyramid
#endif
    p0 = Vector3(0, 0, 0);
    p1 = Vector3(1, 0, 0);
    p2 = Vector3(1, 1, 0);
    p3 = Vector3(0, 1, 0);
    p4 = Vector3(0.5, 0.5, 1); // peak
    eil::geom::pyramid_properties(p0, p1, p2, p3, p4, true, centroid, volume);
    CHECK(eil::geom::approxEqualVectors(centroid, Vector3(0.5, 0.5, 1.0 / 4)), "D:1094 pir centroid");
    CHECK(eil::nm::approxEqualNumbers(volume, number(1.0 / 3)), "D:1095 pir volume");

#ifdef EIL_COMPLEX_NUMBERS
    // D:1098-1109 — pyramid properties, complex step derivative test.
    {
        p0_dash = Vector3(zero, zero, zero);
        p1_dash = Vector3(alpha + ih, zero, zero);
        p2_dash = Vector3(alpha + ih, alpha + ih, zero);
        Vector3 p3_dash = Vector3(zero, alpha + ih, zero);
        Vector3 p4_dash = Vector3(0.5, 0.5, 1); // peak
        pyramid_properties(p0_dash, p1_dash, p2_dash, p3_dash, p4_dash, true, centroid_dash,
                           volume_dash);
        dvol_da = im(volume_dash) / h;
        dcentroid_da = Vector3(im(centroid_dash.x) / h, im(centroid_dash.y) / h,
                               im(centroid_dash.z) / h);
        CHECK(eil::isClose(dvol_da, 2.0 * alpha / 3.0), "D:1107 pyramid dvol");
        // [TODO] centroid sensitivity
    }
#endif

    // Simple cube for the hex cell.
    p0 = Vector3(0, 0, 0);
    p1 = Vector3(1, 0, 0);
    p2 = Vector3(1, 1, 0);
    p3 = Vector3(0, 1, 0);
    p4 = Vector3(0, 0, 1);
    p5 = Vector3(1, 0, 1);
    Vector3 p6 = Vector3(1, 1, 1);
    Vector3 p7 = Vector3(0, 1, 1);
    number iLen = 0.0, jLen = 0.0, kLen = 0.0;
    eil::geom::hex_cell_properties(p0, p1, p2, p3, p4, p5, p6, p7, true, centroid, volume, iLen,
                                   jLen, kLen);
    CHECK(eil::geom::approxEqualVectors(centroid, Vector3(0.5, 0.5, 0.5)), "D:1123 hex centroid");
    CHECK(eil::nm::approxEqualNumbers(volume, number(1.0)), "D:1124 hex volume");

#ifdef EIL_COMPLEX_NUMBERS
    // D:1127-1142 — hex cell properties, complex step derivative test.
    {
        p0_dash = Vector3(0, 0, 0);
        p1_dash = Vector3(alpha + ih, zero, zero);
        p2_dash = Vector3(alpha + ih, alpha + ih, zero);
        Vector3 p3_dash = Vector3(zero, alpha + ih, zero);
        Vector3 p4_dash = Vector3(zero, zero, alpha + ih);
        Vector3 p5_dash = Vector3(alpha + ih, zero, alpha + ih);
        Vector3 p6_dash = Vector3(alpha + ih, alpha + ih, alpha + ih);
        Vector3 p7_dash = Vector3(zero, alpha + ih, alpha + ih);
        hex_cell_properties(p0_dash, p1_dash, p2_dash, p3_dash, p4_dash, p5_dash, p6_dash, p7_dash,
                            true, centroid_dash, volume_dash, iLen, jLen, kLen);
        dvol_da = im(volume_dash) / h;
        dcentroid_da = Vector3(im(centroid_dash.x) / h, im(centroid_dash.y) / h,
                               im(centroid_dash.z) / h);
        CHECK(eil::isClose(dvol_da, 3.0 * eil::pow(alpha, 2)), "D:1140 hex dvol");
        CHECK(eil::geom::approxEqualVectors(dcentroid_da, Vector3(0.5, 0.5, 0.5)),
              "D:1141 hex dcentroid");
    }
#endif

    d = Vector3(0.65, 0.0, 0.0);
    e = Vector3(0.65, -0.1, 0.0);

    p0 = Vector3(0.0, 0.0, 0.0);
    p1 = Vector3(1.0, -0.1, 0.0);
    p2 = Vector3(1.0, 1.0, 0.0);
    p3 = Vector3(0.0, 1.0, 0.0);
    p4 = Vector3(0.0, 0.0, 1.0);
    p5 = Vector3(1.0, -0.1, 1.0);
    p6 = Vector3(1.0, 1.0, 1.0);
    p7 = Vector3(0.0, 1.0, 1.0);
    CHECK(eil::geom::inside_hexahedron(p0, p1, p2, p3, p4, p5, p6, p7, d), "D:1155 in hex d");
    CHECK(!eil::geom::inside_hexahedron(p0, p1, p2, p3, p4, p5, p6, p7, e), "D:1156 in hex e");

    Vector3 f = Vector3(0.1, 0.0, 0.5);
    CHECK(eil::geom::inside_tetrahedron(p0, p1, p2, p4, f), "D:1159 in tet f1");
    f = Vector3(0.0, 0.2, 0.5);
    CHECK(!eil::geom::inside_tetrahedron(p0, p1, p2, p4, f), "D:1161 in tet f2");
    f = Vector3(0.0, -0.2, 0.5);
    CHECK(!eil::geom::inside_tetrahedron(p0, p1, p2, p4, f), "D:1163 in tet f3");
    f = Vector3(1.0, 0.0, 1.0);
    CHECK(!eil::geom::inside_tetrahedron(p0, p1, p2, p4, f), "D:1165 in tet f4");
    f = Vector3(0.0, 0.0, -0.5);
    CHECK(!eil::geom::inside_tetrahedron(p0, p1, p2, p4, f), "D:1167 in tet f5");
}

// ============================================================================
// B. the probe replay. Same cells, same calls, same print order as
//    probes/geom/elements/probe_properties.d.
// ============================================================================
void replay_probe() {
    using namespace eil::geom;

    Vector3 bb0 = Vector3(1.0, 1.0, 1.0);
    Vector3 bb1 = Vector3(2.0, 2.0, 2.0);
    Vector3 pm = 0.5 * (bb0 + bb1);
    Vector3 pz = pm + Vector3(0.0, 0.0, 1.0);
    Vector3 bl = Vector3(0.9999999, 1.0, 1.0);
    emitf("bbox.center dim3=%s dim2=%s", eil::to_string(inside_bounding_box(pm, bb0, bb1, 3)).c_str(),
          eil::to_string(inside_bounding_box(pm, bb0, bb1, 2)).c_str());
    emitf("bbox.zplus1 dim3=%s dim2=%s", eil::to_string(inside_bounding_box(pz, bb0, bb1, 3)).c_str(),
          eil::to_string(inside_bounding_box(pz, bb0, bb1, 2)).c_str());
    emitf("bbox.lo dim3=%s dim2=%s", eil::to_string(inside_bounding_box(bb0, bb0, bb1, 3)).c_str(),
          eil::to_string(inside_bounding_box(bb0, bb0, bb1, 2)).c_str());
    emitf("bbox.hi dim3=%s dim2=%s", eil::to_string(inside_bounding_box(bb1, bb0, bb1, 3)).c_str(),
          eil::to_string(inside_bounding_box(bb1, bb0, bb1, 2)).c_str());
    emitf("bbox.below dim3=%s dim2=%s",
          eil::to_string(inside_bounding_box(bl, bb0, bb1, 3)).c_str(),
          eil::to_string(inside_bounding_box(bl, bb0, bb1, 2)).c_str());

    double alpha = 1.0;
    Vector3 p0 = Vector3(0.0, 0.0, 1.0);
    Vector3 p1 = Vector3(alpha, 0.0, 1.0);
    Vector3 p2 = Vector3(alpha, alpha, 1.0);
    Vector3 centroid, n, t1, t2;
    number area = 0.0;
    triangle_properties(p0, p1, p2, centroid, n, t1, t2, area);
    emit_cell("tri.unit", centroid, n, t1, t2, area);
    emitf("tri.unit.text centroid=%s n=%s", centroid.toString().c_str(), n.toString().c_str());

    alpha = 0.7;
    p1 = Vector3(alpha, 0.0, 1.0);
    p2 = Vector3(alpha, alpha, 1.0);
    triangle_properties(p0, p1, p2, centroid, n, t1, t2, area);
    emit_cell("tri.alpha0.7", centroid, n, t1, t2, area);
    alpha = 1.0;
    p1 = Vector3(alpha, 0.0, 1.0);
    p2 = Vector3(alpha, alpha, 1.0);

    Vector3 s0 = Vector3(0.0, 0.0, 0.0);
    Vector3 s1 = Vector3(1.0, 0.2, 0.3);
    Vector3 s2 = Vector3(-0.4, 1.0, 0.6);
    triangle_properties(s0, s1, s2, centroid, n, t1, t2, area);
    emit_cell("tri.skew3d", centroid, n, t1, t2, area);

    triangle_properties(s0, s0, s1, centroid, n, t1, t2, area);
    emit_cell("tri.degenerate", centroid, n, t1, t2, area);
    // decimal literals, not folded sums — see the note in the probe header
    Vector3 sliver = Vector3(1.0000000001, 0.2000000001, 0.3);
    triangle_properties(s0, s1, sliver, centroid, n, t1, t2, area);
    emit_cell("tri.sliver", centroid, n, t1, t2, area);

    Vector3 p3 = Vector3(0.0, alpha, 1.0);
    quad_properties(p0, p1, p2, p3, centroid, n, t1, t2, area);
    emit_cell("quad.unit", centroid, n, t1, t2, area);
    emitf("quad.unit.text centroid=%s t2=%s", centroid.toString().c_str(), t2.toString().c_str());

    Vector3 q0 = Vector3(0.0, 0.0, 0.0);
    Vector3 q1 = Vector3(1.0, 0.1, 0.0);
    Vector3 q2 = Vector3(1.2, 1.0, 0.4);
    Vector3 q3 = Vector3(-0.1, 0.9, 0.2);
    quad_properties(q0, q1, q2, q3, centroid, n, t1, t2, area);
    emit_cell("quad.skew3d", centroid, n, t1, t2, area);

    eil::Array<Vector3> qarr = { q0, q1, q2, q3 };
    quad_properties(qarr, centroid, n, t1, t2, area);
    emit_cell("quad.skew3d.arrayform", centroid, n, t1, t2, area);

    quad_properties(q0, q1, q1, q0, centroid, n, t1, t2, area);
    emit_cell("quad.degenerate", centroid, n, t1, t2, area);

    emitf("xyarea3.unit=%.17g", xyplane_area(p0, p1, p2));
    emitf("xyarea3.neg=%.17g", xyplane_area(p0, p2, p1));
    emitf("xyarea3.skew=%.17g", xyplane_area(q0, q1, q2));
    emitf("xyarea4.unit=%.17g", xyplane_area(p0, p1, p2, p3));
    emitf("xyarea4.skew=%.17g", xyplane_area(q0, q1, q2, q3));
    emitf("xyarea4.reversed=%.17g", xyplane_area(q3, q2, q1, q0));

    Vector3 c2;
    number a_xy = 0.0, iLen = 0.0, jLen = 0.0, minLen = 0.0;
    xyplane_quad_cell_properties(p0, p1, p2, p3, c2, a_xy, iLen, jLen, minLen);
    number sq = xyplane_quad_squareness(p0, p1, p2, p3);
    emitf("xyquadcell.unit cx=%.17g cy=%.17g cz=%.17g area=%.17g iLen=%.17g jLen=%.17g "
          "minLen=%.17g sq=%.17g", c2.x, c2.y, c2.z, a_xy, iLen, jLen, minLen, sq);
    xyplane_quad_cell_properties(q0, q1, q2, q3, c2, a_xy, iLen, jLen, minLen);
    number sq2 = xyplane_quad_squareness(q0, q1, q2, q3);
    emitf("xyquadcell.skew cx=%.17g cy=%.17g cz=%.17g area=%.17g iLen=%.17g jLen=%.17g "
          "minLen=%.17g sq=%.17g", c2.x, c2.y, c2.z, a_xy, iLen, jLen, minLen, sq2);
    Vector3 r0 = Vector3(0.0, 0.0);
    Vector3 r1 = Vector3(1.0, 0.0);
    Vector3 r2 = Vector3(1.0, 1.0e-4);
    Vector3 r3 = Vector3(0.0, 1.0e-4);
    xyplane_quad_cell_properties(r0, r1, r2, r3, c2, a_xy, iLen, jLen, minLen);
    number sq3 = xyplane_quad_squareness(r0, r1, r2, r3);
    emitf("xyquadcell.sliver cx=%.17g cy=%.17g cz=%.17g area=%.17g iLen=%.17g jLen=%.17g "
          "minLen=%.17g sq=%.17g", c2.x, c2.y, c2.z, a_xy, iLen, jLen, minLen, sq3);

    xyplane_triangle_cell_properties(p0, p1, p2, c2, a_xy, iLen, jLen, minLen);
    emitf("xytricell.unit cx=%.17g cy=%.17g cz=%.17g area=%.17g iLen=%.17g jLen=%.17g minLen=%.17g",
          c2.x, c2.y, c2.z, a_xy, iLen, jLen, minLen);
    xyplane_triangle_cell_properties(q0, q1, q2, c2, a_xy, iLen, jLen, minLen);
    emitf("xytricell.skew cx=%.17g cy=%.17g cz=%.17g area=%.17g iLen=%.17g jLen=%.17g minLen=%.17g",
          c2.x, c2.y, c2.z, a_xy, iLen, jLen, minLen);
    xyplane_triangle_cell_properties(q2, q1, q0, c2, a_xy, iLen, jLen, minLen);
    emitf("xytricell.negarea cx=%.17g cy=%.17g cz=%.17g area=%.17g iLen=%.17g jLen=%.17g minLen=%.17g",
          c2.x, c2.y, c2.z, a_xy, iLen, jLen, minLen);

    Vector3 t0 = Vector3(0, 0, 0);
    Vector3 t1v = Vector3(std::cos(radians(30)), std::sin(radians(30)), 0.0);
    Vector3 t2v = Vector3(0.0, 1.0, 0.0);
    double dx = 0.5 * std::tan(radians(30));
    double dL = std::cos(radians(30));
    double dz = eil::sqrt(dL * dL - dx * dx);
    Vector3 t3v = Vector3(dx, 0.5, dz);
    emitf("tet.consts dx=%.17g dL=%.17g dz=%.17g", dx, dL, dz);
    number volume = 0.0;
    eil::Array<Vector3> tet = { t0, t1v, t2v, t3v };
    Vector3 t21 = Vector3(t2v);
    Vector3 t12 = Vector3(t1v);
    emitf("tet.vol=%.17g", tetrahedron_volume(t0, t1v, t2v, t3v));
    emitf("tet.vol.neg=%.17g", tetrahedron_volume(t0, t21, t12, t3v));
    tetrahedron_properties(t0, t1v, t2v, t3v, centroid, volume);
    emitf("tet.prop cx=%.17g cy=%.17g cz=%.17g vol=%.17g", centroid.x, centroid.y, centroid.z,
          volume);
    tetrahedron_properties(tet, centroid, volume);
    emitf("tet.prop.arrayform cx=%.17g cy=%.17g cz=%.17g vol=%.17g", centroid.x, centroid.y,
          centroid.z, volume);
    number L_min = 0.0;
    tetrahedron_properties(t0, t1v, t2v, t3v, centroid, volume, L_min);
    emitf("tet.propLmin cx=%.17g cy=%.17g cz=%.17g vol=%.17g Lmin=%.17g", centroid.x, centroid.y,
          centroid.z, volume, L_min);
    tetrahedron_properties(tet, centroid, volume, L_min);
    emitf("tet.propLmin.arrayform cx=%.17g cy=%.17g cz=%.17g vol=%.17g Lmin=%.17g", centroid.x,
          centroid.y, centroid.z, volume, L_min);
    Vector3 o0 = Vector3(0.0, 0.0, 0.0);
    Vector3 rt1 = Vector3(1.0, 0.0, 0.0);
    Vector3 rt2 = Vector3(std::cos(radians(60)), std::sin(radians(60)), 0.0);
    Vector3 rt3 = Vector3(0.5, 0.5 * std::tan(radians(30)), eil::sqrt(6.0) / 3.0);
    tetrahedron_properties(rt1, rt2, rt3, o0, centroid, volume, L_min);
    emitf("tet.regular cx=%.17g cy=%.17g cz=%.17g vol=%.17g Lmin=%.17g", centroid.x, centroid.y,
          centroid.z, volume, L_min);
    Vector3 flat3 = Vector3(0.3, 0.4, 0.0);
    tetrahedron_properties(t0, t1v, t2v, flat3, centroid, volume, L_min);
    emitf("tet.flat cx=%.17g cy=%.17g cz=%.17g vol=%.17g Lmin=%.17g", centroid.x, centroid.y,
          centroid.z, volume, L_min);

    Vector3 pmB = Vector3(0.25 * (t0.x + t1v.x + t2v.x + t3v.x), 0.25 * (t0.y + t1v.y + t2v.y + t3v.y),
                          0.25 * (t0.z + t1v.z + t2v.z + t3v.z));
    Vector3 apex = Vector3(t3v);
    emitf("dipyramid.tet=%.17g", tetragonal_dipyramid_volume(t0, t1v, t2v, t3v, pmB, apex));
    Vector3 h0 = Vector3(0, 0, 0);
    Vector3 h1 = Vector3(1, 0, 0);
    Vector3 h2 = Vector3(1, 1, 0);
    Vector3 h3 = Vector3(0, 1, 0);
    Vector3 h4 = Vector3(0, 0, 1);
    Vector3 h5 = Vector3(1, 0, 1);
    Vector3 h6 = Vector3(1, 1, 1);
    Vector3 h7 = Vector3(0, 1, 1);
    Vector3 basem = Vector3(0.25, 0.25, 0.0);
    Vector3 apix = Vector3(h6);
    emitf("dipyramid.unitbase=%.17g", tetragonal_dipyramid_volume(h2, h3, h7, h6, basem, apix));

    Vector3 py0 = Vector3(h0);
    Vector3 py1 = Vector3(h1);
    Vector3 py2 = Vector3(h2);
    Vector3 py3 = Vector3(h3);
    Vector3 py4 = Vector3(0.5, 0.5, 1);
    pyramid_properties(py0, py1, py2, py3, py4, true, centroid, volume);
    emitf("pir.true cx=%.17g cy=%.17g cz=%.17g vol=%.17g", centroid.x, centroid.y, centroid.z, volume);
    pyramid_properties(py0, py1, py2, py3, py4, false, centroid, volume);
    emitf("pir.approx cx=%.17g cy=%.17g cz=%.17g vol=%.17g", centroid.x, centroid.y, centroid.z,
          volume);
    eil::Array<Vector3> pirarr = { py0, py1, py2, py3, py4 };
    pyramid_properties(pirarr, true, centroid, volume);
    emitf("pir.true.arrayform cx=%.17g cy=%.17g cz=%.17g vol=%.17g", centroid.x, centroid.y,
          centroid.z, volume);
    pyramid_properties(py0, py1, py2, py3, py4, true, centroid, volume, L_min);
    emitf("pir.trueLmin cx=%.17g cy=%.17g cz=%.17g vol=%.17g Lmin=%.17g", centroid.x, centroid.y,
          centroid.z, volume, L_min);
    pyramid_properties(pirarr, true, centroid, volume, L_min);
    emitf("pir.trueLmin.arrayform cx=%.17g cy=%.17g cz=%.17g vol=%.17g Lmin=%.17g", centroid.x,
          centroid.y, centroid.z, volume, L_min);
    Vector3 wpy4 = Vector3(0.3, 0.7, 1.4);
    pyramid_properties(py0, py1, py2, py3, wpy4, true, centroid, volume, L_min);
    emitf("pir.skew cx=%.17g cy=%.17g cz=%.17g vol=%.17g Lmin=%.17g", centroid.x, centroid.y,
          centroid.z, volume, L_min);
    emitf("pir.skew.text centroid=%s vol6=%s", centroid.toString().c_str(),
          eil::to_string(volume).c_str());

    Vector3 w0 = Vector3(0, 0, 0);
    Vector3 w1 = Vector3(std::cos(radians(30)), std::sin(radians(30)), 0.0);
    Vector3 w2 = Vector3(0.0, 1.0, 0.0);
    Vector3 incz = Vector3(0, 0, -1.0);
    Vector3 w3 = w0 + incz;
    Vector3 w4 = w1 + incz;
    Vector3 w5 = w2 + incz;
    wedge_properties(w0, w1, w2, w3, w4, w5, true, centroid, volume);
    emitf("wed.true cx=%.17g cy=%.17g cz=%.17g vol=%.17g", centroid.x, centroid.y, centroid.z, volume);
    wedge_properties(w0, w1, w2, w3, w4, w5, false, centroid, volume);
    emitf("wed.approx cx=%.17g cy=%.17g cz=%.17g vol=%.17g", centroid.x, centroid.y, centroid.z,
          volume);
    eil::Array<Vector3> wedarr = { w0, w1, w2, w3, w4, w5 };
    wedge_properties(wedarr, true, centroid, volume);
    emitf("wed.true.arrayform cx=%.17g cy=%.17g cz=%.17g vol=%.17g", centroid.x, centroid.y,
          centroid.z, volume);
    wedge_properties(w0, w1, w2, w3, w4, w5, true, centroid, volume, L_min);
    emitf("wed.trueLmin cx=%.17g cy=%.17g cz=%.17g vol=%.17g Lmin=%.17g", centroid.x, centroid.y,
          centroid.z, volume, L_min);
    wedge_properties(wedarr, true, centroid, volume, L_min);
    emitf("wed.trueLmin.arrayform cx=%.17g cy=%.17g cz=%.17g vol=%.17g Lmin=%.17g", centroid.x,
          centroid.y, centroid.z, volume, L_min);
    Vector3 w1s = Vector3(1.1, 0.2, 0.0);
    Vector3 w2s = Vector3(-0.2, 1.3, 0.1);
    Vector3 w3s = Vector3(0.05, 0.1, -0.9);
    Vector3 w4s = Vector3(1.05, 0.25, -1.1);
    Vector3 w5s = Vector3(-0.1, 1.2, -0.95);
    wedge_properties(w0, w1s, w2s, w3s, w4s, w5s, true, centroid, volume, L_min);
    emitf("wed.skew cx=%.17g cy=%.17g cz=%.17g vol=%.17g Lmin=%.17g", centroid.x, centroid.y,
          centroid.z, volume, L_min);

    number kLen = 0.0;
    hex_cell_properties(h0, h1, h2, h3, h4, h5, h6, h7, true, centroid, volume, iLen, jLen, kLen);
    emitf("hex.cube cx=%.17g cy=%.17g cz=%.17g vol=%.17g iLen=%.17g jLen=%.17g kLen=%.17g",
          centroid.x, centroid.y, centroid.z, volume, iLen, jLen, kLen);
    hex_cell_properties(h0, h1, h2, h3, h4, h5, h6, h7, false, centroid, volume, iLen, jLen, kLen);
    emitf("hex.cube.approx cx=%.17g cy=%.17g cz=%.17g vol=%.17g iLen=%.17g jLen=%.17g kLen=%.17g",
          centroid.x, centroid.y, centroid.z, volume, iLen, jLen, kLen);
    eil::Array<Vector3> hexarr = { h0, h1, h2, h3, h4, h5, h6, h7 };
    hex_cell_properties(hexarr, true, centroid, volume, iLen, jLen, kLen);
    emitf("hex.cube.arrayform cx=%.17g cy=%.17g cz=%.17g vol=%.17g iLen=%.17g jLen=%.17g kLen=%.17g",
          centroid.x, centroid.y, centroid.z, volume, iLen, jLen, kLen);
    Vector3 f0 = Vector3(0.0, 0.0, 0.0);
    Vector3 f1 = Vector3(1.0, 0.0, 0.05);
    Vector3 f2 = Vector3(1.1, 1.0, 0.0);
    Vector3 f3 = Vector3(0.0, 1.0, 0.04);
    Vector3 f4 = Vector3(0.0, 0.0, 1.0e-3);
    Vector3 f5 = Vector3(1.0, 0.05, 0.9e-3);
    Vector3 f6 = Vector3(1.1, 1.0, 1.1e-3);
    Vector3 f7 = Vector3(0.0, 1.0, 1.05e-3);
    hex_cell_properties(f0, f1, f2, f3, f4, f5, f6, f7, true, centroid, volume, iLen, jLen, kLen);
    emitf("hex.warped cx=%.17g cy=%.17g cz=%.17g vol=%.17g iLen=%.17g jLen=%.17g kLen=%.17g",
          centroid.x, centroid.y, centroid.z, volume, iLen, jLen, kLen);
    hex_cell_properties(h0, h1, h2, h3, h0, h1, h2, h3, true, centroid, volume, iLen, jLen, kLen);
    emitf("hex.collapse_exact0 cx=%.17g cy=%.17g cz=%.17g vol=%.17g iLen=%.17g jLen=%.17g kLen=%.17g",
          centroid.x, centroid.y, centroid.z, volume, iLen, jLen, kLen);
    const double heights[6] = { -1.0e-13, -1.0e-11, 1.0e-21, 1.0e-19, 1.0e-3, 0.0 };
    for (double h : heights) {
        Vector3 u4 = Vector3(0.0, 0.0, h);
        Vector3 u5 = Vector3(1.0, 0.0, h);
        Vector3 u6 = Vector3(1.0, 1.0, h);
        Vector3 u7 = Vector3(0.0, 1.0, h);
        hex_cell_properties(h0, h1, h2, h3, u4, u5, u6, u7, true, centroid, volume, iLen, jLen, kLen);
        emitf("hex.thin h=%.17g cx=%.17g cy=%.17g cz=%.17g vol=%.17g iLen=%.17g jLen=%.17g kLen=%.17g",
              h, centroid.x, centroid.y, centroid.z, volume, iLen, jLen, kLen);
    }
    emitf("hex.thresholds smallButSignificantVolume=%.17g verySmallVolume=%.17g",
          smallButSignificantVolume, verySmallVolume);
    emitf("nan.cbrt vol=%.17g pow3=%.17g fmin=%.17g fminRev=%.17g", -0.11785113019775791,
          eil::pow(-0.11785113019775791, 1.0 / 3.0),
          std::fmin(eil::pow(-0.11785113019775791, 1.0 / 3.0), 0.4082),
          std::fmin(0.4082, eil::pow(-0.11785113019775791, 1.0 / 3.0)));
    emitf("nan.sqrt sqrtNeg=%s tostr=%s", eil::to_string(eil::sqrt(-0.44)).c_str(),
          eil::to_string(eil::sqrt(-1.0e-20)).c_str());
    Vector3 b0 = Vector3(0.1, 0.0, 0.0);
    Vector3 b1 = Vector3(1.0, 0.1, 0.0);
    Vector3 b2 = Vector3(1.2, 1.0, 0.05);
    Vector3 b3 = Vector3(0.05, 0.9, -0.02);
    Vector3 b4 = Vector3(0.12, 0.02, 1.0);
    Vector3 b5 = Vector3(1.05, 0.11, 0.95);
    Vector3 b6 = Vector3(1.3, 1.1, 1.02);
    Vector3 b7 = Vector3(0.06, 0.95, 1.01);
    hex_cell_properties(b0, b1, b2, b3, b4, b5, b6, b7, true, centroid, volume, iLen, jLen, kLen);
    emitf("hex.skew cx=%.17g cy=%.17g cz=%.17g vol=%.17g iLen=%.17g jLen=%.17g kLen=%.17g",
          centroid.x, centroid.y, centroid.z, volume, iLen, jLen, kLen);
    emitf("hex.skew.text centroid=%s vol6=%s iLen6=%s", centroid.toString().c_str(),
          eil::to_string(volume).c_str(), eil::to_string(iLen).c_str());

    Vector3 d = Vector3(0.65, 0.0, 0.0);
    Vector3 ee = Vector3(0.65, -0.1, 0.0);
    Vector3 a = Vector3(1.0, 0.0, 0.0);
    Vector3 b = Vector3(1.0, 1.0, 0.0);
    Vector3 cc = Vector3(0.5, 0.0, 0.0);
    Vector3 da = Vector3(a);
    emitf("inside_tri.d=%d inside_tri.e=%d inside_tri.a=%d", inside_triangle(d, a, b, cc),
          inside_triangle(ee, a, b, cc), inside_triangle(da, a, b, cc));
    emitf("on_left.ab.d=%s on_left.ab.e=%s on_left.ba.d=%s",
          eil::to_string(on_left_of_xy_line(a, b, d)).c_str(),
          eil::to_string(on_left_of_xy_line(a, b, ee)).c_str(),
          eil::to_string(on_left_of_xy_line(b, a, d)).c_str());
    Vector3 v0 = Vector3(0.0, 0.0);
    Vector3 v1 = Vector3(1.0, -0.1);
    Vector3 v2 = Vector3(1.0, 1.0);
    Vector3 v3 = Vector3(0.0, 1.0);
    eil::Array<Vector3> poly = { v0, v1, v2, v3 };
    emitf("xy_polygon.d=%s xy_polygon.e=%s xy_triangle.d=%s xy_triangle.e=%s xy_quad.d=%s xy_quad.e=%s",
          eil::to_string(inside_xy_polygon(poly, d)).c_str(),
          eil::to_string(inside_xy_polygon(poly, ee)).c_str(),
          eil::to_string(inside_xy_triangle(v0, v1, v2, d)).c_str(),
          eil::to_string(inside_xy_triangle(v0, v1, v2, ee)).c_str(),
          eil::to_string(inside_xy_quad(v0, v1, v2, v3, d)).c_str(),
          eil::to_string(inside_xy_quad(v0, v1, v2, v3, ee)).c_str());
    Vector3 farp = Vector3(2.0, 2.0);
    Vector3 v0copy = Vector3(v0);
    emitf("winding.d=%d winding.e=%d winding.corner=%d winding.outside=%d",
          winding_number(poly, d), winding_number(poly, ee), winding_number(poly, v0copy),
          winding_number(poly, farp));
    eil::Array<Vector3> polyRev = { v3, v2, v1, v0 };
    emitf("winding.reversed.d=%d", winding_number(polyRev, d));

    Vector3 hc = Vector3(0.5, 0.5, 0.5);
    Vector3 hOut = Vector3(1.5, 0.5, 0.5);
    Vector3 h0copy = Vector3(h0);
    emitf("in_hex.center=%s in_hex.e=%s in_hex.corner=%s in_hex.outside=%s",
          eil::to_string(inside_hexahedron(h0, h1, h2, h3, h4, h5, h6, h7, hc)).c_str(),
          eil::to_string(inside_hexahedron(h0, h1, h2, h3, h4, h5, h6, h7, ee)).c_str(),
          eil::to_string(inside_hexahedron(h0, h1, h2, h3, h4, h5, h6, h7, h0copy)).c_str(),
          eil::to_string(inside_hexahedron(h0, h1, h2, h3, h4, h5, h6, h7, hOut)).c_str());
    Vector3 f1p = Vector3(0.1, 0.0, 0.5);
    Vector3 f2p = Vector3(0.0, 0.2, 0.5);
    Vector3 f3p = Vector3(0.0, -0.2, 0.5);
    Vector3 f4p = Vector3(1.0, 0.0, 1.0);
    Vector3 f5p = Vector3(0.0, 0.0, -0.5);
    emitf("in_tet.f1=%s f2=%s f3=%s f4=%s f5=%s",
          eil::to_string(inside_tetrahedron(h0, h1, h2, h4, f1p)).c_str(),
          eil::to_string(inside_tetrahedron(h0, h1, h2, h4, f2p)).c_str(),
          eil::to_string(inside_tetrahedron(h0, h1, h2, h4, f3p)).c_str(),
          eil::to_string(inside_tetrahedron(h0, h1, h2, h4, f4p)).c_str(),
          eil::to_string(inside_tetrahedron(h0, h1, h2, h4, f5p)).c_str());
    Vector3 pc1 = Vector3(0.4, 0.4, 0.4);
    Vector3 pc2 = Vector3(0.5, 0.5, 1.5);
    Vector3 py0copy = Vector3(py0);
    emitf("in_pyramid.center=%s corner=%s above=%s",
          eil::to_string(inside_pyramid(h0, h1, h2, h3, h4, pc1)).c_str(),
          eil::to_string(inside_pyramid(h0, h1, h2, h3, h4, py0copy)).c_str(),
          eil::to_string(inside_pyramid(h0, h1, h2, h3, h4, pc2)).c_str());
    Vector3 wc1 = Vector3(0.4, 0.3, 0.5);
    Vector3 wc2 = Vector3(0.4, 0.3, -0.5);
    emitf("in_wedge.center=%s below=%s corner=%s",
          eil::to_string(inside_wedge(h0, h1, h2, h4, h5, h6, wc1)).c_str(),
          eil::to_string(inside_wedge(h0, h1, h2, h4, h5, h6, wc2)).c_str(),
          eil::to_string(inside_wedge(h0, h1, h2, h4, h5, h6, h0copy)).c_str());
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <probes/geom_elements_properties.txt>\n";
        return 2;
    }

    port_d_unittest();
    replay_probe();

    std::ifstream in(argv[1]);
    if (!in) {
        std::cerr << "cannot open baseline " << argv[1] << "\n";
        return 2;
    }
    std::vector<std::string> expected;
    for (std::string line; std::getline(in, line);) expected.push_back(line);

    int line_diffs = 0;
    size_t quantities = 0, identical = 0;
    long long worst_ulp = 0;
    std::string worst_where;
    double worst_d = 0.0, worst_cpp = 0.0;
    for (size_t i = 0; i < expected.size() || i < produced.size(); ++i) {
        if (i >= expected.size() || i >= produced.size() || expected[i] != produced[i]) {
            if (line_diffs < 20) {
                std::cout << "line " << (i + 1) << "\n  D:   |"
                          << (i < expected.size() ? expected[i] : std::string("<none>"))
                          << "|\n  C++: |"
                          << (i < produced.size() ? produced[i] : std::string("<none>")) << "|\n";
            }
            ++line_diffs;
        }
        if (i < expected.size() && i < produced.size()) {
            const std::vector<double> a = numbers_in(expected[i]);
            const std::vector<double> b = numbers_in(produced[i]);
            for (size_t k = 0; k < a.size() && k < b.size(); ++k) {
                ++quantities;
                if (std::memcmp(&a[k], &b[k], 8) == 0) {
                    ++identical;
                } else {
                    const long long gap = ulp_gap(a[k], b[k]);
                    if (gap > worst_ulp) {
                        worst_ulp = gap;
                        worst_d = a[k];
                        worst_cpp = b[k];
                        worst_where = expected[i].substr(0, expected[i].find(' '));
                        const size_t eq = expected[i].find('=', expected[i].find(' ') + 1);
                        (void)eq;
                    }
                }
            }
        }
    }
    std::cout << (line_diffs == 0 && failures == 0 ? "PASS" : "FAIL") << ": properties, "
              << checks - failures << "/" << checks << " unittest assertions hold; probe replay "
              << expected.size() << " D lines, " << produced.size() << " C++ lines, " << line_diffs
              << " differ; " << identical << " of " << quantities
              << " numeric quantities bit-identical";
    if (worst_ulp > 0) {
        std::cout << ", worst residual " << worst_ulp << " ULP at " << worst_where << " (D "
                  << worst_d << " vs C++ " << worst_cpp << ")";
    }
    std::cout << "\n";
    return (line_diffs == 0 && failures == 0) ? 0 : 1;
}
