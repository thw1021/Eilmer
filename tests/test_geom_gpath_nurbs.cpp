// tests/test_geom_gpath_nurbs.cpp
//
// Two layers, both required (the pattern of tests/test_geom_gpath_xspline.cpp):
//
//  A. the `unittest` block of geom/gpath/nurbs.d (D:101-114) ported assertion for
//     assertion, in source order. That block is three payload declarations (D:102-111),
//     one construction (D:112) and exactly one assert (D:113, approxEqualVectors
//     against the printed reference point), so it contributes one CHECK and no corpus
//     row. The same point is rows "un.*" of layer B: an assertion that holds says
//     nothing about which digit moved, so the probe prints what it asserts.
//
//  B. a line-for-line replay of probes/geom/gpath/probe_nurbs.d against
//     baselines/e587b235/probes/geom_gpath_nurbs.txt — 195 lines at %.17g, emitted in
//     the probe's own order, each block measuring one of the four things its header
//     says are measured rather than assumed:
//       * rows 1-8, 91-92, 188 — toString()/classString()/dup()/the copy ctor. The
//         `to!string` of a `double[4][]` and a `double[]` goes through the port's own
//         eil::to_string machinery (d_tostr, src/geom/gpath/path.hpp), which is what
//         nurbs.cpp:105-108 calls, so the bracket/comma/number rendering is the
//         subject of the row and not something the test may hand-roll;
//       * rows 9-90 and 93-175 — the whole Path interface over the degree-2 and
//         degree-5 curves (tests/gpath3_dump.hpp dump_path, the C++ side of
//         probes/geom/gpath/pathdump.d) plus the u sweep, whose mid-knot values pin
//         which span findSpan picks;
//       * rows 176-181 — the four STATIC fields mN, mCw, mC, mNws (D:82-85), shared by
//         every NURBS in the process, each constructor resizing mN to its own p+1. A
//         degree-5 curve held across the construction of a degree-2 one reads mN[3] of
//         a 3-slot array, so rows nurbs.static.keep5 and nurbs.static.retry are D's
//         RangeError text and nurbs.static.rebuild5 shows the value returning once mN
//         is sized back to 6. Reproduced, not worked around (src/geom/gpath/nurbs.hpp,
//         non-1:1 note 2);
//       * rows 182-195 — the three constructor guards (D:23-42), which are `Error`s
//         and therefore eil::ErrorBase, and the parameter edges: u at the last knot,
//         u one ULP past it (the tol = 1e-12 pull-back at nurbs_utils.d:34-36), u well
//         outside (findSpan's eil::ExceptionBase) and the NaN parameter. Error and
//         Exception are sibling types in this port, so each row prints which of the
//         two caught and esc(msg) with \n/\r escaped, as probe_nurbs.d:50-59 does.
//
// tests/gpath3_dump.hpp is the harness the later gpath stages share; gpath1_dump.hpp's
// dump_error() has no esc() step to hook into, and this probe's rows are all
// message-bearing.
//
// Regenerate the baseline (recipe in the probe header; ldc2 1.40 against a copy of the
// read-only reference tree with the @nogc attributes stripped):
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip
//        -I probes/geom/gpath probes/geom/gpath/probe_nurbs.d -of=/tmp/probe_nurbs_d
//   /tmp/probe_nurbs_d > baselines/e587b235/probes/geom_gpath_nurbs.txt
//
// Run: ./test_geom_gpath_nurbs <probes/geom_gpath_nurbs.txt>
// EIL_DUMP_PRODUCED=<file> writes the C++ corpus for a full `diff -u`.
#include <array>
#include <functional>
#include <initializer_list>
#include <limits>
#include <memory>
#include <string>

#include "eil/eil.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/nurbs.hpp"
#include "geom/gpath/path.hpp"
#include "nm/number.hpp"

#include "gpath3_dump.hpp"

using eil::geom::NURBS;
using eil::geom::Vector3;
using eil_test_gpath3::bools;
using eil_test_gpath3::emitf;

namespace {

// A weighted control-point net, built the way the probe builds its literals.
eil::Array<std::array<double, 4>> pw(std::initializer_list<std::array<double, 4>> v) {
    eil::Array<std::array<double, 4>> a;
    for (const std::array<double, 4>& x : v) a.append(x);
    return a;
}

eil::Array<double> arr(std::initializer_list<double> v) {
    eil::Array<double> a;
    for (const double& x : v) a.append(x);
    return a;
}

// esc() of probe_nurbs.d:50-59.
std::string esc(const std::string& s) {
    std::string res;
    for (const char c : s) {
        if (c == '\n') res += "\\n";
        else if (c == '\r') res += "\\r";
        else res += c;
    }
    return res;
}

// probe_nurbs.d:64-70 — the u sweep D's own unittest value plus the knot
// boundaries and the mid-knot values.
void sweep(const char* tag, const NURBS& nc) {
    for (const double u : {0.0, 0.001, 0.375, 0.4, 0.5, 0.6, 0.625, 0.9, 0.999, 1.0}) {
        const Vector3 pt = nc(u);
        emitf("%s opCall u=%.17g x=%.17g y=%.17g z=%.17g", tag, u, pt.x, pt.y, pt.z);
    }
}

// probe_nurbs.d:170-190 and :226-251 — the guarded evaluations. `print_u` selects
// which of the probe's two success-row shapes is used; the throw rows are the same
// in both, and print which side of the Error/Exception sibling pair caught plus
// esc(msg), as the probe's own two catch clauses do.
void guarded(const char* tag, bool print_u, double u,
             const std::function<Vector3()>& f) {
    Vector3 pv; // D's uninitialised local: Vector3.init, NaN fields
    const auto t = eil_test_gpath3::classify([&]() { pv = f(); });
    if (t.asError || t.asException) {
        emitf("%s asError=%s asException=%s msg=%s", tag, bools(t.asError),
              bools(t.asException), esc(t.msg).c_str());
    } else if (print_u) {
        emitf("%s u=%.17g x=%.17g y=%.17g z=%.17g", tag, u, pv.x, pv.y, pv.z);
    } else {
        emitf("%s x=%.17g y=%.17g z=%.17g", tag, pv.x, pv.y, pv.z);
    }
}

// probe_nurbs.d:72-82 — one construction attempt, reported as the probe reports it:
// either "built=1 str=..." or the taxonomy and esc(msg). D's two catch clauses are
// the two sibling branches of the port's Throwable pair, so which one fired is part
// of the observable.
void build(const char* tag, eil::Slice<std::array<double, 4>> Pw, eil::Slice<double> U,
           int p) {
    std::string str;
    const auto t = eil_test_gpath3::classify([&]() {
        const NURBS nc(Pw, U, p);
        str = nc.toString();
    });
    if (t.asError || t.asException) {
        emitf("%s built=0 asError=%s asException=%s msg=%s", tag, bools(t.asError),
              bools(t.asException), esc(t.msg).c_str());
    } else {
        emitf("%s built=1 str=%s", tag, str.c_str());
    }
}

// ============================================================================
// A. the D `unittest` block of geom/gpath/nurbs.d (D:101-114), assertion for
//    assertion, in source order. The block is three payload declarations (D:102-111),
//    one construction (D:112) and a single assert (D:113). The literals are written
//    out again here rather than shared with replay_probe() because D writes them out
//    twice too — once in the unittest, once in the probe — and the payload is part of
//    what each layer pins.
// ============================================================================
void port_d_unittest() {
    // D:102-109 — the six weighted control points, in D's own spelling (the third
    // row is written as 2.0*5 / -4.0*5 and the last as 4*2.772 / 4*3.875 / 4*1.736,
    // so the products are computed, not typed as decimals).
    eil::Array<std::array<double, 4>> Pw = pw({
        std::array<double, 4>{{-4.0, -4.0, 0.0, 1.0}},
        std::array<double, 4>{{-2.0, 4.0, 0.0, 1.0}},
        std::array<double, 4>{{2.0 * 5, -4.0 * 5, 0.0, 5}},
        std::array<double, 4>{{4.0, 4.0, 0.0, 1.0}},
        std::array<double, 4>{{3.778, 1.836, 2.933, 1.0}},
        std::array<double, 4>{{4 * 2.772, -4 * 3.875, 4 * 1.736, 4.0}}
    });
    int p = 2;                                   // D:110
    eil::Array<double> U = arr({0.0, 0.0, 0.0, 0.375, 0.5, 0.625, 1.0, 1.0, 1.0}); // D:111
    auto ncurve = std::make_shared<NURBS>(Pw.all(), U.all(), p);                   // D:112
    // D:113
    CHECK3(eil::geom::approxEqualVectors(Vector3(3.782, 2.939, 0.435), (*ncurve)(0.6)),
           "D:113 approxEqualVectors(Vector3(3.782, 2.939, 0.435), ncurve(0.6))");
}

// The whole replay, in probe order: probe_nurbs.d:84-252.
void replay_probe() {
    // ---- nurbs.d's own unittest (D:101-114) --------------------------------
    eil::Array<std::array<double, 4>> Pw = pw({
        std::array<double, 4>{{-4.0, -4.0, 0.0, 1.0}},
        std::array<double, 4>{{-2.0, 4.0, 0.0, 1.0}},
        std::array<double, 4>{{2.0 * 5, -4.0 * 5, 0.0, 5}},
        std::array<double, 4>{{4.0, 4.0, 0.0, 1.0}},
        std::array<double, 4>{{3.778, 1.836, 2.933, 1.0}},
        std::array<double, 4>{{4 * 2.772, -4 * 3.875, 4 * 1.736, 4.0}}
    });
    int p = 2;
    eil::Array<double> U = arr({0.0, 0.0, 0.0, 0.375, 0.5, 0.625, 1.0, 1.0, 1.0});
    auto ncurve = std::make_shared<NURBS>(Pw.all(), U.all(), p);
    const Vector3 puni = (*ncurve)(0.6);
    emitf("un.pt x=%.17g y=%.17g z=%.17g", puni.x, puni.y, puni.z);
    emitf("un.approxEqual=%s",
          bools(eil::geom::approxEqualVectors(Vector3(3.782, 2.939, 0.435), puni)));
    emitf("un.isClose x=%s y=%s z=%s", bools(eil::isClose(puni.x, 3.782)),
          bools(eil::isClose(puni.y, 2.939)), bools(eil::isClose(puni.z, 0.435)));

    // toString/classString/dup (D:60-75).
    emitf("nurbs.str=%s", ncurve->toString().c_str());
    emitf("nurbs.class=%s", ncurve->classString().c_str());
    auto ndup = std::static_pointer_cast<NURBS>(ncurve->dup());
    emitf("nurbs.dup.class=%s", ndup->classString().c_str());
    emitf("nurbs.dup.str=%s", ndup->toString().c_str());
    const Vector3 pdup = (*ndup)(0.6);
    emitf("nurbs.dup u=%.17g x=%.17g y=%.17g z=%.17g", 0.6, pdup.x, pdup.y, pdup.z);

    // The whole Path interface through the NURBS (dpdt/d2pdt2 are Path's finite
    // differences; length/closestDistance/intersect2D all go through opCall).
    eil_test_gpath3::dump_path("nurbs.p2", *ncurve);
    sweep("nurbs.p2", *ncurve);

    // The copy constructor (D:51-59): same payload, no validation, and it resizes
    // the shared mN to the copied degree.
    auto ncopy = std::make_shared<NURBS>(*ncurve);
    emitf("nurbs.copy str=%s", ncopy->toString().c_str());
    const Vector3 pc = (*ncopy)(0.6);
    emitf("nurbs.copy u=%.17g x=%.17g y=%.17g z=%.17g", 0.6, pc.x, pc.y, pc.z);

    // ---- a degree-5 curve on a clamped uniform knot vector -----------------
    eil::Array<std::array<double, 4>> Pw5 = pw({
        std::array<double, 4>{{0.0, 0.0, 0.0, 1.0}},
        std::array<double, 4>{{1.0, 2.0, 0.0, 1.0}},
        std::array<double, 4>{{2.0, 1.0, 0.5, 2.0}},
        std::array<double, 4>{{3.0, 3.0, 1.0, 1.0}},
        std::array<double, 4>{{4.0, 1.5, 0.0, 0.5}},
        std::array<double, 4>{{5.0, 2.5, 0.0, 1.0}}
    });
    eil::Array<double> U5 =
        arr({0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0});
    {
        auto nc5 = std::make_shared<NURBS>(Pw5.all(), U5.all(), 5);
        emitf("nurbs.p5.str=%s", nc5->toString().c_str());
        sweep("nurbs.p5", *nc5);
        eil_test_gpath3::dump_path("nurbs.p5", *nc5);
    }

    // ---- the static fields, in both construction orders -------------------
    // mN/mNws are module-wide; the last constructor wins. Degree 5 first, then a
    // degree-2 curve: mN is left with 6 slots, and the degree-2 evaluation only
    // writes 0..2 of them, so it agrees with the standalone result above.
    // (probe_nurbs.d:142-145, carried as written. Measured on the port — and the
    // keep5 row below measures it on D — `mN.length = 3` SHRINKS the shared array, so
    // the last constructor wins in both directions; the after5 row agrees with the
    // standalone one because a degree-2 evaluation only ever touches slots 0..2.)
    {
        eil::Array<std::array<double, 4>> small2 = pw({
            std::array<double, 4>{{-4.0, -4.0, 0.0, 1.0}},
            std::array<double, 4>{{-2.0, 4.0, 0.0, 1.0}},
            std::array<double, 4>{{10.0, -20.0, 0.0, 5.0}},
            std::array<double, 4>{{4.0, 4.0, 0.0, 1.0}},
            std::array<double, 4>{{3.778, 1.836, 2.933, 1.0}},
            std::array<double, 4>{{11.088, -15.5, 6.944, 4.0}}
        });
        eil::Array<double> U2 = arr({0.0, 0.0, 0.0, 0.375, 0.5, 0.625, 1.0, 1.0, 1.0});
        auto after5 = std::make_shared<NURBS>(small2.all(), U2.all(), 2);
        const Vector3 pa = (*after5)(0.6);
        emitf("nurbs.static.after5 u=%.17g x=%.17g y=%.17g z=%.17g", 0.6, pa.x, pa.y, pa.z);
        // Now a degree-5 curve is built and evaluated, which resizes mN to 6 again.
        auto nc5b = std::make_shared<NURBS>(Pw5.all(), U5.all(), 5);
        const Vector3 pb = (*nc5b)(0.5);
        emitf("nurbs.static.p5again u=%.17g x=%.17g y=%.17g z=%.17g", 0.5, pb.x, pb.y, pb.z);
        // And a degree-4 curve on a 5-point net: mN becomes 5 wide, so the
        // degree-2 curve above still evaluates (it reads only 0..2), but a
        // freshly built degree-6 curve would find mN too narrow.
        // (probe_nurbs.d:158-160, carried verbatim; the construction below is in fact
        // another degree-2 curve on the 6-point net, so mN goes to 3 slots and row
        // nurbs.static.p2again measures the shrink-and-evaluate order instead.)
        auto nc2c = std::make_shared<NURBS>(small2.all(), U2.all(), 2);
        const Vector3 pc2 = (*nc2c)(0.375);
        emitf("nurbs.static.p2again u=%.17g x=%.17g y=%.17g z=%.17g", 0.375, pc2.x, pc2.y,
              pc2.z);
        // The interference itself: hold on to a degree-5 curve, then construct a
        // degree-2 one (which resizes the shared mN to 3 slots), then evaluate the
        // degree-5 curve. basisFuns writes N[3] into mN and D's bounds check fires.
        auto keep5 = std::make_shared<NURBS>(Pw5.all(), U5.all(), 5);
        auto shrink = std::make_shared<NURBS>(small2.all(), U2.all(), 2);
        (void)shrink; // the construction itself is the observable: it resizes the shared mN
        guarded("nurbs.static.keep5", false, 0.0, [&]() { return (*keep5)(0.5); });
        // mCw (D:83) is a static double[4] and mC (D:84) a static double[3], so they
        // are shared too, but deBoor zeroes both before every use, so the sharing is
        // not observable through opCall. Rebuilding the degree-5 curve restores mN,
        // and the value matches the earlier p5 row for the same u.
        guarded("nurbs.static.retry", true, 0.25, [&]() { return (*keep5)(0.25); });
        auto rebuild5 = std::make_shared<NURBS>(Pw5.all(), U5.all(), 5);
        const Vector3 prb = (*rebuild5)(0.5);
        emitf("nurbs.static.rebuild5 u=%.17g x=%.17g y=%.17g z=%.17g", 0.5, prb.x, prb.y,
              prb.z);
    }

    // ---- constructor guards (D:23-42), all `throw new Error` --------------
    eil::Array<std::array<double, 4>> one = pw({std::array<double, 4>{{0.0, 0.0, 0.0, 1.0}}});
    eil::Array<std::array<double, 4>> two = pw({
        std::array<double, 4>{{0.0, 0.0, 0.0, 1.0}},
        std::array<double, 4>{{1.0, 1.0, 0.0, 1.0}}
    });
    eil::Array<std::array<double, 4>> three = pw({
        std::array<double, 4>{{0.0, 0.0, 0.0, 1.0}},
        std::array<double, 4>{{1.0, 1.0, 0.0, 1.0}},
        std::array<double, 4>{{2.0, 0.0, 0.0, 1.0}}
    });
    (void)three; // declared in the probe for completeness; no build() row uses it
    eil::Array<std::array<double, 4>> none;
    eil::Array<double> noU;
    eil::Array<double> k2 = arr({0.0, 0.0, 1.0});   // err.one, err.degree
    eil::Array<double> k1 = arr({0.0, 1.0});        // err.knots, err.zero
    eil::Array<double> k3 = arr({0.0, 0.5, 1.0});   // err.knotsextra
    eil::Array<double> k4 = arr({0.0, 0.0, 1.0, 1.0}); // ok.linear
    build("nurbs.err.none", none.all(), noU.all(), 1);
    build("nurbs.err.one", one.all(), k2.all(), 1);
    build("nurbs.err.degree", two.all(), k2.all(), 2);
    build("nurbs.err.knots", two.all(), k1.all(), 1);
    build("nurbs.err.knotsextra", two.all(), k3.all(), 1);
    build("nurbs.err.zero", two.all(), k1.all(), 0);
    // A valid linear curve, for the record (a=1, p=1 needs 4 knots).
    build("nurbs.ok.linear", two.all(), k4.all(), 1);

    // ---- parameter range edges -------------------------------------------
    {
        auto nc = std::make_shared<NURBS>(Pw.all(), U.all(), 2);
        const Vector3 pend = (*nc)(1.0);
        emitf("nurbs.edge u=%.17g x=%.17g y=%.17g z=%.17g", 1.0, pend.x, pend.y, pend.z);
        const Vector3 pstart = (*nc)(0.0);
        emitf("nurbs.edge u=%.17g x=%.17g y=%.17g z=%.17g", 0.0, pstart.x, pstart.y, pstart.z);
        // 1 ULP above the last knot: nurbs_utils.d:34-36 pulls u back inside the
        // range with tol = 1e-12, so this evaluates rather than throwing.
        const double just_over = 1.0 + 1e-13;
        const Vector3 pio = (*nc)(just_over);
        emitf("nurbs.edge u=%.17g x=%.17g y=%.17g z=%.17g", just_over, pio.x, pio.y, pio.z);
        const double just_under = 0.0 - 1e-13;
        const Vector3 piu = (*nc)(just_under);
        emitf("nurbs.edge u=%.17g x=%.17g y=%.17g z=%.17g", just_under, piu.x, piu.y, piu.z);
        // Well outside: findSpan throws an Exception (not an Error).
        guarded("nurbs.outside", false, 0.0, [&]() { return (*nc)(1.5); });
        guarded("nurbs.below", false, 0.0, [&]() { return (*nc)(-0.25); });
        // NaN parameter: every comparison in findSpan is false, so the binary
        // search walks until it reads out of range or terminates. Dumped as-is.
        // Measured on both sides: it terminates (the binary search at
        // nurbs_utils.cpp:47-57 tests `u < U[mid] || u >= U[mid+1]`, both false for
        // NaN, so findSpan returns the mid it first computed), basisFuns propagates
        // the NaN through left/right, and the row is nan/nan/nan rather than a throw.
        guarded("nurbs.nan", false, 0.0,
                [&]() { return (*nc)(std::numeric_limits<double>::quiet_NaN()); });
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <probes/geom_gpath_nurbs.txt>\n";
        return 2;
    }
    port_d_unittest();
    replay_probe();
    return eil_test_gpath3::report("nurbs", argv[1]);
}
