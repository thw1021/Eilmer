// tests/test_nm_nelmin.cpp — the three unittest blocks from nm/nelmin.d:328-433,
// plus a line-for-line replay of probes/nm/probe_nelmin.d against
// baselines/e587b235/probes/nm_nelmin.txt, and of the text D's unittest blocks
// print against baselines/e587b235/probes/nm_nelmin_unittest.txt.
//
// Why the counters are part of the corpus: nelmin drives the grid-generation
// smoothing in geom/grid/sgrid.d:1411 (tol=1.0e-3, P=3, max_fe=800) and the
// point-to-path distances in geom/gpath/gpath_utils.d:153. An answer that looks
// the same but was reached with a different number of evaluations means the
// reflect/extend/contract/rescale steps took a different path, so every case here
// prints the converged point, f_min, n_fe and n_restart, not just the point.
//
// The D unittests assert nfe in (425,435) with 428 in the comment, nfe in (80,90)
// with 86, nfe in (500,505) with 503, and nres < 6 (5), nres == 0, nres == 0
// (nelmin.d:357-358, :392-393, :430-431). Those bounds are ported as assertions;
// the exact counts are pinned by the corpus comparison.
//
// The probe cases also cover branches the unittests do not reach: the default tol
// (nelmin.d:80), the max_fe cut-off with a non-converged exit, n_check=5,
// delta=0.01, P=3 (sgrid's setting), a far start where the reflection beats the
// best vertex so the extension branch fires, a ridge that forces restarts, a flat
// function whose vertex values are all ties (this is the case that pins the sort:
// std::sort must hand back the permutation D's introsort did), explicit
// Kreflect/Kextend/Kcontract (nelmin.d:81), and a dx whose length does not match
// x, which nelmin.d:88-91 silently repairs to 0.1 per coordinate.
//
// Run: ./test_nm_nelmin <probes/nm_nelmin.txt> <probes/nm_nelmin_unittest.txt>

#include <algorithm>
#include <cstdarg>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "eil/array.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/math.hpp"
#include "nm/nelmin.hpp"
#include "nm/number.hpp"

namespace {

using number = eil::nm::number;
using Objective = double (*)(eil::Slice<number>);

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    } else {
        std::printf("ok: %s\n", what);
    }
}

std::vector<std::string> produced;

std::string fmt(const char* f, ...) {
    char buf[8192];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return std::string(buf);
}

void emit(const std::string& s) { produced.push_back(s); }

// ---- objective functions, copied from nelmin.d's unittest blocks -----------

// nelmin.d:335-340
number testFunction1(eil::Slice<number> x) {
    number sum = 0.0;
    for (size_t i = 0; i < x.length(); ++i)
        sum += (x[i] - 1.0) * (x[i] - 1.0);
    return sum;
}

// nelmin.d:366-378, Example 3.3 from Olsson and Nelson.
number testFunction2(eil::Slice<number> x) {
    number x1 = x[0];
    number x2 = x[1];   // rename to match the paper
    if ((x1 * x1 + x2 * x2) > 1.0) {
        return number(1.0e38);
    } else {
        number yp = 53.69 + 7.26 * x1 - 10.33 * x2 + 7.22 * x1 * x1
            + 6.43 * x2 * x2 + 11.36 * x1 * x2;
        number ys = 82.17 - 1.01 * x1 - 8.61 * x2 + 1.40 * x1 * x1
            - 8.76 * x2 * x2 - 7.20 * x1 * x2;
        return -yp + std::fabs(ys - 87.8);
    }
}

// nelmin.d:401-416, Example 3.5 from Olsson and Nelson; least-squares. The D
// unittest declares this one over double rather than number; T is double there.
double testFunction3(eil::Slice<double> z) {
    eil::Array<double> x({0.25, 0.50, 1.00, 1.70, 2.00, 4.00});
    eil::Array<double> y({0.25, 0.40, 0.60, 0.58, 0.54, 0.27});
    double a1 = z[0];
    double a2 = z[1];
    double alpha1 = z[2];
    double alpha2 = z[3];
    double sum_residuals = 0.0;
    for (int i = 0; i < 6; ++i) {
        double t = x[i];
        // std.math.exp; the substrate has no eil::exp wrapper (its NaN
        // canonicalisation only matters for the domain-error functions), and both
        // languages call the same glibc exp.
        double eta = a1 * std::exp(alpha1 * t) + a2 * std::exp(alpha2 * t);
        double r = y[i] - eta;
        sum_residuals += r * r;
    }
    return sum_residuals;
}

// nelmin_demo.d:50-67, the same body over number (used by the demo's test 3).
number testFunction3n(eil::Slice<number> z) {
    eil::Array<double> x({0.25, 0.50, 1.00, 1.70, 2.00, 4.00});
    eil::Array<double> y({0.25, 0.40, 0.60, 0.58, 0.54, 0.27});
    number a1 = z[0]; number a2 = z[1];
    number alpha1 = z[2]; number alpha2 = z[3];
    number sum_residuals = 0.0;
    for (int i = 0; i < 6; ++i) {
        number t = x[i];
        number eta = a1 * std::exp(alpha1 * t) + a2 * std::exp(alpha2 * t);
        number r = y[i] - eta;
        sum_residuals += r * r;
    }
    return sum_residuals;
}

number rosenbrock(eil::Slice<number> x) {
    number sum = 0.0;
    for (size_t i = 0; i + 1 < x.length(); ++i) {
        number a = x[i+1] - x[i]*x[i];
        number b = 1.0 - x[i];
        sum += 100.0*a*a + b*b;
    }
    return sum;
}

number ridged(eil::Slice<number> x) {
    number sum = 0.0;
    for (size_t i = 0; i < x.length(); ++i) sum += std::fabs(x[i]);
    return sum;
}

number flat(eil::Slice<number> x) { (void)x; return 0.0; }

void dump(const char* tag, bool conv, const eil::Array<number>& x, number fx, int nfe,
          int nres) {
    std::string s = fmt("%s conv=%s nfe=%d nres=%d fx=%.17g x=[", tag,
                        conv ? "true" : "false", nfe, nres, fx);
    for (size_t i = 0; i < x.length(); ++i) {
        if (i) s += ",";
        s += fmt("%.17g", x[i]);
    }
    s += "]";
    emit(s);
}

void dump_d(const char* tag, bool conv, const eil::Array<double>& x, double fx, int nfe,
            int nres) {
    std::string s = fmt("%s conv=%s nfe=%d nres=%d fx=%.17g x=[", tag,
                        conv ? "true" : "false", nfe, nres, fx);
    for (size_t i = 0; i < x.length(); ++i) {
        if (i) s += ",";
        s += fmt("%.17g", x[i]);
    }
    s += "]";
    emit(s);
}

// ---- the ported unittest blocks -------------------------------------------

void unittest_1() {
    // nelmin.d:342-358
    eil::Array<number> x({number(0.0), number(0.0), number(0.0), number(0.0)});
    eil::Array<number> dx;
    number fx;
    int nfe, nres;
    bool conv_flag = eil::nm::nelmin::minimize<Objective, number>(
        testFunction1, x, fx, nfe, nres, dx.all());
    check(conv_flag, "nelmin.d:351 unittest 1 converged");
    check(eil::nm::approxEqualNumbers(x.all(),
          eil::Array<number>({number(1.0), number(1.0), number(1.0), number(1.0)}).all()),
          "nelmin.d:352-355 unittest 1 x ~= (1,1,1,1)");
    check(eil::nm::approxEqualNumbers(fx, number(0.0)),
          "nelmin.d:356 unittest 1 fx ~= 0");
    check((nfe > 425) && (nfe < 435), "nelmin.d:357 unittest 1 425 < nfe < 435");
    check(nres < 6, "nelmin.d:358 unittest 1 nres < 6");
    check(nfe == 428 && nres == 5, "nelmin.d:357-358 comments: nfe == 428, nres == 5");
}

void unittest_2() {
    // nelmin.d:380-393
    number fx;
    int nfe, nres;
    eil::Array<number> x2({number(0.0), number(0.0)});
    eil::Array<number> dx2({number(0.5), number(0.5)});
    bool conv_flag = eil::nm::nelmin::minimize<Objective, number>(
        testFunction2, x2, fx, nfe, nres, dx2.all(), 1.0e-4);
    // The four writeln lines of the block, in D's wording. Note that nelmin.d:388
    // writes "number-of-restarts =" with no space before the value.
    emit("x = " + eil::to_string(x2) + " fx = " + eil::to_string(fx));
    emit("convergence-flag = " + eil::to_string(conv_flag));
    emit("number-of-fn-evaluations = " + eil::to_string(nfe));
    emit("number-of-restarts =" + eil::to_string(nres));
    check(conv_flag, "nelmin.d:389 unittest 2 converged");
    check(eil::nm::approxEqualNumbers(x2.all(),
          eil::Array<number>({number(0.811295), number(-0.584636)}).all()),
          "nelmin.d:390 unittest 2 x2 ~= (0.811295, -0.584636)");
    check(eil::nm::approxEqualNumbers(fx, number(-67.1077)),
          "nelmin.d:391 unittest 2 fx ~= -67.1077");
    check((nfe > 80) && (nfe < 90), "nelmin.d:392 unittest 2 80 < nfe < 90");
    check(nres == 0, "nelmin.d:393 unittest 2 nres == 0");
    check(nfe == 86, "nelmin.d:392 comment: nfe == 86");
}

void unittest_3() {
    // nelmin.d:418-431
    int nfe, nres;
    eil::Array<double> x3({1.0, 1.0, -0.5, -2.5});
    eil::Array<double> dx3({0.1, 0.1, 0.1, 0.1});
    double fx3;
    bool conv_flag = eil::nm::nelmin::minimize<double (*)(eil::Slice<double>), double>(
        testFunction3, x3, fx3, nfe, nres, dx3.all(), 1.0e-9, 2, 800);
    emit("x = " + eil::to_string(x3) + " fx = " + eil::to_string(fx3));
    emit("convergence-flag = " + eil::to_string(conv_flag));
    emit("number-of-fn-evaluations = " + eil::to_string(nfe));
    emit("number-of-restarts = " + eil::to_string(nres));
    check(conv_flag, "nelmin.d:427 unittest 3 converged");
    check(eil::nm::approxEqualNumbers(x3.all(), eil::Array<double>(
          {1.80105, -1.84177, -0.463388, -1.20508}).all()),
          "nelmin.d:428 unittest 3 x3 ~= (1.80105, -1.84177, -0.463388, -1.20508)");
    check(eil::nm::approxEqualNumbers(fx3, 0.000908953),
          "nelmin.d:429 unittest 3 fx3 ~= 0.000908953");
    check((nfe > 500) && (nfe < 505), "nelmin.d:430 unittest 3 500 < nfe < 505");
    check(nres == 0, "nelmin.d:431 unittest 3 nres == 0");
    check(nfe == 503, "nelmin.d:430 comment: nfe == 503");
}

// ---- the probe replay ------------------------------------------------------

void replay_probe() {
    number fx;
    int nfe, nres;
    bool conv;

    eil::Array<number> dx1;   // empty, so minimize() substitutes 0.1 per element

    eil::Array<number> x1({number(0.0), number(0.0), number(0.0), number(0.0)});
    conv = eil::nm::nelmin::minimize<Objective, number>(testFunction1, x1, fx, nfe, nres,
                                                        dx1.all());
    dump("unittest1", conv, x1, fx, nfe, nres);

    eil::Array<number> x2({number(0.0), number(0.0)});
    eil::Array<number> dx2({number(0.5), number(0.5)});
    conv = eil::nm::nelmin::minimize<Objective, number>(testFunction2, x2, fx, nfe, nres,
                                                        dx2.all(), 1.0e-4);
    dump("unittest2", conv, x2, fx, nfe, nres);

    eil::Array<double> x3({1.0, 1.0, -0.5, -2.5});
    eil::Array<double> dx3({0.1, 0.1, 0.1, 0.1});
    double fx3;
    conv = eil::nm::nelmin::minimize<double (*)(eil::Slice<double>), double>(
        testFunction3, x3, fx3, nfe, nres, dx3.all(), 1.0e-9, 2, 800);
    dump_d("unittest3", conv, x3, fx3, nfe, nres);

    eil::Array<number> x3d({number(1.0), number(1.0), number(-0.5), number(-2.5)});
    eil::Array<number> dx3d({number(0.1), number(0.1), number(0.1), number(0.1)});
    conv = eil::nm::nelmin::minimize<Objective, number>(testFunction3n, x3d, fx, nfe, nres,
                                                        dx3d.all(), 1.0e-9, 1, 800);
    dump("demo3_P1", conv, x3d, fx, nfe, nres);

    eil::Array<number> x4({number(0.0), number(0.0)});
    conv = eil::nm::nelmin::minimize<Objective, number>(testFunction2, x4, fx, nfe, nres,
                                                        dx2.all());
    dump("tf2_default_tol", conv, x4, fx, nfe, nres);

    eil::Array<number> x5({number(0.0), number(0.0), number(0.0), number(0.0)});
    conv = eil::nm::nelmin::minimize<Objective, number>(testFunction1, x5, fx, nfe, nres,
                                                        dx1.all(), 1.0e-6, 1, 200);
    dump("tf1_maxfe200", conv, x5, fx, nfe, nres);

    eil::Array<number> x6({number(0.0), number(0.0)});
    conv = eil::nm::nelmin::minimize<Objective, number>(testFunction2, x6, fx, nfe, nres,
                                                        dx2.all(), 1.0e-4, 1, 400, 5);
    dump("tf2_ncheck5", conv, x6, fx, nfe, nres);

    eil::Array<number> x7({number(0.0), number(0.0), number(0.0), number(0.0)});
    conv = eil::nm::nelmin::minimize<Objective, number>(testFunction1, x7, fx, nfe, nres,
                                                        dx1.all(), 1.0e-6, 1, 400, 20, 0.01);
    dump("tf1_delta001", conv, x7, fx, nfe, nres);

    eil::Array<number> x8({number(0.0), number(0.0), number(0.0), number(0.0),
                           number(0.0), number(0.0)});
    eil::Array<number> dx8;
    for (int i = 0; i < 6; ++i) dx8.append(0.05);
    conv = eil::nm::nelmin::minimize<Objective, number>(testFunction1, x8, fx, nfe, nres,
                                                        dx8.all(), 1.0e-3, 3, 800);
    dump("tf1_P3", conv, x8, fx, nfe, nres);

    eil::Array<number> x9({number(-3.0), number(-3.0)});
    eil::Array<number> dx9({number(1.0), number(1.0)});
    conv = eil::nm::nelmin::minimize<Objective, number>(testFunction1, x9, fx, nfe, nres,
                                                        dx9.all(), 1.0e-8, 1, 400);
    dump("tf1_far_start", conv, x9, fx, nfe, nres);

    eil::Array<number> x10({number(-1.2), number(1.0), number(0.5)});
    eil::Array<number> dx10({number(0.5), number(0.5), number(0.5)});
    conv = eil::nm::nelmin::minimize<Objective, number>(rosenbrock, x10, fx, nfe, nres,
                                                        dx10.all(), 1.0e-6, 1, 400);
    dump("rosenbrock3d", conv, x10, fx, nfe, nres);

    eil::Array<number> x11({number(1.0), number(-1.0)});
    eil::Array<number> dx11({number(0.5), number(0.5)});
    conv = eil::nm::nelmin::minimize<Objective, number>(ridged, x11, fx, nfe, nres,
                                                        dx11.all(), 1.0e-6, 1, 400);
    dump("ridged", conv, x11, fx, nfe, nres);

    eil::Array<number> x12({number(0.3), number(-0.4), number(0.0)});
    eil::Array<number> dx12({number(0.1), number(0.1), number(0.1)});
    conv = eil::nm::nelmin::minimize<Objective, number>(flat, x12, fx, nfe, nres,
                                                        dx12.all(), 1.0e-6, 1, 120);
    dump("flat_ties", conv, x12, fx, nfe, nres);

    eil::Array<number> x13({number(-2.0), number(1.5), number(0.25)});
    eil::Array<number> dx13({number(0.4), number(0.4), number(0.4)});
    conv = eil::nm::nelmin::minimize<Objective, number>(rosenbrock, x13, fx, nfe, nres,
                                                        dx13.all(), 1.0e-6, 1, 400, 20,
                                                        0.001, 1.0, 2.0, 0.5);
    dump("rosenbrock_defaultK", conv, x13, fx, nfe, nres);

    eil::Array<number> x14({number(-2.0), number(1.5), number(0.25)});
    conv = eil::nm::nelmin::minimize<Objective, number>(rosenbrock, x14, fx, nfe, nres,
                                                        dx13.all(), 1.0e-6, 1, 400, 20,
                                                        0.001, 0.8, 1.5, 0.4);
    dump("rosenbrock_customK", conv, x14, fx, nfe, nres);

    eil::Array<number> x15({number(0.5), number(0.5), number(0.5)});
    eil::Array<number> dx15({number(0.2), number(0.2)});
    conv = eil::nm::nelmin::minimize<Objective, number>(testFunction1, x15, fx, nfe, nres,
                                                        dx15.all(), 1.0e-8);
    dump("tf1_short_dx", conv, x15, fx, nfe, nres);
}

// ---- corpus diff ----------------------------------------------------------

// Every field must match exactly, with one auditable exception: the `fx=` value.
//
// Why fx is special-cased, measured rather than assumed: the search itself is
// identical (same conv flag, same nfe, same nres, same x= list to the last bit),
// but two of the six exp() calls inside the objective differ by 1 ULP between
// Phobos and glibc. At the converged point, exp(-1.2050774716664778*0.5) is
// 0.54742011542684965 in D and 0.54742011542684954 here, and
// exp(-0.46338762974692316*2.0) is ...64002 vs ...63997. r = y - eta cancels, so
// r*r amplifies 1 ULP of exp into ~8.3e-15 relative in the sum, which is still an
// order of magnitude inside the project's 1e-13 field gate. This is the same
// Phobos/glibc transcendental split recorded in reports/phase-0.md; demanding
// byte equality here would be demanding that glibc's exp round like Phobos'.
//
// The tolerance is applied only to a field that starts with "fx=" and only when
// both sides parse as finite doubles; a tolerated line is counted and named, so
// the exception stays visible instead of quietly widening.
long diff_against(const char* path) {
    std::ifstream in(path);
    if (!in) {
        std::printf("cannot open corpus %s\n", path);
        return -1;
    }
    std::vector<std::string> expected;
    for (std::string line; std::getline(in, line);) expected.push_back(line);

    long diffs = 0;
    long tolerated_fields = 0;
    size_t n = std::max(expected.size(), produced.size());
    for (size_t i = 0; i < n; ++i) {
        if (i >= expected.size() || i >= produced.size()) {
            ++diffs;
            continue;
        }
        std::vector<std::string> a, b;
        for (const std::string* src : {&expected[i], &produced[i]}) {
            std::istringstream ss(*src);
            std::string tok;
            while (ss >> tok) {
                (src == &expected[i] ? a : b).push_back(tok);
            }
        }
        bool line_ok = a.size() == b.size();
        bool line_tol = false;
        for (size_t k = 0; line_ok && k < a.size(); ++k) {
            if (a[k] == b[k]) continue;
            const std::string pref = "fx=";
            if (a[k].compare(0, pref.size(), pref) == 0 &&
                b[k].compare(0, pref.size(), pref) == 0) {
                double va = std::strtod(a[k].c_str() + pref.size(), nullptr);
                double vb = std::strtod(b[k].c_str() + pref.size(), nullptr);
                if (std::isfinite(va) && std::isfinite(vb) && eil::isClose(va, vb, 1.0e-13)) {
                    line_tol = true;
                    ++tolerated_fields;
                    continue;
                }
            }
            line_ok = false;
        }
        if (!line_ok || line_tol) {
            if (diffs < 20) {
                std::printf("line %zu (%s)\n  D:  |%s|\n  C++:|%s|\n", i + 1,
                            line_tol ? "fx within 1e-13, exp 1 ULP" : "DIFFERS",
                            expected[i].c_str(), produced[i].c_str());
            }
            if (!line_tol) ++diffs;
        }
    }
    std::printf("  %s: %zu D lines vs %zu C++ lines, %ld differ, %ld fx field(s) within "
                "1e-13 (Phobos/glibc exp)\n",
                path, expected.size(), produced.size(), diffs, tolerated_fields);
    return diffs;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::printf("usage: %s <probes/nm_nelmin.txt> <probes/nm_nelmin_unittest.txt>\n",
                    argv[0]);
        return 2;
    }
    // The three unittest blocks first: their assertions are checked and the eight
    // lines nelmin.d:385-388 and :423-426 print land in `produced`.
    unittest_1();
    unittest_2();
    unittest_3();
    const long n_unittest_lines = static_cast<long>(produced.size());
    const long d2 = diff_against(argv[2]);
    produced.clear();

    replay_probe();
    const long n_probe_lines = static_cast<long>(produced.size());
    const long d1 = diff_against(argv[1]);

    const bool ok = (d1 == 0) && (d2 == 0) && (failures == 0);
    std::printf("%s: nm.nelmin, %ld probe lines and %ld unittest-printed lines compared, "
                "%ld differ, %d assertion failures\n",
                ok ? "PASS" : "FAIL", n_probe_lines, n_unittest_lines, d1 + d2, failures);
    return ok ? 0 : 1;
}
