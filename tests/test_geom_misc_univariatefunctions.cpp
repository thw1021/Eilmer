// tests/test_geom_misc_univariatefunctions.cpp
//
// Two layers, both required (the pattern of tests/test_geom_misc_svg.cpp and
// tests/test_geom_elements_properties.cpp):
//
//  A. the `unittest` block of geom/misc/univariatefunctions.d (D:769-833) ported
//     assertion-for-assertion, in D's order, with D's own tolerance arguments:
//     10 `isClose(a, b, 1.0e-4)` calls over the five functions the block constructs,
//     plus test_gaussian_function_curvature (D:793-812), which loops m over
//     [0.0, 0.1, 0.5, 0.9, 1.0], inverts the cluster function by the block's own
//     bisect_iterate (D:771-791), and asserts slope >= 0.0 and |curvature| <= 1.0e-3
//     from its h=1.0e-3 finite differences. 20 assertions in total (10 isClose, 10 from
//     the curvature loop's 5 values x 2 asserts), all of which hold.
//     D's `bisect_iterate` counts its iterations in a local that is never read; the
//     port keeps that dead increment (layer B needs the count, so the same body can
//     optionally report it — one loop, no second copy of the bisection).
//
//  B. a line-for-line replay of probes/geom/misc/probe_univariatefunctions.d against
//     baselines/e587b235/probes/geom_misc_univariatefunctions.txt: 857 lines covering
//     the base class and all 8 UnivariateFunction subclasses defined in this module
//     (LinearFunction, QuadraticFunction, RobertsFunction, GeometricFunction,
//     GaussianFunction, GaussGeomHybridFunction, VinokurFunction,
//     VinokurGeomHybridFunction) over f(t) sweeps, toString() text,
//     distribute_parameter_values arrays, dup()/copy-ctor NaN propagation, all 24
//     Error/Exception guard paths with message text and taxonomy, the
//     distribute_parameter_values n<=1 edge cases, and the trip counts of the three
//     converging loops (lambertW, the Gaussian centre Newton, the hybrid switch-point
//     Newton). Every quantity is printed at %.17g. The comparison is byte-exact on
//     structure (text, keys, lengths, iteration counts, NaN sign) and, for real
//     quantities, bit-exact unless within the 1e-13/1e-14 transcendental tolerance the
//     substrate-gap note at the end of this header explains; the summary line reports
//     how many lines and quantities fell into each class.
//
// READ THIS — why the D baseline is not from the module itself. ldc2 1.40.0 cannot
// compile geom/misc/univariatefunctions.d: its two Vinokur constructors call
// nm/brent.d's `solve`, which is `@nogc` yet does `new Exception` (brent.d:51, :123),
// so every instantiation is rejected:
//
//   nm/brent.d(51): Error: cannot use `new` in `@nogc` function
//       `geom.misc.univariatefunctions.VinokurFunction.this.solve!(zeroFun,double).solve`
//
// The corpus was therefore captured from the mechanical copies in probes/geom/misc/
// (univariatefunctions_copy.d, brent_copy.d, univariatefunctions_demo_copy.d), whose
// ENTIRE difference from the originals is: a module rename, `import nm.brent` ->
// `import brent_copy`, one `@nogc` attribute dropped from solve, and three
// statements that count loops D does not expose. `python3 probes/geom/misc/gen_d_copies.py
// --check` prints that diff and re-verifies the copies (run 2026-10-14: no drift). No
// expression, operand order, tolerance, branch or message text was touched. The
// `@nogc` removal is ASSUMED not to affect arithmetic — see the note at the bottom of
// this header for what this corpus can and cannot say about that assumption.
//
// The D probe must be built with --d-debug=1 (the two Vinokur catch blocks build
// their message inside `debug { ... }`, and the frozen reference is a debug build);
// the C++ port keeps that text unconditionally, so both sides print it.
//
// Run: ./test_geom_misc_univariatefunctions ../baselines/e587b235/probes/geom_misc_univariatefunctions.txt
//
// Regenerate the baseline:
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   cd /home/tang/packages/Eilmer
//   ldc2 --d-debug=1 -i -I/home/tang/packages/gdtk/src -Iprobes/geom/misc probes/geom/misc/probe_univariatefunctions.d -of=/tmp/uf_d
//   /tmp/uf_d > baselines/e587b235/probes/geom_misc_univariatefunctions.txt
//   (run 2026-10-14: that command reproduces the committed 857 lines byte for byte.
//   The build line is kept on one physical line because a backslash at the end of a
//   // comment line is what -Wcomment warns about.)
//
// Format-string note: every line below is built with eil::format, i.e. the same
// renderer the port measures against Phobos, so the format strings here are the
// probe's strings verbatim. That matters in two ways: D's `%g` is `%.6g` (not the
// shortest round trip), and D's `%d` accepts an integer where C's `%g` would need a
// double. Never pass an int to `%g` — UB in C++, though D's std.format prints an
// integer there.
//
// Constant-fold note: ldc2 folds literal-only arithmetic through 80-bit extended
// precision and the result can be 1 ULP off a run-time division (svg.d:55 `90.0/25.4`,
// see probes/substrate/probe_fold_div.d). Every arithmetic site in this module has at
// least one non-literal operand (ratio, beta, a, r, ns, pi, m, s, x), so nothing in it
// is foldable and no D constant had to be taken as a hex literal here. Measured
// 2026-10-14 on the expression shapes of this module: `(1.1+1.0)/(1.1-1.0)` folds to
// 21 but computes as 20.999999999999982 at run time, and `1.0/(1.0+0.3275911*0.5)`
// folds to 0.85925745290914712 where the run-time value is 0.85925745290914701 — the
// hazard is real for these shapes, and the roberts rows replaying bit-identically
// shows D used the run-time values, as its source structure says it must (the only
// gaussian rows that move are the two exp-driven ones named in the substrate note).
//
// The @nogc assumption, and what this corpus can say about it: `@nogc` is a
// compile-time memory-management attribute; dropping it changes no operand, no
// evaluation order and no constant, and gen_d_copies.py proves the copy is otherwise
// text-identical. The corpus by itself CANNOT falsify the assumption, because the
// unmodified module does not compile with this toolchain, so there is no second D
// measurement of the same quantity to compare against. What the replay does give is a
// cross-implementation check: src/nm/brent.hpp's solve was ported from the ORIGINAL
// nm/brent.d (where @nogc has no C++ counterpart at all), and where brent's iteration
// does not step on a differing transcendental it lands on exactly the D copy's root —
// measured directly: beta for VinokurFunction(40,0.01,0.02) is 1.952162467438211 on
// both sides, for VinokurFunction(100,0.001,0.01) 2.9183965017079974 on both sides, and
// for VinokurGeomHybridFunction(20,0.02,2,1.5,0.02,3,0.8) 2.7314110761533215 on both
// sides; the two brent failure messages print the same f(x1)=1.66667e-07 and
// f(x2)=1.80649e+84. So the copy's one removed attribute is not what made those numbers
// what they are. (4 of the 7 brackets do land 1 ULP apart; the substrate note below
// attributes that to sinh, the function brent iterates on, not to the attribute.)
//
// KNOWN SUBSTRATE GAP (42 of 857 lines; the summary line prints the live counts).
// D's std.math.exp/tanh/sinh/cosh are Phobos' own implementations, NOT libm calls.
// Measured 2026-10-14 over 4001 arguments on [0,2], from inside D: std.math.exp differs
// from core.stdc.math.exp (glibc, double) on 569 arguments (14%) and also from
// double(glibc expl(x)) on those same arguments, always by 1 ULP of the function value;
// cosh differs on 699/4001, sinh on 1933/4001, tanh on 1286/4001 (and tanh is not
// sign-symmetric in Phobos, though glibc's is). No call substitution on the C++ side
// reproduces it, and porting Phobos' exp into the substrate is a cross-module decision
// (it would move every module's transcendental output), NOT something to settle inside
// geom/misc. Effects in this corpus are confined to the transcendental-bearing rows:
//   * the two Vinokur classes (40 lines): opCall's tanh, and the beta brent converges
//     to through sinh — measured betas agree with the C++ port for 3 of the 7 Vinokur
//     brackets and differ by 1 ULP for 4 (vk50, vg40, vg2, vg3). Because
//     u = 0.5*(1 + tanh(beta*(x-0.5))/tanh(0.5*beta)) cancels, 1 ULP of tanh or beta
//     costs up to 21 ULP of the printed value (relative 3.15e-15), and at t=0 D's
//     non-symmetric tanh leaves a residual where glibc gives exactly zero: lines :653
//     (1.7554167342883506e-17 vs 0) and :662 (-1.1102230246251575e-15 vs 0).
//   * GaussianFunction at t=0.25 (2 lines, :515, :556) — its private erf calls exp.
// Everything else replays bit-identically: all toString text, all 24 guard messages and
// their Error/Exception taxonomy, all three loop trip counts, all distribution lengths
// and the remaining 1501 printed quantities.
// The gate below therefore separates (i) structure — message text, keys, lengths,
// iteration counts, NaN sign — which must be byte-identical with no tolerance, from
// (ii) real quantities, which must be bit-identical or within rel 1e-13 / abs 1e-14
// (wave-02-brief section 6's numeric criterion; 1e-14 is a 9x margin over the worst
// residual measured here, 1.1102230246251575e-15, and 1e-13 is 30x the worst relative
// one, 3.15e-15). A 1e-9 regression, a changed message, a flipped NaN sign or an
// off-by-one iteration count still fails. The counts stay in the summary line so the
// gap cannot be forgotten or quietly widened.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "geom/misc/univariatefunctions.hpp"
#include "nm/number.hpp"

using eil::geom::UnivariateFunction;
using eil::geom::LinearFunction;
using eil::geom::QuadraticFunction;
using eil::geom::RobertsFunction;
using eil::geom::GeometricFunction;
using eil::geom::GaussianFunction;
using eil::geom::GaussGeomHybridFunction;
using eil::geom::VinokurFunction;
using eil::geom::VinokurGeomHybridFunction;
using eil::geom::roberts_original;

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

/// The D probe's writefln, mirrored exactly: same format string, same argument order.
/// The no-argument overload exists because `eil::format(fmt)` with an empty argument
/// pack instantiates a path that warns under -Wall -Wextra (set-but-unused spec,
/// width, prec), and the probe's final line ("done=1") has no arguments.
inline void emitf(const std::string& fmt) { produced.push_back(fmt); }
template <typename... Args> void emitf(const std::string& fmt, const Args&... args) {
    produced.push_back(eil::format(fmt, args...));
}

/// The probe's TS sweep points (probe:60), spelled as D spells them.
const double TS[9] = {0.0, 0.05, 0.1, 0.25, 0.5, 0.75, 0.9, 0.95, 1.0};

long long ulp_gap(double a, double b) {
    int64_t ia, ib;
    std::memcpy(&ia, &a, 8);
    std::memcpy(&ib, &b, 8);
    if (ia < 0) ia = INT64_MIN - ia;
    if (ib < 0) ib = INT64_MIN - ib;
    return ia > ib ? ia - ib : ib - ia;
}

/// One `key=value` numeric field of an observation line.
struct Field {
    std::string key;  // the text between the preceding space and the '='
    double v;
};

/// Split a line into (a) a text skeleton with each numeric value replaced by '#', and
/// (b) the ordered numeric fields. A `=` whose tail does not parse as a whole number
/// keeps its text in the skeleton, so a message difference can never hide in a numeric
/// slot — that is what makes the structural half of the gate byte-exact.
void parse_obs(const std::string& s, std::string& skeleton, std::vector<Field>& fields) {
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] != '=') { skeleton += s[i++]; continue; }
        const size_t b = i + 1;
        size_t e = s.find(' ', b);
        if (e == std::string::npos) e = s.size();
        const std::string tok = s.substr(b, e - b);
        char* endp = nullptr;
        const double v = std::strtod(tok.c_str(), &endp);
        if (tok.empty() || endp != tok.c_str() + tok.size()) {
            skeleton += s[i++]; // not a number: keep the '=' as text and keep scanning
            continue;
        }
        size_t k0 = s.rfind(' ', i);
        k0 = (k0 == std::string::npos) ? 0 : k0 + 1;
        fields.push_back(Field{s.substr(k0, i - k0), v});
        skeleton.append(s, k0, i - k0);
        skeleton += "=";
        skeleton += '#';
        i = e;
    }
}

/// Fields that are counts, flags or lengths: exact always, tolerance never.
bool is_count_field(const std::string& key) {
    static const char* const counts[] = {"n",  "len",       "lam",    "gauss",   "xsw",
                                         "iters", "built",  "isError", "isException"};
    for (const char* c : counts) {
        if (key == c) return true;
    }
    return false;
}

/// Relative/absolute bound for a real quantity, per wave-02-brief section 6 and the
/// substrate-gap note in this header's comment.
bool within_transcendental_tolerance(double d, double c) {
    const double diff = std::fabs(d - c);
    const double scale = std::fmax(std::fabs(d), std::fabs(c));
    return diff <= 1.0e-14 || diff <= 1.0e-13 * scale;
}

/// probe:91-98 — one printed line per observation: the multi-line guard messages embed
/// '\n', so both sides escape them to the two-character sequence.
std::string esc(const std::string& s) {
    std::string res;
    for (char c : s) {
        if (c == '\n') res += "\\n";
        else res += c;
    }
    return res;
}

/// probe:62-64.
void sweep(const std::string& tag, const UnivariateFunction& f) {
    for (double t : TS) emitf("f %s t=%.17g y=%.17g", tag, t, f(t));
}

/// probe:66-70.
void dist(const std::string& tag, const UnivariateFunction& f, size_t n, double t0,
          double t1) {
    eil::Array<double> tv = f.distribute_parameter_values(n, t0, t1);
    emitf("dist %s n=%d t0=%.17g t1=%.17g len=%d", tag, n, t0, t1, tv.length());
    for (size_t i = 0; i < tv.length(); ++i) emitf("dist %s v[%d]=%.17g", tag, i, tv[i]);
}

/// probe:75-86 — build inside a try so the Error/Exception taxonomy and the message
/// text are observable. Error and Exception are siblings on the C++ side too, so the
/// two catch clauses are as exclusive as D's (wave-02-brief section 1, lesson 1).
template <typename F> void try_build(const std::string& tag, F build) {
    try {
        auto g = build();
        emitf("err %s built=1 isError=0 isException=0 y0=%.17g y1=%.17g", tag, (*g)(0.0),
              (*g)(1.0));
    } catch (const eil::ErrorBase& e) {
        emitf("err %s built=0 isError=1 isException=0 msg=[%s]", tag, esc(e.msg()));
    } catch (const eil::ExceptionBase& e) {
        emitf("err %s built=0 isError=0 isException=1 msg=[%s]", tag, esc(e.msg()));
    }
}

/// D's unittest helper (D:771-791), assertion for assertion. The `count` local D
/// writes but never reads is kept exactly as D has it; layer B needs that count, so
/// the body can hand it out through an optional pointer instead of the loop being
/// ported twice. Same tolerances (1e-6 relative interval, 1e-6 residual), same `&&`.
template <typename F> double bisect_iterate(F f, double p0, double p1,
                                            int* count_out = nullptr) {
    int count = 0;
    double f0 = f(p0);
    double f1 = f(p1);
    if (f0 * f1 > 0.0) {
        throw eil::ExceptionBase("Does not bracket an odd number of roots.", __FILE__,
                                 __LINE__);
    }
    double pmid = 0.5 * (p0 + p1);
    double fmid = f(pmid);
    do {
        if (f0 * fmid < 0.0) {
            p1 = pmid; f1 = fmid;
        } else {
            p0 = pmid; f0 = fmid;
        }
        pmid = 0.5 * (p0 + p1);
        fmid = f(pmid);
        count += 1; // D:788 — the original never reads this
    } while ((std::fabs(p1 - p0) / pmid > 1e-6) && (std::fabs(fmid) > 1e-6));
    if (count_out != nullptr) *count_out = count;
    return pmid;
}

// ============================================================================
// A. D's unittest block (D:769-833), ported in source order.
// ============================================================================
void port_d_unittest() {
    // D:793-812 test_gaussian_function_curvature: ensure the inflection point of the
    // gaussian cluster function is at m. Solve for the test point that gives m, then
    // check curvature == 0 and slope >= 0 with D's finite differences.
    auto test_gaussian_function_curvature = []() {
        double test_point, slope, curvature;
        double h = 1.0e-3; // Step in finite difference test
        for (double m : {0.0, 0.1, 0.5, 0.9, 1.0}) {
            auto cf = std::make_shared<GaussianFunction>(m, 0.1, 0.2);
            if ((m == 0.0) || (m == 1.0)) {
                // The cluster center must be at the edge
                test_point = m;
            } else {
                test_point = bisect_iterate(
                    [cf, m](double x) { return (*cf)(x) - m; }, 0.0, 1.0);
            }
            slope = ((*cf)(test_point + h) - (*cf)(test_point - h)) / (2 * h);
            curvature = ((*cf)(test_point + h) - 2 * (*cf)(test_point) +
                         (*cf)(test_point - h)) / (h * h);
            CHECK(slope >= 0.0, "D:809 gaussian curvature test slope >= 0");
            CHECK(std::fabs(curvature) <= 1.0e-3, "D:810 gaussian curvature |c| <= 1e-3");
        }
    };

    auto cf = std::make_shared<RobertsFunction>(false, true, 1.1);
    CHECK(eil::isClose((*cf)(0.1), 0.166167, 1.0e-4), "D:815 roberts f(0.1)");
    CHECK(eil::isClose((*cf)(0.9), 0.96657, 1.0e-4), "D:816 roberts f(0.9)");

    auto cf2 = std::make_shared<LinearFunction>(1.0, 0.0);
    CHECK(eil::isClose((*cf2)(0.1), 0.9, 1.0e-4), "D:819 linear f(0.1)");
    CHECK(eil::isClose((*cf2)(0.9), 0.1, 1.0e-4), "D:820 linear f(0.9)");

    auto cf3 = std::make_shared<GeometricFunction>(0.005, 1.1, 40, false);
    CHECK(eil::isClose((*cf3)(0.1), 0.0225106, 1.0e-4), "D:823 geometric f(0.1)");
    CHECK(eil::isClose((*cf3)(0.9), 0.85256413, 1.0e-4), "D:824 geometric f(0.9)");

    test_gaussian_function_curvature();

    auto cf4 = std::make_shared<GaussianFunction>(0.5, 0.1, 0.2);
    CHECK(eil::isClose((*cf4)(0.1), 0.1250750, 1.0e-4), "D:828 gaussian f(0.1)");
    CHECK(eil::isClose((*cf4)(0.9), 0.8749249, 1.0e-4), "D:829 gaussian f(0.9)");

    auto cf5 = std::make_shared<GaussGeomHybridFunction>(0.01, 1.2, 40, 0.8, 0.1, 0.2, false);
    CHECK(eil::isClose((*cf5)(0.1), 0.0518068, 1.0e-4), "D:832 gaussgeom f(0.1)");
    CHECK(eil::isClose((*cf5)(0.9), 0.8984721, 1.0e-4), "D:833 gaussgeom f(0.9)");
}

// ============================================================================
// B. the probe replay — same constructions, same order, same print statements as
//    probes/geom/misc/probe_univariatefunctions.d main().
// ============================================================================
void replay_probe() {
    // ---- base class (D:17) ------------------------------------------------
    auto base = std::make_shared<UnivariateFunction>();
    emitf("base.toString=%s", base->toString());
    sweep("base", *base);
    dist("base", *base, 5, -1.0, 3.0);

    // ---- LinearFunction (D:41) -------------------------------------------
    auto lin = std::make_shared<LinearFunction>();
    emitf("str LinearFunction.default=%s", lin->toString());
    sweep("lin.default", *lin);
    auto lin2 = std::make_shared<LinearFunction>(1.0, 0.0);
    emitf("str LinearFunction(1,0)=%s", lin2->toString());
    sweep("lin.1_0", *lin2);
    emitf("close lin.1_0 f0.1=%.17g f0.9=%.17g", (*lin2)(0.1), (*lin2)(0.9));
    auto lin3 = std::make_shared<LinearFunction>(2.5, -7.25);
    emitf("str LinearFunction(2.5,-7.25)=%s", lin3->toString());
    sweep("lin.2.5_-7.25", *lin3);
    auto lind = lin3->dup();
    emitf("dup LinearFunction y0=%.17g y1=%.17g same=%s", (*lind)(0.0), (*lind)(1.0),
          eil::to_string((*lind)(0.5) == (*lin3)(0.5)));
    dist("lin.default", *lin, 11, 0.0, 1.0);
    dist("lin.1_0", *lin2, 5, -1.0, 3.0);

    // ---- QuadraticFunction (D:69) ----------------------------------------
    for (double ratio : {2.0, 5.0, 1.5, 10.0}) {
        for (bool rev : {false, true}) {
            auto q = std::make_shared<QuadraticFunction>(ratio, rev);
            std::string tag = "quad.r" + eil::to_string(ratio) + (rev ? ".rev" : ".fwd");
            emitf("str %s=%s", tag, q->toString());
            sweep(tag, *q);
        }
    }
    auto q1 = std::make_shared<QuadraticFunction>(2.0, false);
    // The copy ctor (D:85-89) skips a and b, so dup() is all NaN. Preserved.
    auto qd = q1->dup();
    emitf("dup QuadraticFunction original=%.17g dup=%.17g dupIsNaN=%s dup0=%.17g",
          (*q1)(0.25), (*qd)(0.25), eil::to_string((*qd)(0.25) != (*qd)(0.25)), (*qd)(0.0));
    emitf("dup QuadraticFunction str=%s", qd->toString());
    dist("quad.r2", *q1, 9, 0.0, 1.0);

    // ---- RobertsFunction (D:110) and roberts_original (D:168) -------------
    const bool combos[4][2] = {{false, true}, {true, false}, {true, true}, {false, false}};
    for (const auto& combo : combos) {
        for (double beta : {1.1, 2.0, 0.9, 1.0}) {
            auto rf = std::make_shared<RobertsFunction>(combo[0], combo[1], beta);
            std::string tag = eil::format("roberts.e%de%d.b%g", static_cast<int>(combo[0]),
                                          static_cast<int>(combo[1]), beta);
            emitf("str %s=%s", tag, rf->toString());
            sweep(tag, *rf);
        }
    }
    auto runittest = std::make_shared<RobertsFunction>(false, true, 1.1);
    emitf("close roberts.unittest f0.1=%.17g f0.9=%.17g", (*runittest)(0.1),
          (*runittest)(0.9));
    auto rd = runittest->dup();
    emitf("dup RobertsFunction f0.1=%.17g same=%s", (*rd)(0.1),
          eil::to_string((*rd)(0.1) == (*runittest)(0.1)));
    {
        auto tmp = std::make_shared<RobertsFunction>(true, true, 1.1);
        dist("roberts.e0e1.b1.1", *tmp, 11, 0.0, 1.0);
    }

    // roberts_original directly, including singular beta=1 and beta<1 (negative base
    // to a fractional power -> NaN).
    for (double beta : {2.0, 1.1, 1.5}) {
        for (double alpha : {0.5, 0.0}) {
            std::string tag = eil::format("roberts_orig.a%g.b%g", alpha, beta);
            for (double t : TS)
                emitf("f %s t=%.17g y=%.17g", tag, t, roberts_original(t, alpha, beta));
        }
    }
    for (double t : TS) {
        emitf("f roberts_orig.beta1 t=%.17g y=%.17g", t, roberts_original(t, 0.5, 1.0));
        emitf("f roberts_orig.beta0.5 t=%.17g y=%.17g", t, roberts_original(t, 0.5, 0.5));
        emitf("f roberts_orig.beta0 t=%.17g y=%.17g", t, roberts_original(t, 0.5, 0.0));
    }

    // ---- GeometricFunction (D:183) ---------------------------------------
    auto geo = std::make_shared<GeometricFunction>(0.005, 1.1, 40, false);
    emitf("close geometric.unittest f0.1=%.17g f0.9=%.17g", (*geo)(0.1), (*geo)(0.9));
    sweep("geo.0.005_1.1_40", *geo);
    auto geo2 = std::make_shared<GeometricFunction>(0.005, 1.1, 40, true);
    sweep("geo.0.005_1.1_40.rev", *geo2);
    auto geo3 = std::make_shared<GeometricFunction>(0.005, 1.2, 40, false);
    sweep("geo.0.005_1.2_40", *geo3);
    auto geo4 = std::make_shared<GeometricFunction>(0.01, 1.1, 40, false);
    sweep("geo.0.01_1.1_40", *geo4);
    auto geo5 = std::make_shared<GeometricFunction>(0.005, 1.1, 61, false);
    sweep("geo.0.005_1.1_61", *geo5);
    auto geod = geo3->dup();
    emitf("dup GeometricFunction f0.5=%.17g same=%s", (*geod)(0.5),
          eil::to_string((*geod)(0.5) == (*geo3)(0.5)));
    dist("geo.0.005_1.1_40", *geo, 40, 0.0, 1.0);

    // ---- GaussianFunction (D:286) ----------------------------------------
    for (double m : {0.0, 0.1, 0.5, 0.9, 1.0}) {
        auto gf = std::make_shared<GaussianFunction>(m, 0.1, 0.2);
        std::string tag = eil::format("gauss.m%g", m);
        sweep(tag, *gf);
        // D's GaussianFunction has no toString of its own, so the base text is what
        // comes back; the corpus rows say "UnivariateFunction()".
        emitf("str %s=%s", tag, gf->toString());
    }
    auto g4 = std::make_shared<GaussianFunction>(0.5, 0.1, 0.2);
    emitf("close gaussian.unittest f0.1=%.17g f0.9=%.17g", (*g4)(0.1), (*g4)(0.9));
    {
        auto tmp = std::make_shared<GaussianFunction>(0.5, 0.3, 0.9);
        sweep("gauss.s0.3.r0.9", *tmp);
    }
    {
        auto tmp = std::make_shared<GaussianFunction>(0.5, 0.01, 0.01);
        sweep("gauss.s0.01.r0.01", *tmp);
    }
    auto gd = g4->dup();
    // GaussianFunction's copy ctor (D:324-331) omits b -> dup() is all NaN.
    emitf("dup GaussianFunction original=%.17g dup=%.17g dupIsNaN=%s", (*g4)(0.25),
          (*gd)(0.25), eil::to_string((*gd)(0.25) != (*gd)(0.25)));
    dist("gauss.m0.5", *g4, 11, 0.0, 1.0);

    // ---- GaussGeomHybridFunction (D:370) ---------------------------------
    auto gh = std::make_shared<GaussGeomHybridFunction>(0.01, 1.2, 40, 0.8, 0.1, 0.2, false);
    emitf("close gaussgeom.unittest f0.1=%.17g f0.9=%.17g", (*gh)(0.1), (*gh)(0.9));
    sweep("gaussgeom.0.01_1.2_40.m0.8", *gh);
    auto gh2 = std::make_shared<GaussGeomHybridFunction>(0.01, 1.2, 40, 0.5, 0.1, 0.2, false);
    sweep("gaussgeom.0.01_1.2_40.m0.5", *gh2);
    auto gh3 = std::make_shared<GaussGeomHybridFunction>(0.005, 1.1, 40, 0.3, 0.05, 0.3, false);
    sweep("gaussgeom.0.005_1.1_40.m0.3", *gh3);
    auto gh4 = std::make_shared<GaussGeomHybridFunction>(0.01, 1.2, 40, 0.8, 0.1, 0.2, true);
    sweep("gaussgeom.0.01_1.2_40.m0.8.rev", *gh4);
    auto ghd = gh->dup();
    // The copy ctor (D:409-419) omits a and c -> NaN again.
    emitf("dup GaussGeomHybrid f0.5=%.17g dup0.5=%.17g dupIsNaN=%s", (*gh)(0.5),
          (*ghd)(0.5), eil::to_string((*ghd)(0.5) != (*ghd)(0.5)));
    dist("gaussgeom.0.01_1.2_40", *gh, 21, 0.0, 1.0);

    // ---- VinokurFunction (D:557) -----------------------------------------
    auto vk = std::make_shared<VinokurFunction>(40, 0.01, 0.02);
    sweep("vinokur.n40.0.01.0.02", *vk);
    auto vk2 = std::make_shared<VinokurFunction>(100, 0.001, 0.01);
    sweep("vinokur.n100.0.001.0.01", *vk2);
    auto vk3 = std::make_shared<VinokurFunction>(50, 0.1, 0.001);
    sweep("vinokur.n50.0.1.0.001", *vk3);
    auto vkd = vk->dup();
    emitf("dup Vinokur f0.5=%.17g same=%s", (*vkd)(0.5),
          eil::to_string((*vkd)(0.5) == (*vk)(0.5)));
    dist("vinokur.n40", *vk, 41, 0.0, 1.0);

    // ---- VinokurGeomHybridFunction (D:643) -------------------------------
    auto vg = std::make_shared<VinokurGeomHybridFunction>(40, 0.01, 3, 1.2, 0.02, 4, 1.1);
    sweep("vinokurgeo.n40", *vg);
    auto vg2 = std::make_shared<VinokurGeomHybridFunction>(40, 0.01, 0, 1.0, 0.01, 0, 1.0);
    sweep("vinokurgeo.nolayers", *vg2);
    auto vg3 = std::make_shared<VinokurGeomHybridFunction>(60, 0.005, 5, 1.05, 0.005, 5, 1.05);
    sweep("vinokurgeo.sym", *vg3);
    auto vg4 = std::make_shared<VinokurGeomHybridFunction>(20, 0.02, 2, 1.5, 0.02, 3, 0.8);
    sweep("vinokurgeo.r1lt1", *vg4);
    auto vgd = vg->dup();
    emitf("dup VinokurGeomHybrid f0.5=%.17g same=%s", (*vgd)(0.5),
          eil::to_string((*vgd)(0.5) == (*vg)(0.5)));
    dist("vinokurgeo.n40", *vg, 41, 0.0, 1.0);

    // ---- the guard branches, plus the two that are not guards -------------
    try_build("geometric.a0", []() {
        return std::make_shared<GeometricFunction>(0.0, 1.1, 40, false);
    });
    try_build("geometric.aneg", []() {
        return std::make_shared<GeometricFunction>(-0.5, 1.1, 40, false);
    });
    try_build("geometric.r1", []() {
        return std::make_shared<GeometricFunction>(0.005, 1.0, 40, false);
    });
    try_build("geometric.r0.9", []() {
        return std::make_shared<GeometricFunction>(0.005, 0.9, 40, false);
    });
    // ns outside [1, N]: lambertW converges but the switch point is negative.
    try_build("geometric.ns_neg", []() {
        return std::make_shared<GeometricFunction>(0.05, 1.1, 40, false);
    });
    // lambertW itself timing out at the code's tol=1e-15 (D:253 Error).
    try_build("geometric.lambertW_timeout.21", []() {
        return std::make_shared<GeometricFunction>(0.005, 1.1, 21, false);
    });
    try_build("geometric.lambertW_timeout.a0.002", []() {
        return std::make_shared<GeometricFunction>(0.002, 1.1, 40, false);
    });
    try_build("geometric.lambertW_timeout.r1.01", []() {
        return std::make_shared<GeometricFunction>(0.005, 1.01, 40, false);
    });
    try_build("gaussian.mneg", []() {
        return std::make_shared<GaussianFunction>(-0.1, 0.1, 0.2);
    });
    try_build("gaussian.m2", []() {
        return std::make_shared<GaussianFunction>(2.0, 0.1, 0.2);
    });
    try_build("gaussian.ratio0", []() {
        return std::make_shared<GaussianFunction>(0.5, 0.1, 0.0);
    });
    try_build("gaussian.ratio2", []() {
        return std::make_shared<GaussianFunction>(0.5, 0.1, 2.0);
    });
    try_build("gaussgeom.A0", []() {
        return std::make_shared<GaussGeomHybridFunction>(0.0, 1.2, 40, 0.8, 0.1, 0.2, false);
    });
    try_build("gaussgeom.R1", []() {
        return std::make_shared<GaussGeomHybridFunction>(0.01, 1.0, 40, 0.8, 0.1, 0.2, false);
    });
    try_build("gaussgeom.m0", []() {
        return std::make_shared<GaussGeomHybridFunction>(0.01, 1.2, 40, 0.0, 0.1, 0.2, false);
    });
    try_build("gaussgeom.m1", []() {
        return std::make_shared<GaussGeomHybridFunction>(0.01, 1.2, 40, 1.0, 0.1, 0.2, false);
    });
    try_build("gaussgeom.ratio0", []() {
        return std::make_shared<GaussGeomHybridFunction>(0.01, 1.2, 40, 0.8, 0.1, 0.0, false);
    });
    try_build("gaussgeom.ratio1", []() {
        return std::make_shared<GaussGeomHybridFunction>(0.01, 1.2, 40, 0.8, 0.1, 1.0, false);
    });
    // solve_for_xswitch's Newton landing outside (0,1): Error at D:402.
    try_build("gaussgeom.xs_out", []() {
        return std::make_shared<GaussGeomHybridFunction>(0.02, 1.1, 21, 0.4, 0.15, 0.5, false);
    });
    try_build("vinokur.Blt1", []() {
        return std::make_shared<VinokurFunction>(10, 1.0, 1.0);
    });
    // B == 1 exactly is NOT guarded by `if (B<1.0)` (D:583): sinh(beta)/beta - 1 > 0
    // over the whole bracket, so brent cannot bracket a root and the catch at D:605
    // rethrows an Exception carrying brent's message.
    try_build("vinokur.Beq1", []() {
        return std::make_shared<VinokurFunction>(20, 0.05, 0.05);
    });
    try_build("vinokur.noroot", []() {
        return std::make_shared<VinokurFunction>(1, 1.0e-100, 1.0e-100);
    });
    // Multi-line Error messages (D:678-682 and :688-692 append three more lines).
    try_build("vinokurgeo.vnlt1", []() {
        return std::make_shared<VinokurGeomHybridFunction>(4, 0.01, 2, 1.2, 0.02, 2, 1.1);
    });
    try_build("vinokurgeo.Blt1", []() {
        return std::make_shared<VinokurGeomHybridFunction>(21, 0.02, 0, 1.0, 0.03, 5, 1.3);
    });

    // ---- distribute_parameter_values edge cases (D:27-38) ----------------
    // dt = 1.0/(n-1) with n a size_t: n=1 divides by zero (inf), n=0 wraps.
    {
        auto f1 = std::make_shared<LinearFunction>(0.0, 1.0);
        dist("linear.n1", *f1, 1, 0.0, 1.0);
    }
    {
        auto f2 = std::make_shared<LinearFunction>(0.0, 1.0);
        dist("linear.n2", *f2, 2, 0.0, 1.0);
    }
    {
        auto f0 = std::make_shared<LinearFunction>(0.0, 1.0);
        dist("linear.n0", *f0, 0, 0.0, 1.0);
    }
    {
        auto fr = std::make_shared<LinearFunction>(1.0, 0.0);
        dist("linear.reversed.n6", *fr, 6, 10.0, 20.0);
    }

    // ---- iteration counts (added by univariatefunctions_copy.d) ----------
    // D exposes no counter, so the copy writes the loop index into a module global;
    // the port keeps the same counts per object (see the header note in
    // src/geom/misc/univariatefunctions.hpp), reset by construction.
    {
        auto gi = std::make_shared<GeometricFunction>(0.005, 1.1, 40, false);
        emitf("iter GeometricFunction lam=%d f0.1=%.17g f0.9=%.17g",
              gi->lambertW_iterations, (*gi)(0.1), (*gi)(0.9));
    }
    {
        auto gi2 = std::make_shared<GeometricFunction>(0.005, 1.2, 40, false);
        emitf("iter GeometricFunction.r1.2 lam=%d f0.5=%.17g", gi2->lambertW_iterations,
              (*gi2)(0.5));
    }
    {
        auto gi3 = std::make_shared<GeometricFunction>(0.005, 1.1, 61, false);
        emitf("iter GeometricFunction.n61 lam=%d f0.5=%.17g", gi3->lambertW_iterations,
              (*gi3)(0.5));
    }

    for (double m : {0.0, 0.1, 0.5, 0.9, 1.0}) {
        auto gfi = std::make_shared<GaussianFunction>(m, 0.1, 0.2);
        emitf("iter GaussianFunction m=%g gauss=%d f0.5=%.17g errNotConverged=%s", m,
              gfi->centre_iterations, (*gfi)(0.5),
              eil::to_string(gfi->centre_iterations == 100));
    }
    {
        auto g5 = std::make_shared<GaussianFunction>(0.5, 0.3, 0.9);
        emitf("iter GaussianFunction s0.3.r0.9 gauss=%d f0.5=%.17g", g5->centre_iterations,
              (*g5)(0.5));
    }
    {
        auto g6 = std::make_shared<GaussianFunction>(0.5, 0.01, 0.01);
        emitf("iter GaussianFunction s0.01.r0.01 gauss=%d f0.5=%.17g", g6->centre_iterations,
              (*g6)(0.5));
    }

    const double ghc[4][7] = {{0.01, 1.2, 40, 0.8, 0.1, 0.2, 0.0},
                              {0.01, 1.2, 40, 0.5, 0.1, 0.2, 0.0},
                              {0.005, 1.1, 40, 0.3, 0.05, 0.3, 0.0},
                              {0.01, 1.2, 40, 0.8, 0.1, 0.2, 1.0}};
    for (const auto& c : ghc) {
        bool rev = (c[6] != 0.0);
        auto ghi = std::make_shared<GaussGeomHybridFunction>(c[0], c[1], static_cast<int>(c[2]),
                                                             c[3], c[4], c[5], rev);
        emitf("iter GaussGeomHybrid A=%g R=%g n=%d m=%g s=%g ratio=%g rev=%s xsw=%d f0.5=%.17g",
              c[0], c[1], static_cast<int>(c[2]), c[3], c[4], c[5], rev,
              ghi->xswitch_iterations, (*ghi)(0.5));
    }

    // The two Vinokur classes iterate inside brent.solve (whose own iteration count is
    // gated by ctest:nm_brent), so only the converged root shows up here.
    {
        auto vki = std::make_shared<VinokurFunction>(40, 0.01, 0.02);
        emitf("vinokur.copy n40 f0=%.17g f0.5=%.17g f1=%.17g", (*vki)(0.0), (*vki)(0.5),
              (*vki)(1.0));
    }
    {
        auto vgi = std::make_shared<VinokurGeomHybridFunction>(40, 0.01, 3, 1.2, 0.02, 4, 1.1);
        emitf("vinokurgeo.copy n40 f0=%.17g f0.5=%.17g f1=%.17g", (*vgi)(0.0), (*vgi)(0.5),
              (*vgi)(1.0));
    }

    // ---- the unittest's bisection inverse, with its iteration count -------
    // Mirrors test_gaussian_function_curvature (D:793-812): for each m, invert the
    // Gaussian cluster function by bisection, then measure slope and curvature there
    // with the finite differences D uses (h = 1.0e-3).
    for (double m : {0.0, 0.1, 0.5, 0.9, 1.0}) {
        auto cf = std::make_shared<GaussianFunction>(m, 0.1, 0.2);
        double test_point, slope, curvature;
        double h = 1.0e-3;
        int iters = 0;
        if ((m == 0.0) || (m == 1.0)) {
            test_point = m;
            emitf("inverse m=%g path=endpoint test=%.17g iters=-1", m, test_point);
        } else {
            test_point = bisect_iterate(
                [cf, m](double x) { return (*cf)(x) - m; }, 0.0, 1.0, &iters);
            emitf("inverse m=%g path=bisect test=%.17g iters=%d resid=%.17g", m, test_point,
                  iters, std::fabs((*cf)(test_point) - m));
        }
        slope = ((*cf)(test_point + h) - (*cf)(test_point - h)) / (2 * h);
        curvature = ((*cf)(test_point + h) - 2 * (*cf)(test_point) +
                     (*cf)(test_point - h)) / (h * h);
        emitf("inverse m=%g slope=%.17g curvature=%.17g", m, slope, curvature);
    }
    // The bracket failure path of the same helper (D:776).
    {
        int iters = 0;
        try {
            auto cf = std::make_shared<GaussianFunction>(0.5, 0.1, 0.2);
            // f(x) - 0.5 on [0.4, 0.6] has both endpoints on the same side of zero.
            double x = bisect_iterate([cf](double t) { return (*cf)(t) + 1.0; }, 0.0, 1.0,
                                      &iters);
            emitf("inverse bracket built=1 x=%.17g", x);
        } catch (const eil::ExceptionBase& e) {
            emitf("inverse bracket built=0 isError=0 isException=1 msg=[%s] iters=%d",
                  esc(e.msg()), iters);
        } catch (const eil::ErrorBase& e) {
            emitf("inverse bracket built=0 isError=1 isException=0 msg=[%s] iters=%d",
                  esc(e.msg()), iters);
        }
    }

    emitf("done=1");
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0]
                  << " <probes/geom_misc_univariatefunctions.txt>\n";
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

    size_t byte_identical = 0, tol_lines = 0, unexplained = 0;
    size_t quantities = 0, identical = 0, moved = 0;
    long long worst_ulp = 0;
    std::string worst_ulp_where;
    double worst_abs = 0.0, worst_abs_rel = 0.0;
    double worst_d = 0.0, worst_cpp = 0.0;
    std::string worst_abs_where;

    for (size_t i = 0; i < expected.size() || i < produced.size(); ++i) {
        const bool both = i < expected.size() && i < produced.size();
        std::string dskel, cskel;
        std::vector<Field> df, cf;
        if (both) {
            parse_obs(expected[i], dskel, df);
            parse_obs(produced[i], cskel, cf);
        }
        const bool exact = both && expected[i] == produced[i];

        // Classify a differing line: it is tolerated only if the TEXT skeleton is
        // identical, the fields are the same keys in the same order, and every field is
        // either bit-equal, or a non-count finite quantity inside the transcendental
        // tolerance. Message text, iteration counts, lengths and NaN signs can never
        // be tolerated.
        bool tol_ok = false;
        if (!exact && both && dskel == cskel && df.size() == cf.size()) {
            tol_ok = true;
            for (size_t j = 0; j < df.size(); ++j) {
                const double a = df[j].v, b = cf[j].v;
                if (df[j].key != cf[j].key) { tol_ok = false; break; }
                if (std::memcmp(&a, &b, 8) == 0) continue;
                if (is_count_field(df[j].key) || !std::isfinite(a) || !std::isfinite(b) ||
                    !within_transcendental_tolerance(a, b)) {
                    tol_ok = false;
                    break;
                }
            }
        }

        if (both) {
            for (size_t j = 0; j < df.size() && j < cf.size(); ++j) {
                ++quantities;
                const double a = df[j].v, b = cf[j].v;
                if (std::memcmp(&a, &b, 8) == 0) { ++identical; continue; }
                ++moved;
                const std::string tag = expected[i].substr(0, expected[i].find(' '));
                const std::string where = tag + " " + df[j].key;
                const long long gap = ulp_gap(a, b);
                const double ad = std::fabs(a - b);
                const double scale = std::fmax(std::fabs(a), std::fabs(b));
                if (ad > worst_abs) {
                    worst_abs = ad;
                    worst_abs_rel = (scale > 0.0) ? ad / scale : 1.0;
                    worst_d = a;
                    worst_cpp = b;
                    worst_abs_where = where;
                }
                // A ULP distance only means something when neither side is zero.
                if (a != 0.0 && b != 0.0 && std::isfinite(a) && std::isfinite(b) &&
                    gap > worst_ulp) {
                    worst_ulp = gap;
                    worst_ulp_where = where;
                }
            }
        }

        if (exact) {
            ++byte_identical;
        } else if (tol_ok) {
            ++tol_lines;
        } else {
            if (unexplained < 20) {
                std::cout << "line " << (i + 1) << "\n  D:   |"
                          << (i < expected.size() ? expected[i] : std::string("<none>"))
                          << "|\n  C++: |"
                          << (i < produced.size() ? produced[i] : std::string("<none>")) << "|\n";
            }
            ++unexplained;
        }
    }

    const bool complete =
        byte_identical + tol_lines == expected.size() && expected.size() == produced.size();
    std::cout << ((unexplained == 0 && failures == 0 && complete) ? "PASS" : "FAIL")
              << ": univariatefunctions, " << checks - failures << "/" << checks
              << " unittest assertions hold (univariatefunctions.d:769-833, including "
                 "test_gaussian_function_curvature); probe replay "
              << expected.size() << " D lines vs " << produced.size() << " C++ lines: "
              << byte_identical << " byte-identical, " << tol_lines
              << " differ only inside the Phobos-vs-glibc transcendental tolerance "
                 "(exp/tanh/sinh, see the header note), "
              << unexplained << " unexplained; " << identical << " of " << quantities
              << " printed quantities bit-identical, " << moved << " differ";
    if (moved > 0) {
        std::cout << "; worst absolute residual " << worst_abs << " on " << worst_abs_where
                  << " (D " << worst_d << " vs C++ " << worst_cpp << ", relative "
                  << worst_abs_rel << ")";
        if (worst_ulp > 0) {
            std::cout << "; worst ULP gap between non-zero values " << worst_ulp << " on "
                      << worst_ulp_where;
        }
    }
    std::cout << "\n";
    return (unexplained == 0 && failures == 0 && complete) ? 0 : 1;
}
