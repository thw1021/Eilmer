// tests/test_geom_gpath_gpath_utils.cpp — the C++ mirror of
// probes/geom/gpath/probe_gpath_utils.d (609 lines), replayed line for line against
// baselines/e587b235/probes/geom_gpath_gpath_utils.txt (564 lines).
//
// LAYERS. There is no layer A: geom/gpath/gpath_utils.d has NO unittest block, so this
// probe corpus is the whole gate for the module (the cmake fragment says the same).
// port_d_unittest() is therefore not written here — inventing assertions the D module
// never made would be a different gate, not D's. Nor is there a weaker version of that
// layer: grep over the D module finds no `assert(` in any of the four function bodies
// (its only throws are the four guard Exceptions, which are corpus rows here), so there
// are no D function-body asserts to cover either; the report's "0/0 unittest assertions
// hold" is the honest count, as in tests/test_geom_gpath_arcsteps.cpp. What is covered
// instead, in D's source order:
//   * discretize_path (D:19-28), rows "disc.*": a Path sampled at
//     clusterf.distribute_parameter_values(n) values in order, under a LinearFunction,
//     a RobertsFunction, a QuadraticFunction and the base UnivariateFunction (whose
//     opCall is the identity, univariatefunctions.d:19-22), plus the n=1 and n=0
//     degenerate distributions and one curved path.
//   * readPointsFromFile (D:30-43), rows "read.*": the cowl sample and three files of
//     the tokenisation edges — a line with only an x, a first token exactly "#"
//     (skipped) versus "#c" (a ConversionException, printed with esc()), a missing
//     file (taxonomy only, its message quotes an absolute path), an empty file.
//   * optimiseBezierPoints (D:45-53 file form, D:55-174 point form) and
//     optimiseBezierPoints2 (D:176-315), rows "fit.*": each fit dumps the resulting
//     Bezier (toString, classString, nb, L, the control points, and the curve at six
//     parameter values) and the ts array.
//   * the two guard Exceptions of both functions (D:61-66, :74-80, :184-190,
//     :196-202), rows "fit.err.*" and "fit2.err.*" — including D:184/:197 naming
//     optimiseBezierPoints in ...2's messages, a copy-paste in D that is observable
//     text and is kept.
//
// THE "counted.*" ROWS, AND WHAT THEY PROVE ON THIS SIDE. In D, n_fe/n_restart are
// locals of the fit (gpath_utils.d:147, :282) and never reach the caller, so the number
// of simplex steps a fit took cannot be observed through D's entry point; the probe
// therefore carries line-for-line copies of the two functions whose only additions are
// the writefln of dp, f_min, n_fe, n_restart and the design vector d after minimize(),
// and every counted result is compared with the real function's in rows "cmp.*". The
// port publishes exactly those four values plus d through the optional FitCounters
// out-parameter of src/geom/gpath/gpath_utils.hpp:79-85 and :99/:105/:113 —
// write-only, so with a non-null
// pointer the arithmetic, the call sequence and the return value are the real
// function's. "counted.*" is therefore emitted from the REAL entry point, and "cmp.*"
// compares that call's Bezier and ts against a plain call of the same fit with no
// counter: sameBez/sameTs=true is what attributes the counts to D's code path. A port
// reaching the same optimum in a different number of evaluations, or with a different
// d, still fails here — n_fe/n_restart/d are corpus rows like any other.
//
// TWO D BEHAVIOURS DUMPED, NOT FIXED (probe header :29-38; module notes
// src/geom/gpath/gpath_utils.hpp:44-54):
//   * optimiseBezierPoints2 sizes its design vector 2*(nCtrlPts-2)-2 wide but fills it
//     from i=2 with stride 2, so d[0] and d[1] stay at double.init (they print nan) and
//     the last two entries are later overwritten by the end-slope x values
//     (gpath_utils.d:227-244). The two dead entries make the simplex wider than the
//     parameters the cost function reads, which is why the counts differ from the
//     first fit's. dim=3 is dead on arrival there: the fill runs two entries past the
//     end, so D raises a RangeError (an Error, not an Exception) and so does the port —
//     rows "fit.pts2.dim3*" pin the message text and the taxonomy.
//   * both cost functions share ONE `double t` declared in the enclosing function
//     (D:58, :180) and pass it by ref to closestDistance, so t carries the previous
//     data point's parameter forward. The port keeps it (a pointer to the enclosing
//     function's local inside each capturing functor); it is not restructured anywhere
//     in this test either.
//
// THE ONE ROW THAT DOES NOT MATCH, AND WHY (measured, not inferred). 563 of the 564
// baseline lines are byte-identical; row 86 differs in the text of a std.conv error:
//   D   read.bad asError=false asException=true msg=no digits seen for input "#c".
//   C++ read.bad asError=false asException=true msg=error converting input to floating point for input "#c".
// Same taxonomy (both an Exception, not an Error), same input token; only the message
// differs. The cause is upstream of this module: src/geom/gpath/gpath_utils.cpp:56
// calls eil::to_double, and substrate/include/eil/conv.hpp:88-89 raises
// "error converting input to floating point for input %s." for ANY operand whose first
// character is not a digit, '.', '+' or '-'. ldc2 1.40 splits that case two ways,
// measured with a throwaway D program (16 inputs through to!double, all printed): the
// wording appears only when the operand starts with n/N/i/I, i.e. when the nan/inf
// word match fails -- "n", "N", "i", "I", "nx", "ix" all give it, while
// "NaN"/"nan"/"INF" convert -- and every other non-numeric start gives
// "no digits seen for input %s.": "#c", "abc", "x", "+c", "-c", " 2.0", "e5". So
// conv.hpp:89 is too broad by one predicate and "#c" (the probe's deliberate
// tokenisation edge) lands on the wrong branch. The same over-broad branch will fire
// on the 12 "no digits seen" rows of
// baselines/e587b235/probes/geom_gpath_svgpath.txt. Fixing it means
// editing the substrate, which is outside this slice; the row is therefore left
// byte-compared, not relaxed, and the gap is reported in the PASS/FAIL line's NOTE.
//
// Regenerate the baseline (exact recipe from the probe header; nothing under
// /home/tang/packages/gdtk is written — the reference tree is only READ as the cwd for
// the relative data path, and the copy under /tmp is what gets compiled):
//   cd /home/tang/packages/Eilmer
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
//   /home/tang/packages/dlang/bin/ldc2 -i -I/tmp/gdtk_shadow
//     -I/tmp/gdtk_shadow/extern/gzip probes/geom/gpath/probe_gpath_utils.d
//     -of=/tmp/probe_gpath_utils_d
//   /tmp/probe_gpath_utils_d /home/tang/packages/gdtk/src/geom/sample-data
//     /home/tang/packages/Eilmer/tests/test_data
//     > baselines/e587b235/probes/geom_gpath_gpath_utils.txt
//   (the ldc2 line and the probe line continue on the next comment line only for
//   readability; the real continuation backslashes are in the probe's own header)
//
// Run: ./test_geom_gpath_gpath_utils <baseline.txt> <tests/test_data dir>
//
// SUBSTRATE RULES CHECKED FOR THIS MODULE (brief section 5): grep over the whole call
// chain the corpus exercises -- src/geom/gpath/gpath_utils.cpp, path.cpp, bezier.cpp,
// line.cpp and src/nm/nelmin.cpp -- finds no atan/atan2 call, no EIL_PI use and no iota
// (D's `import std.range : iota` is unused in gpath_utils.d, see the module header
// :14-15), so the Phobos-vs-glibc arc-tangent rule cannot bite here. The only
// transcendental sites reachable from these rows are the cluster functions'
// eil::pow/eil::log in src/geom/misc/univariatefunctions.cpp:208-256, already the
// substrate spellings.
//
// argv: the baseline is argv[1] and the data directory argv[2]
// (cmake/wave2_geom_gpath2.cmake). The probe's argv[1] is gdtk's sample-data dir and
// its argv[2] is tests/test_data; the port is handed one directory because
// tests/test_data/cowl-spline-pts.dat is byte-identical to
// /home/tang/packages/gdtk/src/geom/sample-data/cowl-spline-pts.dat (verified with
// diff during this port: 39 lines, no differences), so "fit.file.*" and "read.cowl"
// read the same bytes either way.
#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/bezier.hpp"
#include "geom/gpath/gpath_utils.hpp"
#include "geom/gpath/line.hpp"
#include "geom/gpath/path.hpp"
#include "geom/misc/univariatefunctions.hpp"
#include "nm/nelmin.hpp"
#include "nm/number.hpp"

#include "gpath3_dump.hpp"

using eil::geom::Bezier;
using eil::geom::BezierPtr;
using eil::geom::FitCounters;
using eil::geom::Line;
using eil::geom::Vector3;
using eil::number;
using eil_test_gpath3::bools;
using eil_test_gpath3::emitf;

namespace {

std::string data_dir;

// The probe's esc() (probe_gpath_utils.d:71-80): D's Error/Exception messages can
// carry newlines, and the corpus is line-oriented, so they are escaped.
std::string esc(const std::string& msg) {
    std::string res;
    for (const char c : msg) {
        if (c == '\n') res += "\\n";
        else if (c == '\r') res += "\\r";
        else res += c;
    }
    return res;
}

// probe_gpath_utils.d:82-88.
void dump_points(const char* tag, const eil::Array<Vector3>& pts) {
    emitf("%s n=%d", tag, static_cast<int>(pts.length()));
    for (size_t i = 0; i < pts.length(); ++i) {
        const Vector3& p = pts[i];
        emitf("%s ptd[%d] x=%.17g y=%.17g z=%.17g", tag, static_cast<int>(i), p.x, p.y, p.z);
    }
}

// probe_gpath_utils.d:90-104.
void dump_bezier(const char* tag, const BezierPtr& bz) {
    emitf("%s str=%s", tag, bz->toString().c_str());
    emitf("%s class=%s", tag, bz->classString().c_str());
    emitf("%s nb=%d L=%.17g", tag, static_cast<int>(bz->B.length()), bz->length());
    for (size_t i = 0; i < bz->B.length(); ++i) {
        const Vector3& p = bz->B[i];
        emitf("%s B[%d] x=%.17g y=%.17g z=%.17g", tag, static_cast<int>(i), p.x, p.y, p.z);
    }
    // Six parameter values on the fitted curve, so a fit that lands on the same
    // control points but evaluates differently still shows up.
    for (double t : {0.0, 0.1, 0.25, 0.5, 0.75, 1.0}) {
        const Vector3 p = (*bz)(t);
        emitf("%s opCall t=%.17g x=%.17g y=%.17g z=%.17g", tag, t, p.x, p.y, p.z);
    }
}

// probe_gpath_utils.d:106-112.
void dump_ts(const char* tag, const eil::Array<double>& ts) {
    emitf("%s n=%d", tag, static_cast<int>(ts.length()));
    for (size_t i = 0; i < ts.length(); ++i) {
        emitf("%s ts[%d]=%.17g", tag, static_cast<int>(i), ts[i]);
    }
}

// probe_gpath_utils.d:114-124. D compares the handles for null first, then every
// control-point coordinate with == (so a NaN in one and not the other is false).
bool same_bezier(const BezierPtr& a, const BezierPtr& b) {
    if (a == nullptr || b == nullptr) return a == b;
    if (a->B.length() != b->B.length()) return false;
    for (size_t i = 0; i < a->B.length(); ++i) {
        if (a->B[i].x != b->B[i].x || a->B[i].y != b->B[i].y || a->B[i].z != b->B[i].z) {
            return false;
        }
    }
    return true;
}

// D's `ts1 == ts2` on double[]: length equality then element-wise ==, which a NaN
// fails. Written out because eil::Array has no operator==.
bool same_ts(const eil::Array<double>& a, const eil::Array<double>& b) {
    if (a.length() != b.length()) return false;
    for (size_t i = 0; i < a.length(); ++i) {
        if (!(a[i] == b[i])) return false;
    }
    return true;
}

// The probe's counted rows: dp, f_min, n_fe, n_restart and the design vector after
// minimize(), tagged "counted.bez" (the optimiseBezierPoints spelling) or
// "counted.bez2" (the optimiseBezierPoints2 spelling), exactly as
// probe_gpath_utils.d:209-213 and :324-328 print them.
void dump_counted(const char* tag, const FitCounters& c, bool success) {
    emitf("%s dp=%.17g f_min=%.17g n_fe=%d n_restart=%d success=%s", tag, c.dp, c.f_min,
          c.n_fe, c.n_restart, bools(success));
    for (size_t i = 0; i < c.d.length(); ++i) {
        emitf("%s d[%d]=%.17g", tag, static_cast<int>(i), c.d[i]);
    }
}

// ---------------------------------------------------------------------------
// groups
// ---------------------------------------------------------------------------
void group_disc() {
    // ---- discretize_path (D:19-28) -----------------------------------------
    auto pth = std::make_shared<Line>(Vector3(0.0, 0.0), Vector3(3.0, 4.0));
    auto lin = std::make_shared<eil::geom::LinearFunction>(0.0, 1.0);
    dump_points("disc.line.linear", eil::geom::discretize_path(pth, 5, lin));
    auto rob = std::make_shared<eil::geom::RobertsFunction>(true, false, 0.3);
    dump_points("disc.line.roberts", eil::geom::discretize_path(pth, 6, rob));
    auto quad = std::make_shared<eil::geom::QuadraticFunction>(4.0, false);
    dump_points("disc.line.quadratic", eil::geom::discretize_path(pth, 7, quad));
    // The base UnivariateFunction as the cluster function: distribute_parameter_values
    // is linear and opCall is the identity (univariatefunctions.d:19-38), so the
    // sampled points march along the line.
    auto base = std::make_shared<eil::geom::UnivariateFunction>();
    dump_points("disc.line.base", eil::geom::discretize_path(pth, 4, base));
    // n = 1 and n = 0 hit the two divide-by-degenerate cases of
    // distribute_parameter_values (see the note in geom/misc/univariatefunctions.hpp).
    dump_points("disc.line.linear.n1", eil::geom::discretize_path(pth, 1, lin));
    dump_points("disc.line.linear.n0", eil::geom::discretize_path(pth, 0, lin));
    // A curved path, so the sampled parameters actually bend the polyline.
    eil::Array<Vector3> bzB = {Vector3(0.0, 0.0), Vector3(1.0, 2.0), Vector3(3.0, 0.0)};
    auto bez = std::make_shared<Bezier>(bzB);
    dump_points("disc.bez.linear", eil::geom::discretize_path(bez, 5, lin));
}

// ---- readPointsFromFile (D:30-43) ---------------------------------------
// cowlPts is read once here and shared with the fit groups, as the probe does with
// its function-scope `cowlPts`.
eil::Array<Vector3> g_cowl;

void group_read() {
    const std::string cowl = data_dir + "/cowl-spline-pts.dat";
    eil::geom::readPointsFromFile(cowl, g_cowl);
    dump_points("read.cowl", g_cowl);

    eil::Array<Vector3> edgePts;
    eil::geom::readPointsFromFile(data_dir + "/gpath_edge.dat", edgePts);
    dump_points("read.edge", edgePts);

    eil::Array<Vector3> p3Pts;
    eil::geom::readPointsFromFile(data_dir + "/gpath_points3d.dat", p3Pts);
    dump_points("read.points3d", p3Pts);

    // A first token of "#c" is not the "#" comment marker, so the reader converts it
    // and std.conv raises a ConversionException -- an Exception. The probe prints the
    // escaped message; eil::ConversionException is an ExceptionBase, not an ErrorBase
    // (brief section 1 item 1: the two are siblings, so which one was caught is an
    // observable).
    const eil_test_gpath3::ErrorTaxonomy bad = eil_test_gpath3::classify([&]() {
        eil::Array<Vector3> badPts;
        eil::geom::readPointsFromFile(data_dir + "/gpath_bad.dat", badPts);
    });
    emitf("read.bad asError=%s asException=%s msg=%s", bools(bad.asError),
          bools(bad.asException), esc(bad.msg).c_str());

    // The message of a FileException quotes the absolute path, which the C++ side is
    // handed differently (argv), so only the taxonomy is compared here.
    const eil_test_gpath3::ErrorTaxonomy missing =
        eil_test_gpath3::classify(
            [&]() {
                eil::Array<Vector3> m;
                eil::geom::readPointsFromFile(data_dir + "/no_such_file_here.dat", m);
            },
            false);
    emitf("read.missing asError=%s asException=%s", bools(missing.asError),
          bools(missing.asException));

    // An empty file leaves points untouched (the caller's array stays empty).
    eil::Array<Vector3> emptyPts;
    eil::geom::readPointsFromFile(data_dir + "/gpath_empty.dat", emptyPts);
    dump_points("read.empty", emptyPts);
}

// ---- the file form of optimiseBezierPoints (D:45-53), the demo's call -----
void group_fit_file() {
    BezierPtr b_init_guess; // D: default-initialised Bezier handle == null
    eil::Array<double> ts;
    bool success = false;
    const BezierPtr bz =
        eil::geom::optimiseBezierPoints(data_dir + "/cowl-spline-pts.dat", 7, b_init_guess, ts, success);
    dump_bezier("fit.file.n7", bz);
    dump_ts("fit.file.n7", ts);
    emitf("fit.file.n7 success=%s", bools(success));
}

// ---- the point form (D:55-174), nCtrlPts = 7 and 5 -----------------------
void group_fit_pts() {
    eil::Array<double> ts1;
    bool success1 = false;
    const BezierPtr real1 =
        eil::geom::optimiseBezierPoints(g_cowl.all(), 7, BezierPtr(), ts1, success1);
    dump_bezier("fit.pts.n7", real1);
    dump_ts("fit.pts.n7", ts1);
    emitf("fit.pts.n7 success=%s", bools(success1));

    eil::Array<double> ts2;
    bool success2 = false;
    FitCounters cnt2;
    const BezierPtr bez2 = eil::geom::optimiseBezierPoints(g_cowl.all(), 7, BezierPtr(), ts2, success2,
                                            1.0e-06, 10000, 2, &cnt2);
    dump_counted("counted.bez", cnt2, success2);
    emitf("cmp.pts.n7 sameBez=%s sameTs=%s", bools(same_bezier(real1, bez2)),
          bools(same_ts(ts1, ts2)));

    // Five control points on the same data (a smaller simplex).
    eil::Array<double> ts3;
    bool success3 = false;
    const BezierPtr real3 =
        eil::geom::optimiseBezierPoints(g_cowl.all(), 5, BezierPtr(), ts3, success3);
    dump_bezier("fit.pts.n5", real3);
    dump_ts("fit.pts.n5", ts3);
    emitf("fit.pts.n5 success=%s", bools(success3));
    eil::Array<double> ts3c;
    bool success3c = false;
    FitCounters cnt3;
    const BezierPtr bez3 = eil::geom::optimiseBezierPoints(g_cowl.all(), 5, BezierPtr(), ts3c, success3c,
                                            1.0e-06, 10000, 2, &cnt3);
    dump_counted("counted.bez", cnt3, success3c);
    emitf("cmp.pts.n5 sameBez=%s sameTs=%s", bools(same_bezier(real3, bez3)),
          bools(same_ts(ts3, ts3c)));
}

// ---- a supplied initial guess (D:73-82) ---------------------------------
// A Bezier through the same endpoints with the interior points pushed away from the
// chord.
void group_fit_guess() {
    eil::Array<Vector3> gb = {Vector3(g_cowl[0].x, g_cowl[0].y), Vector3(0.9, 0.3),
                              Vector3(1.0, 0.25), Vector3(1.1, 0.22), Vector3(1.2, 0.2),
                              Vector3(1.25, 0.19),
                              Vector3(g_cowl[g_cowl.length() - 1].x, g_cowl[g_cowl.length() - 1].y)};
    const BezierPtr guess = std::make_shared<Bezier>(gb);
    eil::Array<double> ts4;
    bool success4 = false;
    const BezierPtr real4 = eil::geom::optimiseBezierPoints(g_cowl.all(), 7, guess, ts4, success4);
    dump_bezier("fit.pts.guess", real4);
    dump_ts("fit.pts.guess", ts4);
    emitf("fit.pts.guess success=%s", bools(success4));
    eil::Array<double> ts4c;
    bool success4c = false;
    FitCounters cnt4;
    const BezierPtr bez4 = eil::geom::optimiseBezierPoints(g_cowl.all(), 7, guess, ts4c, success4c,
                                            1.0e-06, 10000, 2, &cnt4);
    dump_counted("counted.bez", cnt4, success4c);
    emitf("cmp.pts.guess sameBez=%s sameTs=%s", bools(same_bezier(real4, bez4)),
          bools(same_ts(ts4, ts4c)));
    // The guess is copied (D:81 `new Bezier(initGuess)`), so the caller's
    // object is not mutated by the fit.
    emitf("fit.pts.guess kept B0 x=%.17g B3 x=%.17g", guess->B[0].x, guess->B[3].x);
}

// ---- dim = 3 (D:100-113, :109-113): three design values per interior control point.
void group_fit_dim3() {
    eil::Array<double> ts5;
    bool success5 = false;
    const BezierPtr real5 = eil::geom::optimiseBezierPoints(g_cowl.all(), 5, BezierPtr(), ts5, success5,
                                             1.0e-06, 10000, 3);
    dump_bezier("fit.pts.dim3", real5);
    dump_ts("fit.pts.dim3", ts5);
    emitf("fit.pts.dim3 success=%s", bools(success5));
    eil::Array<double> ts5c;
    bool success5c = false;
    FitCounters cnt5;
    const BezierPtr bez5 = eil::geom::optimiseBezierPoints(g_cowl.all(), 5, BezierPtr(), ts5c, success5c,
                                            1.0e-06, 10000, 3, &cnt5);
    dump_counted("counted.bez", cnt5, success5c);
    emitf("cmp.pts.dim3 sameBez=%s sameTs=%s", bools(same_bezier(real5, bez5)),
          bools(same_ts(ts5, ts5c)));
}

// ---- a looser tolerance terminates in fewer steps; the counts are the point.
// The probe reaches this optimum only through its counted copy, so the corpus rows
// are the counted ones first and then the fit's own dump (baseline:384-438).
void group_fit_tol1e3() {
    eil::Array<double> ts6;
    bool success6 = false;
    FitCounters cnt6;
    const BezierPtr cnt = eil::geom::optimiseBezierPoints(g_cowl.all(), 5, BezierPtr(), ts6, success6,
                                           1.0e-03, 10000, 2, &cnt6);
    dump_counted("counted.bez", cnt6, success6);
    dump_bezier("fit.pts.tol1e3", cnt);
    dump_ts("fit.pts.tol1e3", ts6);
}

// ---- optimiseBezierPoints2 (D:176-315) ----------------------------------
void group_fit2() {
    eil::Array<double> ts7;
    bool success7 = false;
    const Vector3 sl = Vector3(1.0, 0.5, 0.0);
    const Vector3 sr = Vector3(1.0, -0.25, 0.0);
    const BezierPtr real7 =
        eil::geom::optimiseBezierPoints2(g_cowl.all(), 7, BezierPtr(), sl, sr, ts7, success7);
    dump_bezier("fit.pts2.n7", real7);
    dump_ts("fit.pts2.n7", ts7);
    emitf("fit.pts2.n7 success=%s", bools(success7));
    eil::Array<double> ts7c;
    bool success7c = false;
    FitCounters cnt7;
    const BezierPtr bez7 = eil::geom::optimiseBezierPoints2(g_cowl.all(), 7, BezierPtr(), sl, sr, ts7c,
                                             success7c, 1.0e-06, 10000, 2, &cnt7);
    dump_counted("counted.bez2", cnt7, success7c);
    emitf("cmp.pts2.n7 sameBez=%s sameTs=%s", bools(same_bezier(real7, bez7)),
          bools(same_ts(ts7, ts7c)));

    // The slope constraint with a guess supplied, and the dim=3 branch
    // (d.length = 3*(nCtrlPts-2) - 4, D:231).
    eil::Array<Vector3> gb = {Vector3(g_cowl[0].x, g_cowl[0].y), Vector3(0.9, 0.3),
                              Vector3(1.0, 0.25), Vector3(1.1, 0.22), Vector3(1.2, 0.2),
                              Vector3(1.25, 0.19),
                              Vector3(g_cowl[g_cowl.length() - 1].x, g_cowl[g_cowl.length() - 1].y)};
    const BezierPtr guess = std::make_shared<Bezier>(gb);
    eil::Array<double> ts8;
    bool success8 = false;
    FitCounters cnt8;
    const BezierPtr bez8 = eil::geom::optimiseBezierPoints2(g_cowl.all(), 7, guess, sl, sr, ts8, success8,
                                             1.0e-06, 10000, 2, &cnt8);
    dump_counted("counted.bez2", cnt8, success8);
    dump_bezier("fit.pts2.guess", bez8);
    dump_ts("fit.pts2.guess", ts8);

    // dim = 3 here is dead on arrival: d.length is 3*(nCtrlPts-2) - 4 while the fill
    // loop writes up to 3*(nCtrlPts-3) - 1, two entries past the end, for every
    // nCtrlPts. D's bounds check fires, and so does the port's (the same
    // "index [N] is out of bounds" RangeError, which is an Error not an Exception).
    eil::Array<double> ts9;
    bool success9 = false;
    const eil_test_gpath3::ErrorTaxonomy cnt9 = eil_test_gpath3::classify([&]() {
        const BezierPtr b = eil::geom::optimiseBezierPoints2(g_cowl.all(), 6, BezierPtr(), sl, sr, ts9,
                                              success9, 1.0e-06, 10000, 3);
        dump_bezier("fit.pts2.dim3", b);
        dump_ts("fit.pts2.dim3", ts9);
    });
    emitf("fit.pts2.dim3 asError=%s asException=%s msg=%s", bools(cnt9.asError),
          bools(cnt9.asException), esc(cnt9.msg).c_str());
    // The real function, same input: the guard fires at the same place.
    eil::Array<double> ts9r;
    bool success9r = false;
    const eil_test_gpath3::ErrorTaxonomy r9 = eil_test_gpath3::classify([&]() {
        const BezierPtr b = eil::geom::optimiseBezierPoints2(g_cowl.all(), 6, BezierPtr(), sl, sr, ts9r,
                                              success9r, 1.0e-06, 10000, 3);
        dump_bezier("fit.pts2.dim3real", b);
    });
    emitf("fit.pts2.dim3real asError=%s asException=%s msg=%s", bools(r9.asError),
          bools(r9.asException), esc(r9.msg).c_str());
}

// ---- the two guards (D:61-66, :74-80) ----------------------------------
void group_guards() {
    eil::Array<double> tsg;
    bool sg = false;
    const Vector3 sl = Vector3(1.0, 0.5, 0.0);
    const Vector3 sr = Vector3(1.0, -0.25, 0.0);
    const eil_test_gpath3::ErrorTaxonomy toofew = eil_test_gpath3::classify([&]() {
        const BezierPtr bz = eil::geom::optimiseBezierPoints(g_cowl.all(), 40, BezierPtr(), tsg, sg);
        emitf("fit.err.toofew built=1 nb=%d", static_cast<int>(bz->B.length()));
    });
    if (toofew.asError || toofew.asException) {
        emitf("fit.err.toofew built=0 asError=%s asException=%s msg=%s", bools(toofew.asError),
              bools(toofew.asException), esc(toofew.msg).c_str());
    }
    eil::Array<Vector3> g3b = {Vector3(0.0), Vector3(0.5, 0.5), Vector3(1.0)};
    const BezierPtr guess3 = std::make_shared<Bezier>(g3b);
    const eil_test_gpath3::ErrorTaxonomy guess = eil_test_gpath3::classify([&]() {
        const BezierPtr bz = eil::geom::optimiseBezierPoints(g_cowl.all(), 7, guess3, tsg, sg);
        emitf("fit.err.guess built=1 nb=%d", static_cast<int>(bz->B.length()));
    });
    if (guess.asError || guess.asException) {
        emitf("fit.err.guess built=0 asError=%s asException=%s msg=%s", bools(guess.asError),
              bools(guess.asException), esc(guess.msg).c_str());
    }
    // The same two guards in optimiseBezierPoints2 (its messages name
    // optimiseBezierPoints, D:184 and D:197 — a copy-paste in D that is kept).
    const eil_test_gpath3::ErrorTaxonomy toofew2 = eil_test_gpath3::classify([&]() {
        const BezierPtr bz =
            eil::geom::optimiseBezierPoints2(g_cowl.all(), 40, BezierPtr(), sl, sr, tsg, sg);
        emitf("fit2.err.toofew built=1 nb=%d", static_cast<int>(bz->B.length()));
    });
    if (toofew2.asError || toofew2.asException) {
        emitf("fit2.err.toofew built=0 asError=%s asException=%s msg=%s", bools(toofew2.asError),
              bools(toofew2.asException), esc(toofew2.msg).c_str());
    }
    const eil_test_gpath3::ErrorTaxonomy guess2 = eil_test_gpath3::classify([&]() {
        const BezierPtr bz =
            eil::geom::optimiseBezierPoints2(g_cowl.all(), 7, guess3, sl, sr, tsg, sg);
        emitf("fit2.err.guess built=1 nb=%d", static_cast<int>(bz->B.length()));
    });
    if (guess2.asError || guess2.asException) {
        emitf("fit2.err.guess built=0 asError=%s asException=%s msg=%s", bools(guess2.asError),
              bools(guess2.asException), esc(guess2.msg).c_str());
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) return 2;
    data_dir = argv[2];
    group_disc();
    group_read();
    group_fit_file();
    group_fit_pts();
    group_fit_guess();
    group_fit_dim3();
    group_fit_tol1e3();
    group_fit2();
    group_guards();
    return eil_test_gpath3::report("gpath_utils", argv[1],
        "the single differing row is read.bad's std.conv message text: cause is "
        "substrate/include/eil/conv.hpp:89 raising 'error converting input to floating "
        "point' for any non-digit/./+/- start, where ldc2 1.40's std.conv reserves that "
        "wording for failed nan/inf word matches (n/N/i/I starts) and gives 'no digits "
        "seen for input \"#c\".' otherwise (measured with a 16-input to!double probe). "
        "Fix is in the substrate, outside this slice; the row is left byte-compared.");
}
