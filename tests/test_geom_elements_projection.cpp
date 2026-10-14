// tests/test_geom_elements_projection.cpp
//
// Two layers:
//
//  A. the `unittest` block of geom/elements/projection.d (D:186-309) ported
//     assertion-for-assertion, in source order. NOTE: that block does not compile in
//     the reference tree — ldc2 1.40.0 rejects it with
//     "projection.d(154): cannot use `new` in `@nogc` function
//      geom.elements.projection.barycentricCoords", with or without -i, because the
//     unittest calls the four-point barycentricCoords at D:295. The assertions are
//     still ported verbatim; they compile and pass here because C++ has no @nogc.
//     The `version(complex_numbers)` blocks (D:204-236, :244-268) are carried under
//     EIL_COMPLEX_NUMBERS and not compiled (backlog T-904).
//
//  B. a line-for-line replay of probes/geom/elements/probe_projection.d (built
//     against the one-token shadow copy of projection.d, see its header) against
//     baselines/e587b235/probes/geom_elements_projection.txt.
//
// The semantics this test pins, all measured rather than assumed:
//   * D's `out double t` (D:17) is initialised to double.init, which is NaN — so a
//     parallel pair leaves the caller's variable NaN, not untouched.
//   * the four-point barycentricCoords throws GeometryException (an *Exception*, so
//     catchable as ExceptionBase) for a non-convex quad, with the message
//     "Quadrilateral is not convex."; the three-point overload stays distinguishable
//     by arity.
//   * GeometryException's file/line name the throw site, not geometry_exception.d.
//
// Run: ./test_geom_elements_projection ../baselines/e587b235/probes/geom_elements_projection.txt

#include <array>
#include <cctype>
#include <cstdarg>
#include <cmath>
#include "eil/math.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "geom/geometry_exception.hpp"
#include "geom/elements/nomenclature.hpp"
#include "geom/elements/projection.hpp"
#include "geom/elements/properties.hpp"
#include "geom/elements/vector3.hpp"
#include "nm/number.hpp"

using eil::geom::Vector3;
using eil::number;
using eil::geom::intersect2D;
using eil::geom::barycentricCoords;
using eil::geom::project_onto_plane;

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

// ============================================================================
// A. the D unittest block (projection.d:186-309).
// ============================================================================
void port_d_unittest() {
    double t_intersection = 0.0;
    bool foundIntersection = intersect2D(Vector3(0.0, 1.0), Vector3(1.0, 1.0), Vector3(0.5, 0.5),
                                        Vector3(0.0, 1.0), t_intersection);
    CHECK(foundIntersection, "D:191 intersect2D found");
    CHECK(eil::isClose(t_intersection, 0.5), "D:192 intersect2D t = 0.5");

    // Projection onto a plane.
    Vector3 a = Vector3(1.0, 0.0, 0.0); // plane through a,b,c
    Vector3 b = Vector3(1.0, 1.0, 0.0);
    Vector3 c = Vector3(0.5, 0.0, 0.0);
    Vector3 qr = Vector3(3.0, 3.0, -3.0); // direction
    Vector3 q = Vector3(0.0, 0.0, 1.0); // start point
    int flag = project_onto_plane(q, qr, a, b, c);
    (void)flag;
    CHECK(eil::geom::approxEqualVectors(q, Vector3(1.0, 1.0, 0.0)), "D:201 projected point");

#ifdef EIL_COMPLEX_NUMBERS
    // D:204-236 — projection onto a plane, complex step derivative test.
    {
        Vector3 q0 = Vector3(q);
        q = Vector3(0.0, 0.0, 1.0);
        number hIm = eil::Complex<double>(0.0, 1.0e-20);
        qr.z += hIm;
        flag = project_onto_plane(q, qr, a, b, c);
        std::array<double, 3> qDerivCmplx;
        qDerivCmplx[0] = im(q.x) / im(hIm);
        qDerivCmplx[1] = im(q.y) / im(hIm);
        qDerivCmplx[2] = im(q.z) / im(hIm);
        qr = Vector3(3.0, 3.0, -3.0);
        q = Vector3(0.0, 0.0, 1.0);
        double hRe = 1.0e-06;
        qr.z += hRe;
        flag = project_onto_plane(q, qr, a, b, c);
        std::array<double, 3> qDerivReal;
        qDerivReal[0] = (re(q.x) - re(q0.x)) / hRe;
        qDerivReal[1] = (re(q.y) - re(q0.y)) / hRe;
        qDerivReal[2] = (re(q.z) - re(q0.z)) / hRe;
        for (size_t idx = 0; idx < 3; ++idx) {
            CHECK(eil::isClose(qDerivCmplx[idx], qDerivReal[idx], 1.0e-5, 1.0e-9),
                  "D:234 complex vs real step");
        }
    }
#endif

    // projection onto a cylinder.
    Vector3 myp = Vector3(1.0, 1.0, 1.0);
    eil::geom::map_neutral_plane_to_cylinder(myp, number(1.0));
    CHECK(eil::geom::approxEqualVectors(myp, Vector3(1.0, eil::sin(1.0), eil::cos(1.0))),
          "D:241 cylinder mapping");

#ifdef EIL_COMPLEX_NUMBERS
    // D:244-268 — projection onto a cylinder, complex step derivative test.
    {
        Vector3 myp0 = Vector3(myp);
        myp = Vector3(1.0, 1.0, 1.0);
        myp.z += hIm;
        map_neutral_plane_to_cylinder(myp, number(1.0));
        qDerivCmplx[0] = im(myp.x) / im(hIm);
        qDerivCmplx[1] = im(myp.y) / im(hIm);
        qDerivCmplx[2] = im(myp.z) / im(hIm);
        myp = Vector3(1.0, 1.0, 1.0);
        myp.z += hRe;
        map_neutral_plane_to_cylinder(myp, number(1.0));
        qDerivReal[0] = (re(myp.x) - re(myp0.x)) / hRe;
        qDerivReal[1] = (re(myp.y) - re(myp0.y)) / hRe;
        qDerivReal[2] = (re(myp.z) - re(myp0.z)) / hRe;
        for (size_t idx = 0; idx < 3; ++idx) {
            CHECK(eil::isClose(qDerivCmplx[idx], qDerivReal[idx]), "D:267 cylinder step");
        }
    }
#endif

    // Barycentric coordinates for the midpoint of an equilateral triangle.
    Vector3 p0 = Vector3(0.0, 0.0);
    Vector3 p1 = Vector3(1.0, 0.0);
    Vector3 p2 = Vector3(0.5, eil::sin(60.0 * static_cast<double>(EIL_PI) / 180.0));
    Vector3 p = (p0 + p1 + p2) / 3.0;
    std::array<number, 3> bcc = barycentricCoords(p, p0, p1, p2);
    CHECK(eil::isClose(eil::re(bcc[0]), 1.0 / 3.0), "D:276 bcc[0]");
    CHECK(eil::isClose(eil::re(bcc[1]), 1.0 / 3.0), "D:277 bcc[1]");
    CHECK(eil::isClose(eil::re(bcc[2]), 1.0 / 3.0), "D:278 bcc[2]");
    CHECK(!eil::geom::is_outside_triangle(bcc), "D:279 inside triangle");
    // Now move the point down to the bottom edge of the triangle.
    p = Vector3(0.5, 0.0);
    bcc = barycentricCoords(p, p0, p1, p2);
    CHECK(!eil::geom::is_outside_triangle(bcc), "D:283 on the bottom edge");
    // Finally, move the point just outside the triangle.
    p = Vector3(0.5, -0.01);
    bcc = barycentricCoords(p, p0, p1, p2);
    CHECK(eil::geom::is_outside_triangle(bcc), "D:287 outside the triangle");

    // Barycentric coordinates for the midpoint of a square.
    p0 = Vector3(0.0, 0.0);
    p1 = Vector3(1.0, 0.0);
    p2 = Vector3(1.0, 1.0);
    Vector3 p3 = Vector3(0.0, 1.0);
    p = (p0 + p1 + p2 + p3) / 4.0;
    std::array<number, 4> bcc4 = barycentricCoords(p, p0, p1, p2, p3);
    CHECK(eil::isClose(eil::re(bcc4[0]), 0.25), "D:296 bcc4[0]");
    CHECK(eil::isClose(eil::re(bcc4[1]), 0.25), "D:297 bcc4[1]");
    CHECK(eil::isClose(eil::re(bcc4[2]), 0.25), "D:298 bcc4[2]");
    CHECK(eil::isClose(eil::re(bcc4[3]), 0.25), "D:299 bcc4[3]");
    CHECK(!eil::geom::is_outside_quad(bcc4), "D:300 inside quad");
    // Now move the point down to the bottom edge of the square.
    p = Vector3(0.5, 0.0);
    bcc4 = barycentricCoords(p, p0, p1, p2, p3);
    CHECK(!eil::geom::is_outside_quad(bcc4), "D:304 on the bottom edge of the quad");
    // Finally, move the point just outside the square.
    p = Vector3(0.5, -0.01);
    bcc4 = barycentricCoords(p, p0, p1, p2, p3);
    CHECK(eil::geom::is_outside_quad(bcc4), "D:308 outside the quad");
}

// ============================================================================
// B. the probe replay.
// ============================================================================
void replay_probe() {
    using namespace eil::geom;

    emitf("== intersect2D, the unittest case ==");
    Vector3 i0 = Vector3(0.0, 1.0);
    Vector3 i1 = Vector3(1.0, 1.0);
    Vector3 ps = Vector3(0.5, 0.5);
    Vector3 dir = Vector3(0.0, 1.0);
    double t = 7.0;
    bool found = intersect2D(i0, i1, ps, dir, t);
    emitf("cross.found=%s t17=%.17g t6=%s", eil::to_string(found).c_str(), t,
          eil::to_string(t).c_str());

    emitf("== intersect2D, parallel: is t reset, and to what? ==");
    Vector3 a0 = Vector3(0.0, 1.0);
    Vector3 a1 = Vector3(1.0, 1.0);
    Vector3 ap = Vector3(0.5, 0.0);
    Vector3 ad = Vector3(1.0, 0.0);
    double tpar = 7.0;
    bool fpar = intersect2D(a0, a1, ap, ad, tpar);
    emitf("parallel.found=%s t17=%.17g isNaN=%s", eil::to_string(fpar).c_str(), tpar,
          eil::to_string(std::isnan(tpar)).c_str());
    double tpar2 = 0.0;
    intersect2D(a0, a1, ap, ad, tpar2);
    emitf("parallel.zeroed t17=%.17g isNaN=%s", tpar2, eil::to_string(std::isnan(tpar2)).c_str());

    emitf("== intersect2D, more crossings and near-parallel ==");
    Vector3 s0 = Vector3(0.0, 0.0);
    Vector3 s1 = Vector3(2.0, 2.0);
    for (int k = 0; k < 5; ++k) {
        double x = 0.25 + 0.5 * static_cast<double>(k);
        Vector3 q = Vector3(x, -1.0);
        Vector3 dv = Vector3(0.0, 1.0);
        double tk = -1.0;
        bool fk = intersect2D(s0, s1, q, dv, tk);
        emitf("cross2 k=%d q=%.17g found=%s t=%.17g", k, x, eil::to_string(fk).c_str(), tk);
    }
    Vector3 tiny1 = Vector3(1.0e-16, 1.0e-16);
    double tp = -1.0;
    bool tf = intersect2D(s0, tiny1, ps, dir, tp);
    emitf("tol.tinyseg found=%s t=%.17g", eil::to_string(tf).c_str(), tp);

    emitf("== project_onto_plane, all three return codes ==");
    Vector3 pa = Vector3(1.0, 0.0, 0.0);
    Vector3 pb = Vector3(1.0, 1.0, 0.0);
    Vector3 pc = Vector3(0.5, 0.0, 0.0);
    Vector3 qr = Vector3(3.0, 3.0, -3.0);
    Vector3 q = Vector3(0.0, 0.0, 1.0);
    int flag = project_onto_plane(q, qr, pa, pb, pc);
    emitf("plane.flag=%d qx=%.17g qy=%.17g qz=%.17g qs=%s", flag, q.x, q.y, q.z,
          q.toString().c_str());
    Vector3 qr1 = Vector3(1.0, 0.0, 0.0);
    Vector3 q1 = Vector3(0.75, 0.0, 0.0);
    int flag1 = project_onto_plane(q1, qr1, pa, pb, pc);
    emitf("plane.flag1=%d q1=%.17g %.17g %.17g", flag1, q1.x, q1.y, q1.z);
    Vector3 q2 = Vector3(0.75, 0.0, 3.0);
    int flag2 = project_onto_plane(q2, qr1, pa, pb, pc);
    emitf("plane.flag2=%d q2=%.17g %.17g %.17g", flag2, q2.x, q2.y, q2.z);
    Vector3 qr3 = Vector3(1.0, 0.0, 1.0e-13);
    Vector3 q3 = Vector3(0.0, 0.0, 1.0);
    int flag3 = project_onto_plane(q3, qr3, pa, pb, pc);
    emitf("plane.flag3=%d q3=%.17g %.17g %.17g", flag3, q3.x, q3.y, q3.z);

    emitf("== map_neutral_plane_to_cylinder ==");
    const double hs[4] = { 1.0, 2.0, 0.0, -1.0 };
    for (double h : hs) {
        Vector3 mp = Vector3(1.0, 1.0, 1.0);
        map_neutral_plane_to_cylinder(mp, number(h));
        emitf("cyl h=%.17g x=%.17g y=%.17g z=%.17g s=%s", h, mp.x, mp.y, mp.z,
              mp.toString().c_str());
    }
    Vector3 mp9 = Vector3(0.5, 1.5, -0.25);
    map_neutral_plane_to_cylinder(mp9, number(0.75));
    emitf("cyl.off x=%.17g y=%.17g z=%.17g", mp9.x, mp9.y, mp9.z);
    emitf("cyl.ref sin1=%.17g cos1=%.17g", eil::sin(1.0), eil::cos(1.0));

    emitf("== barycentricCoords, triangle (3-point) ==");
    Vector3 b0 = Vector3(0.0, 0.0);
    Vector3 b1 = Vector3(1.0, 0.0);
    Vector3 b2 = Vector3(0.5, eil::sin(60.0 * static_cast<double>(EIL_PI) / 180.0));
    Vector3 bm = (b0 + b1 + b2) / 3.0;
    std::array<number, 3> bc = barycentricCoords(bm, b0, b1, b2);
    emitf("bary3.mid l0=%.17g l1=%.17g l2=%.17g sum=%.17g outside=%s", bc[0], bc[1], bc[2],
          bc[0] + bc[1] + bc[2], eil::to_string(is_outside_triangle(bc)).c_str());
    Vector3 be = Vector3(0.5, 0.0);
    std::array<number, 3> bc2 = barycentricCoords(be, b0, b1, b2);
    emitf("bary3.edge l0=%.17g l1=%.17g l2=%.17g sum=%.17g outside=%s outside_tol=%.17g", bc2[0],
          bc2[1], bc2[2], bc2[0] + bc2[1] + bc2[2],
          eil::to_string(is_outside_triangle(bc2)).c_str(), 1.0e-10);
    Vector3 bo = Vector3(0.5, -0.01);
    std::array<number, 3> bc3 = barycentricCoords(bo, b0, b1, b2);
    emitf("bary3.outside l0=%.17g l1=%.17g l2=%.17g outside=%s", bc3[0], bc3[1], bc3[2],
          eil::to_string(is_outside_triangle(bc3)).c_str());
    emitf("bary3.tol outside_1e-3=%s outside_0=%s",
          eil::to_string(is_outside_triangle(bc3, 1.0e-3)).c_str(),
          eil::to_string(is_outside_triangle(bc3, 0.0)).c_str());
    emitf("bary3.denomzero l0=%s l1=%s l2=%s", eil::to_string(bc[0]).c_str(),
          eil::to_string(bc2[1]).c_str(), eil::to_string(bc3[2]).c_str());
    Vector3 dd = Vector3(1.0, 1.0);
    std::array<number, 3> bc4 = barycentricCoords(bm, b0, b1, dd);
    emitf("bary3.degenerate l0=%.17g l1=%.17g l2=%.17g outside=%s", bc4[0], bc4[1], bc4[2],
          eil::to_string(is_outside_triangle(bc4)).c_str());

    emitf("== barycentricCoords, quad (4-point, Floater mean-value) ==");
    Vector3 c0 = Vector3(0.0, 0.0);
    Vector3 c1 = Vector3(1.0, 0.0);
    Vector3 c2 = Vector3(1.0, 1.0);
    Vector3 c3 = Vector3(0.0, 1.0);
    Vector3 cq = (c0 + c1 + c2 + c3) / 4.0;
    std::array<number, 4> bq = barycentricCoords(cq, c0, c1, c2, c3);
    emitf("bary4.mid l0=%.17g l1=%.17g l2=%.17g l3=%.17g sum=%.17g outside=%s", bq[0], bq[1], bq[2],
          bq[3], bq[0] + bq[1] + bq[2] + bq[3],
          eil::to_string(is_outside_quad(bq)).c_str());
    Vector3 cq2 = Vector3(0.5, 0.0);
    std::array<number, 4> bq2 = barycentricCoords(cq2, c0, c1, c2, c3);
    emitf("bary4.edge l0=%.17g l1=%.17g l2=%.17g l3=%.17g outside=%s", bq2[0], bq2[1], bq2[2],
          bq2[3], eil::to_string(is_outside_quad(bq2)).c_str());
    Vector3 cq3 = Vector3(0.5, -0.01);
    std::array<number, 4> bq3 = barycentricCoords(cq3, c0, c1, c2, c3);
    emitf("bary4.outside l0=%.17g l1=%.17g l2=%.17g l3=%.17g outside=%s", bq3[0], bq3[1], bq3[2],
          bq3[3], eil::to_string(is_outside_quad(bq3)).c_str());
    emitf("bary4.tol outside_1e-3=%s outside_0=%s",
          eil::to_string(is_outside_quad(bq3, 1.0e-3)).c_str(),
          eil::to_string(is_outside_quad(bq3, 0.0)).c_str());
    Vector3 cq4 = Vector3(0.3, 0.7);
    Vector3 d1 = Vector3(1.2, -0.1);
    Vector3 d2 = Vector3(0.9, 1.1);
    Vector3 d3 = Vector3(-0.2, 0.8);
    std::array<number, 4> bq4 = barycentricCoords(cq4, c0, d1, d2, d3);
    emitf("bary4.skew l0=%.17g l1=%.17g l2=%.17g l3=%.17g sum=%.17g outside=%s", bq4[0], bq4[1],
          bq4[2], bq4[3], bq4[0] + bq4[1] + bq4[2] + bq4[3],
          eil::to_string(is_outside_quad(bq4)).c_str());

    Vector3 nv0 = Vector3(0.0, 0.0);
    Vector3 nv1 = Vector3(1.0, 0.0);
    Vector3 nv2 = Vector3(0.5, 0.2); // pushed inside -> non-convex
    Vector3 nv3 = Vector3(0.0, 1.0);
    Vector3 np = Vector3(0.2, 0.5);
    std::string msg;
    std::string cls = "none";
    bool caught = false;
    try {
        std::array<number, 4> junk = barycentricCoords(np, nv0, nv1, nv2, nv3);
        emitf("bary4.concave unexpectedly_ok l0=%.17g", junk[0]);
    } catch (const GeometryException& e) {
        caught = true;
        msg = e.msg();
        cls = "geom.geometry_exception.GeometryException";
        emitf("bary4.concave geom hasfile=%s hasline=%s class=%s msg=%s",
              eil::to_string(std::strlen(e.file()) > 0).c_str(),
              eil::to_string(e.line() > 0).c_str(), cls.c_str(), msg.c_str());
    } catch (const eil::ExceptionBase& e) {
        cls = "eil::ExceptionBase";
        msg = e.msg();
        emitf("bary4.concave wrong_catch %s", cls.c_str());
    }
    emitf("bary4.concave caught=%s class=%s msg=%s", eil::to_string(caught).c_str(), cls.c_str(),
          msg.c_str());
    emitf("bary4.concave siteshape throwInLibrary=true");

    emitf("== GeometryException file/line default at the call site ==");
    std::string m2;
    std::string f2s;
    size_t l2s = 0;
    bool asException = false;
    try {
        EIL_THROW_GEOM("explicit throw site"); // NOLINT: the macro is the point
    } catch (const eil::ExceptionBase& e) {
        asException = true;
        m2 = e.msg();
        f2s = e.file();
        l2s = e.line();
    }
    // D compares e.file to the path the compiler was handed for this module; the
    // ported equivalent is that the recorded file is this translation unit's own.
    emitf("geomthrow asException=%s msg=%s hasfile=%s hasline=%s sitename=%s",
          eil::to_string(asException).c_str(), m2.c_str(),
          eil::to_string(!f2s.empty()).c_str(), eil::to_string(l2s > 0).c_str(),
          eil::to_string(f2s == __FILE__ ? true : false).c_str());
    {
        // D's question was "is a GeometryException an Exception?"; in C++ the same
        // fact is that the thrown object dynamic_casts back to GeometryException when
        // caught as the ExceptionBase it derives from.
        bool derived = false;
        try {
            EIL_THROW_GEOM("y");
        } catch (const eil::ExceptionBase& e) {
            derived = (dynamic_cast<const GeometryException*>(&e) != nullptr);
        }
        emitf("geomthrow derivedFromException=%s", eil::to_string(derived).c_str());
        CHECK(derived, "GeometryException is catchable as ExceptionBase");
    }

    emitf("== Error vs Exception reachability ==");
    bool eInExceptionCatch = false;
    bool eInErrorCatch = false;
    std::string emsg;
    try {
        face_index("up");
    } catch (const eil::ExceptionBase& e) {
        eInExceptionCatch = true;
        emsg = e.msg();
    } catch (const eil::ErrorBase& e) {
        eInErrorCatch = true;
        emsg = e.msg();
    }
    emitf("face_index.caughtAsException=%s caughtAsError=%s msg=%s",
          eil::to_string(eInExceptionCatch).c_str(), eil::to_string(eInErrorCatch).c_str(),
          emsg.c_str());

    emitf("== Quaternion renders as a static array ==");
    Quaternion qq = { { 0.0, 1.0, 2.0, 3.0 } };
    emitf("quat=%s len=%u", eil::to_string(qq).c_str(), static_cast<unsigned>(qq.size()));

    // ---- separate assertions on the throw-site contract (not byte-compared) ----
    {
        Vector3 s1a = Vector3(0.0, 0.0);
        Vector3 s2a = Vector3(1.0, 0.0);
        Vector3 s3a = Vector3(0.5, 0.2);
        Vector3 s4a = Vector3(0.0, 1.0);
        Vector3 sp = Vector3(0.2, 0.5);
        // Copied out of the exception object, which is destroyed when its catch
        // clause ends — a bare const char* would dangle.
        std::string file_at_throw;
        size_t line_at_throw = 0;
        try {
            barycentricCoords(sp, s1a, s4a, s3a, s2a); // deliberately non-convex order
            CHECK(false, "non-convex quad must throw");
        } catch (const GeometryException& e) {
            file_at_throw = e.file();
            line_at_throw = e.line();
        }
        CHECK(!file_at_throw.empty(), "GeometryException carries a file");
        CHECK(line_at_throw > 0, "GeometryException carries a line");
        CHECK(file_at_throw.find("projection.cpp") != std::string::npos,
              "GeometryException file names the throw site (projection.cpp), "
              "not geometry_exception.hpp");
        CHECK(file_at_throw.find("geometry_exception.hpp") == std::string::npos,
              "GeometryException file is not the header that declares the class");
    }
    {
        // An Error is not an Exception: the GeometryException catch clause below is
        // first, so a face_index Error must fall through to the ErrorBase clause.
        bool reached_error_clause = false;
        try {
            face_index("nope");
        } catch (const eil::geom::GeometryException&) {
            CHECK(false, "GeometryException must not catch an ErrorBase");
        } catch (const eil::ExceptionBase&) {
            CHECK(false, "ExceptionBase must not catch an ErrorBase");
        } catch (const eil::ErrorBase&) {
            reached_error_clause = true;
        }
        CHECK(reached_error_clause, "face_index Error only matches ErrorBase");
    }
    {
        // D's `out double t` initialises to double.init (NaN).
        double keep = 3.25;
        Vector3 x0 = Vector3(0.0, 0.0);
        Vector3 x1 = Vector3(1.0, 0.0);
        Vector3 xp = Vector3(0.0, 1.0);
        Vector3 xd = Vector3(1.0, 0.0);
        bool ok = intersect2D(x0, x1, xp, xd, keep);
        CHECK(!ok, "parallel intersect2D returns false");
        CHECK(std::isnan(keep), "out parameter t becomes NaN, not the caller's 3.25");
    }
    {
        // the two barycentricCoords overloads stay distinguishable by arity
        Vector3 t0 = Vector3(0.0, 0.0);
        Vector3 t1 = Vector3(1.0, 0.0);
        Vector3 t2 = Vector3(0.0, 1.0);
        Vector3 pt = (t0 + t1 + t2) / 3.0;
        std::array<number, 3> three = barycentricCoords(pt, t0, t1, t2);
        Vector3 t3 = t1 + t2; // (1,1), so t0,t1,t3,t2 is a convex unit square
        std::array<number, 4> four = barycentricCoords(pt, t0, t1, t3, t2);
        CHECK(three.size() == 3 && four.size() == 4, "3- and 4-point overloads resolve");
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <probes/geom_elements_projection.txt>\n";
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
                    }
                }
            }
        }
    }
    std::cout << (line_diffs == 0 && failures == 0 ? "PASS" : "FAIL") << ": projection, "
              << checks - failures << "/" << checks << " assertions hold; probe replay "
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
