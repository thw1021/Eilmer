// tests/test_geom_gpath_line.cpp
//
// Verification of geom/gpath/line.d (106 D lines) -> src/geom/gpath/line.{hpp,cpp}.
//
//  A. the `unittest` block of geom/gpath/line.d (D:64-106) ported assertion for
//     assertion, in source order, including the `version (complex_numbers)` block
//     (D:85-105) which is carried under EIL_COMPLEX_NUMBERS and not compiled in the
//     trunk build (backlog T-904).
//
//  B. a line-for-line replay of probes/geom/gpath/probe_line.d against
//     baselines/e587b235/probes/geom_gpath_line.txt (376 D lines at %.17g).
//     Line replaces four of Path's virtuals with exact expressions
//     (dpdt, d2pdt2, partial_length, point_from_length), so the probe prints the
//     override and the qualified base version side by side: a port that dropped an
//     `override` would silently fall back to the finite differences and still print
//     plausible numbers within 1e-3.
//
// Run: ./test_geom_gpath_line ../baselines/e587b235/probes/geom_gpath_line.txt

#include <cmath>
#include <memory>
#include <string>

#include "eil/eil.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/line.hpp"
#include "geom/gpath/path.hpp"
#include "nm/number.hpp"

#include "gpath1_dump.hpp"

using eil::geom::Path;
using eil::geom::PathPtr;
using eil::geom::Line;
using eil::geom::Vector3;
using eil::number;

using namespace eil_test_gpath1;

namespace {

// ============================================================================
// A. line.d's unittest, assertion for assertion, in D's order.
// ============================================================================
void port_d_unittest() {
    // Some simple evaluations.
    Vector3 a = Vector3(eil::Array<number>{1.0, 2.2, 3.0}.all());
    Vector3 b = Vector3(1.0);
    Line ab = Line(a, b);
    Vector3 c = ab(0.5);
    CHECK1(eil::geom::approxEqualVectors(c, Vector3(1.0, 1.1, 1.5)), "D:70 approxEqualVectors(c, (1,1.1,1.5))");
    PathPtr ab2 = ab.dup();
    Vector3 d = (*ab2)(0.5);
    CHECK1(eil::geom::approxEqualVectors(c, d), "D:73 dup() reproduces the curve");

    // Set up an intersection that should be found on Line.
    Line pth = Line(Vector3(0.0, 1.0), Vector3(1.0, 1.0));
    Vector3 ps = Vector3(0.5, 0.5);
    Vector3 dir = Vector3(0.0, 1.0);
    double t = eil::d_init<double>();
    bool found = pth.intersect2D(ps, dir, t);
    CHECK1(found, "D:81 intersect2D found");
    // intersect2D parametric location on Line
    CHECK1(eil::isClose(t, 0.5), "D:83 isClose(t, 0.5)");

#ifdef EIL_COMPLEX_NUMBERS
    // D:85-105 — complex derivative evaluation. Not compiled in the trunk build
    // (backlog T-904); kept verbatim so the wave-12 port can use it.
    {
        Vector3 p0 = Vector3(0.0, 0.0);
        double alpha = 1.0;
        Vector3 p1 = Vector3(alpha, alpha);
        Line line0 = Line(p0, p1);
        double h = 1.0e-20;
        number ih = eil::Complex<double>(0.0, h);
        Vector3 p1_dash = Vector3(alpha + ih, alpha + ih);
        Line line1 = Line(p0, p1_dash);
        // What we want to compute is the sensitivity
        // of the midpoint with respect to alpha.
        double dpmid_da_x = eil::im(line1(0.5).x) / h;
        double dpmid_da_y = eil::im(line1(0.5).y) / h;
        double dpmid_da_z = eil::im(line1(0.5).z) / h;
        CHECK1(eil::isClose(dpmid_da_x, 0.5), "D:102 complex dpmid/da x");
        CHECK1(eil::isClose(dpmid_da_y, 0.5), "D:103 complex dpmid/da y");
        CHECK1(eil::isClose(dpmid_da_z, 0.0), "D:104 complex dpmid/da z");
        (void)line0;
    }
#endif
}

// ============================================================================
// B. the probe replay, in probe order.
// ============================================================================
void replay_probe() {
    // The line from line.d's unittest (line.d:66-73).
    Vector3 a = Vector3(eil::Array<number>{1.0, 2.2, 3.0}.all());
    Vector3 b = Vector3(1.0);
    Line ab = Line(a, b);
    dump_path("line.ab", ab);
    dump_path("line.dup", *ab.dup());

    // The horizontal line of the intersection test (line.d:76-83).
    Line pth = Line(Vector3(0.0, 1.0), Vector3(1.0, 1.0));
    dump_path("line.horiz", pth);

    // Exact vs finite-difference.
    for (double t : {0.0, 0.5, 1.0}) {
        Vector3 e = ab.dpdt(t);
        Vector3 f = ab.Path::dpdt(t); // the base implementation, via qualification
        emitf("line.cmp t=%.17g exact x=%.17g y=%.17g z=%.17g fd x=%.17g y=%.17g z=%.17g", t, e.x,
              e.y, e.z, f.x, f.y, f.z);
        Vector3 e2 = ab.d2pdt2(t);
        Vector3 f2 = ab.Path::d2pdt2(t);
        emitf("line.cmp2 t=%.17g exact x=%.17g y=%.17g z=%.17g fd x=%.17g y=%.17g z=%.17g", t, e2.x,
              e2.y, e2.z, f2.x, f2.y, f2.z);
    }

    // partial_length: exact |p1-p0|*|tb-ta| vs the 100-chord sum.
    emitf("line.pl exact=%.17g base=%.17g ta=%.17g tb=%.17g", ab.partial_length(0.2, 0.7),
          ab.Path::partial_length(0.2, 0.7), 0.2, 0.7);
    emitf("line.pl.swap exact=%.17g base=%.17g", ab.partial_length(0.7, 0.2),
          ab.Path::partial_length(0.7, 0.2));
    emitf("line.length exact=%.17g base=%.17g", ab.length(), ab.Path::length());

    // point_from_length, including a target beyond the end.
    Vector3 dab = b - a;
    double Lab = eil::geom::abs(dab);
    for (double L : {0.0, 0.5 * Lab, Lab, 2.0 * Lab}) {
        double t = eil::d_init<double>();
        Vector3 p = ab.point_from_length(L, t);
        emitf("line.pfl target=%.17g t=%.17g x=%.17g y=%.17g z=%.17g", L, t, p.x, p.y, p.z);
    }

    // A degenerate line (p0 == p1): point_from_length divides by |dp| = 0.
    Line dd = Line(Vector3(1.0, 2.0, 3.0), Vector3(1.0, 2.0, 3.0));
    double tz = eil::d_init<double>();
    Vector3 pz = dd.point_from_length(0.0, tz);
    emitf("line.degen pfl t=%.17g x=%.17g y=%.17g z=%.17g", tz, pz.x, pz.y, pz.z);
    emitf("line.degen length=%.17g pl=%.17g", dd.length(), dd.partial_length(0.0, 1.0));
    dump_path("line.degen", dd);

    // The copy constructor (line.d:23-26) through a base handle.
    Line cp = Line(ab);
    dump_path("line.copy", cp);
    emitf("line.copy str=%s", cp.toString().c_str());
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <probes/geom_gpath_line.txt>\n";
        return 2;
    }
    port_d_unittest();
    replay_probe();
    return report("line", argv[1], argc > 2);
}
