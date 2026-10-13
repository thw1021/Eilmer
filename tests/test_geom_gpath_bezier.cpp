// tests/test_geom_gpath_bezier.cpp
//
// Verification of geom/gpath/bezier.d (206 D lines) -> src/geom/gpath/bezier.{hpp,cpp}.
//
//  A. the `unittest` block of geom/gpath/bezier.d (D:139-206) ported assertion for
//     assertion, in source order, with the `version (complex_numbers)` block
//     (D:159-205) — which is where every `^^` in this module lives — carried under
//     EIL_COMPLEX_NUMBERS and not compiled (T-904). Its cubic Bernstein terms
//     (1-t)^^3, 3*t*(1-t)^^2, 3*(1-t)*t^^2, t^^3 are ported through eil::pow and are
//     never expanded into products (wave-02-brief section 5: x^^3 differs from
//     x*x*x by 1 ULP).
//
//  B. a line-for-line replay of probes/geom/gpath/probe_bezier.d against
//     baselines/e587b235/probes/geom_gpath_bezier.txt (486 D lines at %.17g):
//     the control polygon B before and after elevateDegree(), the de Casteljau
//     evaluation over the t sweep that includes both boundaries, and dpdt/d2pdt2 at
//     t = 0 and t = 1, which are the end points of the derivative polygons C and D
//     that set_deriv_control_points() builds (bezier.d:103-122) — the four values a
//     wrong derivative control point corrupts first.
//
// Run: ./test_geom_gpath_bezier ../baselines/e587b235/probes/geom_gpath_bezier.txt

#include <cmath>
#include <memory>
#include <string>

#include "eil/eil.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/arc.hpp"
#include "geom/gpath/bezier.hpp"
#include "geom/gpath/path.hpp"
#include "nm/number.hpp"

#include "gpath1_dump.hpp"

using eil::geom::Arc;
using eil::geom::Bezier;
using eil::geom::Path;
using eil::geom::PathPtr;
using eil::geom::Vector3;
using eil::number;

using namespace eil_test_gpath1;

namespace {

// Mirrors dump_points() / dump_deriv() in probes/geom/gpath/probe_bezier.d.
void dump_points(const char* tag, const eil::Array<Vector3>& B) {
    for (size_t i = 0; i < B.length(); ++i) {
        emitf("%s B[%d] x=%.17g y=%.17g z=%.17g", tag, static_cast<int>(i), B[i].x, B[i].y, B[i].z);
    }
    emitf("%s npts=%d", tag, static_cast<int>(B.length()));
}

void dump_deriv(const char* tag, const Bezier& bz) {
    for (double t : {0.0, 1.0}) {
        Vector3 d1 = bz.dpdt(t);
        Vector3 d2 = bz.d2pdt2(t);
        emitf("%s deriv t=%.17g d1 x=%.17g y=%.17g z=%.17g d2 x=%.17g y=%.17g z=%.17g", tag, t, d1.x,
              d1.y, d1.z, d2.x, d2.y, d2.z);
    }
}

// ============================================================================
// A. bezier.d's unittest, assertion for assertion, in D's order.
// ============================================================================
void port_d_unittest() {
    Vector3 a = Vector3(eil::Array<number>{2.0, 2.0, 0.0}.all());
    Vector3 b = Vector3(eil::Array<number>{1.0, 2.0, 1.0}.all());
    Vector3 c = Vector3(eil::Array<number>{1.0, 2.0, 0.0}.all());
    Arc abc = Arc(a, b, c);
    Vector3 d = abc(0.5);
    Bezier adb = Bezier(eil::Array<Vector3>{a, d, b});
    Vector3 e = adb(0.5);
    CHECK1(eil::geom::approxEqualVectors(e, Vector3(1.60355, 2, 0.603553)),
           "D:148 approxEqualVectors(adb(0.5), (1.60355, 2, 0.603553))");
    Bezier ab = Bezier(eil::Array<Vector3>{a, b});
    CHECK1(eil::geom::approxEqualVectors(ab.dpdt(0.5), Vector3(-1, 0, 1)), "D:150 ab.dpdt(0.5)");
    CHECK1(eil::geom::approxEqualVectors(ab.d2pdt2(0.5), Vector3(0)), "D:151 ab.d2pdt2(0.5)");
    Bezier acb = Bezier(eil::Array<Vector3>{a, c, b});
    CHECK1(eil::geom::approxEqualVectors(acb.dpdt(0.5), Vector3(-1, 0, 1)), "D:153 acb.dpdt(0.5)");
    CHECK1(eil::geom::approxEqualVectors(acb.d2pdt2(0.5), Vector3(2, 0, 2)), "D:154 acb.d2pdt2(0.5)");
    //
    PathPtr adb_copy_ptr = adb.dup();
    Bezier& adbCopy = *std::dynamic_pointer_cast<Bezier>(adb_copy_ptr);
    adbCopy.elevateDegree(4);
    CHECK1(eil::geom::approxEqualVectors(adb(0.5), adbCopy(0.5)),
           "D:158 elevation preserves the curve to approxEqualVectors tolerance");

#ifdef EIL_COMPLEX_NUMBERS
    // D:159-205 — complex step derivative against the analytic Bernstein
    // derivatives. Not compiled in the trunk build (backlog T-904).
    {
        auto cubic_bezier_analytic_derivative = [](double t, size_t pt) {
            double value;
            if (pt == 0) value = eil::pow(1.0 - t, 3);
            else if (pt == 1) value = 3.0 * t * eil::pow(1.0 - t, 2);
            else if (pt == 2) value = 3.0 * (1.0 - t) * eil::pow(t, 2);
            else value = eil::pow(t, 3); // assume pt = 3
            return value;
        };
        Vector3 P0 = Vector3(eil::Array<number>{2.0, 2.0, 0.0}.all());
        Vector3 P1 = Vector3(eil::Array<number>{1.0, 2.0, 1.0}.all());
        Vector3 P2 = Vector3(eil::Array<number>{1.0, 2.0, 0.0}.all());
        Vector3 P3 = Vector3(eil::Array<number>{2.0, 2.0, 1.0}.all());
        eil::Array<Vector3> P = {P0, P1, P2, P3};
        eil::Array<Vector3> Po = {P0, P1, P2, P3};
        const double h = 1.0e-20;
        number ih = eil::Complex<double>(0.0, h);
        const double t = 0.5;
        for (size_t idx = 0; idx < P.length(); ++idx) {
            const double dPt_dP_analytic = cubic_bezier_analytic_derivative(t, idx);
            P[idx].x = P[idx].x + ih;
            Bezier myNewBez = Bezier(eil::Array<Vector3>{P[0], P[1], P[2], P[3]});
            const double cx = eil::im(myNewBez(t).x) / ih.im;
            const double cy = eil::im(myNewBez(t).y) / ih.im;
            const double cz = eil::im(myNewBez(t).z) / ih.im;
            CHECK1(eil::isClose(dPt_dP_analytic, cx), "D:199 analytic vs complex x");
            CHECK1(eil::isClose(0.0, cy), "D:200 analytic vs complex y");
            CHECK1(eil::isClose(0.0, cz), "D:201 analytic vs complex z");
            P[idx].x = Po[idx].x; // restore point to original position
        }
    }
#endif
}

// ============================================================================
// B. the probe replay, in probe order.
// ============================================================================
void replay_probe() {
    Vector3 a = Vector3(eil::Array<number>{2.0, 2.0, 0.0}.all());
    Vector3 b = Vector3(eil::Array<number>{1.0, 2.0, 1.0}.all());
    Vector3 c = Vector3(eil::Array<number>{1.0, 2.0, 0.0}.all());
    Arc abc = Arc(a, b, c);
    Vector3 d = abc(0.5);
    Bezier adb = Bezier(eil::Array<Vector3>{a, d, b});
    dump_path("bezier.quad", adb);
    dump_points("bezier.quad", adb.B);
    dump_deriv("bezier.quad", adb);

    // Linear Bezier: C has one point, D is the single Vector3(0) of bezier.d:115.
    Bezier ab = Bezier(eil::Array<Vector3>{a, b});
    dump_path("bezier.linear", ab);
    dump_points("bezier.linear", ab.B);
    dump_deriv("bezier.linear", ab);
    Vector3 l1 = ab.dpdt(0.5);
    Vector3 l2 = ab.d2pdt2(0.5);
    emitf("bezier.linear dpdt(0.5) x=%.17g y=%.17g z=%.17g", l1.x, l1.y, l1.z);
    emitf("bezier.linear d2pdt2(0.5) x=%.17g y=%.17g z=%.17g", l2.x, l2.y, l2.z);

    // The quadratic through c (bezier.d:152-154).
    Bezier acb = Bezier(eil::Array<Vector3>{a, c, b});
    dump_path("bezier.acb", acb);
    dump_deriv("bezier.acb", acb);
    Vector3 s1 = acb.dpdt(0.5);
    Vector3 s2 = acb.d2pdt2(0.5);
    emitf("bezier.acb dpdt(0.5) x=%.17g y=%.17g z=%.17g", s1.x, s1.y, s1.z);
    emitf("bezier.acb d2pdt2(0.5) x=%.17g y=%.17g z=%.17g", s2.x, s2.y, s2.z);

    // Degree elevation, from the unittest (bezier.d:156-158), and further.
    Bezier adbCopy = *std::dynamic_pointer_cast<Bezier>(adb.dup());
    adbCopy.elevateDegree(4);
    dump_points("bezier.elev4", adbCopy.B);
    dump_deriv("bezier.elev4", adbCopy);
    dump_path("bezier.elev4", adbCopy);
    Vector3 m_adb = adb(0.5);
    Vector3 m_elev = adbCopy(0.5);
    emitf("bezier.elev4 mid cmp adb=%.17g %.17g %.17g elev=%.17g %.17g %.17g", m_adb.x, m_adb.y,
          m_adb.z, m_elev.x, m_elev.y, m_elev.z);
    Bezier big = Bezier(eil::Array<Vector3>{a, c, b});
    big.elevateDegree(7);
    dump_points("bezier.elev7", big.B);
    dump_deriv("bezier.elev7", big);
    dump_path("bezier.elev7", big);
    // Elevating to the current degree returns immediately (bezier.d:76-77).
    Bezier same = Bezier(eil::Array<Vector3>{a, c, b});
    same.elevateDegree(2);
    dump_points("bezier.elevsame", same.B);
    dump_deriv("bezier.elevsame", same);

    // A cubic, the shape the surface patches use.
    Vector3 P0 = Vector3(eil::Array<number>{2.0, 2.0, 0.0}.all());
    Vector3 P1 = Vector3(eil::Array<number>{1.0, 2.0, 1.0}.all());
    Vector3 P2 = Vector3(eil::Array<number>{1.0, 2.0, 0.0}.all());
    Vector3 P3 = Vector3(eil::Array<number>{2.0, 2.0, 1.0}.all());
    Bezier cub = Bezier(eil::Array<Vector3>{P0, P1, P2, P3});
    dump_path("bezier.cubic", cub);
    dump_points("bezier.cubic", cub.B);
    dump_deriv("bezier.cubic", cub);

    // Error paths (bezier.d:22-25, :73 and :108).
    dump_error("bezier.err.nopoints", []() { Bezier(eil::Array<Vector3>()); });
    dump_error("bezier.err.onepoint", [&a]() { Bezier(eil::Array<Vector3>{a}); });
    Bezier low = Bezier(eil::Array<Vector3>{a, c, b});
    dump_error("bezier.err.lowerdegree", [&low]() { low.elevateDegree(1); });
    emitf("bezier.str=%s", cub.toString().c_str());
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <probes/geom_gpath_bezier.txt>\n";
        return 2;
    }
    port_d_unittest();
    replay_probe();
    return report("bezier", argv[1], argc > 2);
}
