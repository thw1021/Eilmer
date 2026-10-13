// tests/test_geom_gpath_arc.cpp
//
// Verification of geom/gpath/arc.d (210 D lines) -> src/geom/gpath/arc.{hpp,cpp}.
//
//  A. the `unittest` block of geom/gpath/arc.d (D:162-210) ported assertion for
//     assertion, in source order, with the `version (complex_numbers)` block
//     (D:185-209) carried under EIL_COMPLEX_NUMBERS and not compiled (T-904).
//
//  B. a line-for-line replay of probes/geom/gpath/probe_arc.d against
//     baselines/e587b235/probes/geom_gpath_arc.txt (526 D lines at %.17g).
//     Arc builds a local frame from its three points (arc.d:71-90), rotates in it
//     and transforms back, so the order of the cross products, the normalize()
//     calls and the atan2 branch are all observable. Arc overrides length() with
//     the geometric length while Arc3 does not, and Arc3 solves for its centre by
//     least squares (arc.d:117-140), so each triple dumps its solved centre and the
//     three radii it implies. The five throw paths print which side of the
//     Exception/Error split caught them.
//
// Run: ./test_geom_gpath_arc ../baselines/e587b235/probes/geom_gpath_arc.txt

#include <cmath>
#include <memory>
#include <string>

#include "eil/eil.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/arc.hpp"
#include "geom/gpath/line.hpp"
#include "geom/gpath/path.hpp"
#include "nm/number.hpp"

#include "gpath1_dump.hpp"

using eil::geom::Arc;
using eil::geom::Arc3;
using eil::geom::Path;
using eil::geom::PathPtr;
using eil::geom::Vector3;
using eil::number;

using namespace eil_test_gpath1;

namespace {

// ============================================================================
// A. arc.d's unittest, assertion for assertion, in D's order.
// ============================================================================
void port_d_unittest() {
    // Some simple evaluations.
    Vector3 a = Vector3(eil::Array<number>{2.0, 2.0, 0.0}.all());
    Vector3 b = Vector3(eil::Array<number>{1.0, 2.0, 1.0}.all());
    Vector3 c = Vector3(eil::Array<number>{1.0, 2.0, 0.0}.all());
    Arc abc = Arc(a, b, c);
    Vector3 d = abc(0.5);
    CHECK1(eil::geom::approxEqualVectors(d, Vector3(1.7071068, 2.0, 0.7071068)),
           "D:171 approxEqualVectors(abc(0.5), (1.7071068, 2.0, 0.7071068))");
    Arc3 adb = Arc3(a, d, b);
    CHECK1(eil::geom::approxEqualVectors(d, adb(0.5)), "D:173 approxEqualVectors(d, adb(0.5)) \"Arc3\"");

    // Set up an intersection that should be found on Line
    Arc3 pth = Arc3(Vector3(0.0, 1.0), Vector3(0.5, 1.2), Vector3(1.0, 1.0));
    Vector3 ps = Vector3(0.5, 0.5);
    Vector3 dir = Vector3(0.0, 1.0);
    double t = eil::d_init<double>();
    bool found = pth.intersect2D(ps, dir, t, 10);
    CHECK1(found, "D:181 intersect2D found");
    // intersect2D parametric location on Arc3
    CHECK1(eil::isClose(t, 0.5), "D:183 isClose(t, 0.5)");

#ifdef EIL_COMPLEX_NUMBERS
    // D:185-209 — complex derivative evaluation. Not compiled in the trunk build
    // (backlog T-904); kept verbatim so the wave-12 port can use it.
    {
        Vector3 pc = Vector3(0.0, 0.0);
        double alpha = 1.0;
        Vector3 pa = Vector3(0.0, alpha);
        Vector3 pb = Vector3(alpha, 0.0);
        Arc arc0 = Arc(pa, pb, pc);
        double h = 1.0e-20;
        number ih = eil::Complex<double>(0.0, h);
        number zero = 0.0;
        Vector3 pa_dash = Vector3(zero, alpha + ih);
        Vector3 pb_dash = Vector3(alpha + ih, zero);
        Arc arc1 = Arc(pa_dash, pb_dash, pc);
        // What we want to compute is the sensitivity
        // of the midpoint of the arc with respect to alpha.
        double dpmid_da_x = eil::im(arc1(0.5).x) / h;
        double dpmid_da_y = eil::im(arc1(0.5).y) / h;
        double dpmid_da_z = eil::im(arc1(0.5).z) / h;
        CHECK1(eil::isClose(dpmid_da_x, 1.0 / eil::sqrt(2.0)), "D:206 complex arc dpmid/da x");
        CHECK1(eil::isClose(dpmid_da_y, 1.0 / eil::sqrt(2.0)), "D:207 complex arc dpmid/da y");
        CHECK1(eil::isClose(dpmid_da_z, 0.0), "D:208 complex arc dpmid/da z");
        (void)arc0;
    }
#endif
}

// ============================================================================
// B. the probe replay, in probe order.
// ============================================================================
void replay_probe() {
    // arc.d:166-174 — the quarter arc used by the unittest and by gpath_demo.
    Vector3 a = Vector3(eil::Array<number>{2.0, 2.0, 0.0}.all());
    Vector3 b = Vector3(eil::Array<number>{1.0, 2.0, 1.0}.all());
    Vector3 c = Vector3(eil::Array<number>{1.0, 2.0, 0.0}.all());
    Arc abc = Arc(a, b, c);
    dump_path("arc.qtr", abc);

    // evaluate_position_and_length directly: the out parameters are the raw
    // observables (arc.d:58-101).
    for (double t : {0.0, 0.25, 0.5, 1.0, 1.5, -0.5}) {
        Vector3 loc;  // D: `Vector3 loc;` is Vector3.init
        number L = eil::d_init<number>();
        abc.evaluate_position_and_length(t, loc, L);
        emitf("arc.eval t=%.17g L=%.17g x=%.17g y=%.17g z=%.17g", t, L, loc.x, loc.y, loc.z);
    }

    // The geometric length against Path's chord sum.
    emitf("arc.length geom=%.17g chords=%.17g", abc.length(), abc.Path::length());
    emitf("arc.pl half geom=%.17g chords=%.17g", abc.partial_length(0.0, 0.5),
          abc.Path::partial_length(0.0, 0.5));

    // Arc3 (three points, centre solved by least squares — arc.d:108-143).
    Vector3 d = abc(0.5);
    Arc3 adb = Arc3(a, d, b);
    dump_path("arc3.thru", adb);
    emitf("arc3.thru.length geom=%.17g chords=%.17g", adb.length(), adb.Path::length());

    // The centre solve itself (arc.d:117-140).
    emitf("arc3.centre tag=thru a=%s m=%s b=%s c=%s", a.toString().c_str(), d.toString().c_str(),
          b.toString().c_str(), adb.c.toString().c_str());
    emitf("arc3.centre tag=thru cx=%.17g cy=%.17g cz=%.17g", adb.c.x, adb.c.y, adb.c.z);
    {
        Vector3 ca = adb.a - adb.c;
        Vector3 cm = d - adb.c;
        Vector3 cb = adb.b - adb.c;
        emitf("arc3.centre tag=thru rad a=%.17g m=%.17g b=%.17g", eil::geom::abs(ca),
              eil::geom::abs(cm), eil::geom::abs(cb));
    }
    // The shallow arc of arc.d:176-183, and two triples that leave the xy plane.
    {
        Arc3 sh = Arc3(Vector3(0.0, 1.0), Vector3(0.5, 1.2), Vector3(1.0, 1.0));
        emitf("arc3.centre tag=shallow a=%s m=%s b=%s c=%s", Vector3(0.0, 1.0).toString().c_str(),
              Vector3(0.5, 1.2).toString().c_str(), Vector3(1.0, 1.0).toString().c_str(),
              sh.c.toString().c_str());
    emitf("arc3.centre tag=shallow cx=%.17g cy=%.17g cz=%.17g", sh.c.x, sh.c.y, sh.c.z);
        Vector3 q1 = Vector3(0.0, 0.0, 0.0);
        // q2 is off the right angle at the midpoint of q1-q3: (0,0,0),(1,1,0),(2,0,1)
        // is an exact semicircle, which arc.d:75-79 rejects (measured).
        Vector3 q2 = Vector3(0.8, 1.0, 0.25);
        Vector3 q3 = Vector3(2.0, 0.0, 1.0);
        Arc3 sk = Arc3(q1, q2, q3);
        emitf("arc3.centre tag=skew a=%s m=%s b=%s c=%s", q1.toString().c_str(), q2.toString().c_str(),
              q3.toString().c_str(), sk.c.toString().c_str());
        emitf("arc3.centre tag=skew cx=%.17g cy=%.17g cz=%.17g", sk.c.x, sk.c.y, sk.c.z);
        {
            Vector3 ca = sk.a - sk.c;
            Vector3 cm = q2 - sk.c;
            Vector3 cb = sk.b - sk.c;
            emitf("arc3.centre tag=skew rad a=%.17g m=%.17g b=%.17g", eil::geom::abs(ca),
                  eil::geom::abs(cm), eil::geom::abs(cb));
        }
        Vector3 s5 = sk(0.5);
        emitf("arc3.skew t=0.5 x=%.17g y=%.17g z=%.17g", s5.x, s5.y, s5.z);
        dump_path("arc3.skew", sk);
        // Nearly colinear: nm.bbla calls the matrix singular and throws an
        // *Exception* (bbla.d:789), not the Error arc.d:112 raises.
        dump_error("arc3.err.nearlycolinear", []() {
            Arc3(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 1.0e-7, 0.0), Vector3(2.0, 0.0, 0.0));
        });
    }

    // The Arc3 of arc.d:176-183, the shallow curve used for the intersection test.
    Arc3 pth = Arc3(Vector3(0.0, 1.0), Vector3(0.5, 1.2), Vector3(1.0, 1.0));
    dump_path("arc3.shallow", pth);

    // A near-semicircle and an arc whose sweep is negative (b before a), to pin
    // atan2's branch and the sign of L (arc.d:92-94).
    Arc semi = Arc(Vector3(1.0, 0.0, 0.0), Vector3(-0.98, 0.199, 0.0), Vector3(0.0, 0.0, 0.0));
    dump_path("arc.semi", semi);
    Arc back = Arc(Vector3(1.0, 0.0, 0.0), Vector3(-0.98, -0.199, 0.0), Vector3(0.0, 0.0, 0.0));
    Vector3 b5 = back(0.5);
    emitf("arc.back length=%.17g opCall(0.5) x=%.17g y=%.17g z=%.17g", back.length(), b5.x, b5.y,
          b5.z);

    // Copy constructors.
    dump_path("arc.copy", Arc(abc));
    dump_path("arc3.copy", Arc3(adb));

    // The exact half circle, which D rejects.
    Arc half = Arc(Vector3(1.0, 0.0, 0.0), Vector3(-1.0, 0.0, 0.0), Vector3(0.0, 0.0, 0.0));
    dump_error("arc.err.halfcircle", [&half]() { half(0.5); });

    // Error paths. radii mismatch (arc.d:68-70):
    Arc bad = Arc(Vector3(2.0, 0.0, 0.0), Vector3(0.0, 3.0, 0.0), Vector3(0.0, 0.0, 0.0));
    dump_error("arc.err.radii", [&bad]() { bad(0.5); });
    // Colinear points, no plane (arc.d:75-79).
    Arc col = Arc(Vector3(1.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0), Vector3(0.0, 0.0, 0.0));
    dump_error("arc.err.colinear", [&col]() { col(0.5); });
    // Arc3 with colinear points (arc.d:111-116):
    dump_error("arc3.err.colinear", []() {
        Arc3(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 1.0, 0.0), Vector3(2.0, 2.0, 0.0));
    });
    emitf("arc.class abc=%s adb=%s semi=%s", abc.classString().c_str(), adb.classString().c_str(),
          semi.classString().c_str());
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <probes/geom_gpath_arc.txt>\n";
        return 2;
    }
    port_d_unittest();
    replay_probe();
    return report("arc", argv[1], argc > 2);
}
