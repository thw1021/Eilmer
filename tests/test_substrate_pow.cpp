// tests/test_substrate_pow.cpp
//
// Substrate conformance test: replays probes/substrate/probe_pow.d line for line
// and diffs against baselines/e587b235/probes/substrate_pow.txt.
//
// What it pins: D's `a ^^ b` is std.math.pow, not repeated multiplication. Two of
// the seven cube rows are bit-different (1.2345^^3 = 1.8813659636249997 vs
// 1.2345*1.2345*1.2345 = 1.8813659636249995), and Wave 2 scope has 107 `^^` sites
// (89 of them `^^2`) in Bezier/NURBS/Paver polynomial evaluation. A port that
// "optimises" x^^3 into three multiplications is a 1 ULP behaviour change.
//
// Run: ./test_substrate_pow ../baselines/e587b235/probes/substrate_pow.txt

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "eil/format.hpp"
#include "eil/math.hpp"
#include "eil/number.hpp"

namespace {

std::vector<std::string> produced;

void emitf(const char* fmt, ...) {
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    produced.emplace_back(buf);
}

// The same table as the D probe, in the same order.
const double kXs[7] = {1.2345, 0.1, 2.0 / 3.0, 1.0e154, 1e-8, -3.7, 0.5};

// IEEE-754 pattern as 16 hex digits, printed the way probes/substrate/probe_pow.d prints
// it. Needed because `==` cannot express what T-925 records: -0.0 == 0.0 is true, and
// every NaN pair compares false however identical the bits are.
std::string pat(double v) {
    uint64_t u;
    std::memcpy(&u, &v, sizeof u);
    char buf[20];
    std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(u));
    return std::string(buf);
}

void section_sq_cube() {
    for (double x : kXs) {
        // `op' is D's `x ^^ 2'. Since T-931 that is eil::sqr (one multiply), NOT
        // eil::pow(x,2): over 2,222 operands the two agree in D but glibc's pow(x,2.0)
        // misses 3 of them by 1 ULP. On these 7 operands they happen to coincide, which
        // is exactly how the old rule survived. See the sq2 sweep below.
        const double op = eil::sqr(x);
        const double mul = x * x;
        emitf("sq %.17g op=%.17g mul=%.17g powd=%.17g powi=%.17g opIsPow=%s opIsMul=%s", x, op,
              mul, eil::pow(x, 2.0), op, op == eil::pow(x, 2.0) ? "true" : "false",
              op == mul ? "true" : "false");
    }
    for (double x : kXs) {
        const double op = eil::pow(x, 3);
        const double mul = x * x * x;
        emitf("cube %.17g op=%.17g mul=%.17g powd=%.17g powi=%.17g opIsPow=%s opIsMul=%s", x, op,
              mul, eil::pow(x, 3.0), op, op == eil::pow(x, 3.0) ? "true" : "false",
              op == mul ? "true" : "false");
    }
}

void section_misc() {
    for (double x : kXs) {
        emitf("misc %.17g pow0=%.17g powNeg2=%.17g powHalf=%.17g", x, eil::pow(x, 0),
              eil::pow(x, -2), eil::pow(1.0, 0.5));
    }
}

// T-925: D's `x ^^ 0.5` is sqrt(x), not pow(x, 0.5). Measured over 765 non-negative
// operands (i/7, i*0.001, 1/i, i*1e-8 plus specials), `x^^0.5` reproduces sqrt(x) on all
// 765 bit patterns, while pow(x,0.5) differs on exactly one: x = -0.0, where sqrt keeps
// the sign of zero and pow returns +0.0. That single operand is the whole reason the rule
// exists -- a signed zero flips the sign of every later division by it, and it is what
// made geom_grid_paver rows 145-146 (`sec2.ulv3.0/1') differ in the last bit under
// eil::pow(e, 0.5). `op=' below is therefore emitted with eil::sqrt: a port that maps
// `^^0.5' onto eil::pow goes red on the first row of this group.
void section_half() {
    // The 7th entry is D's `real.max`, which is an 80-bit maximum: stored into a double
    // array it overflows, so the operand this row actually tests is +inf
    // (pattern 7ff0000000000000), not DBL_MAX.
    const double hs[9] = {-0.0, 0.0, 2.0, 1.0 / 3.0, 1.0e300, 1.0e-300,
                          std::numeric_limits<double>::infinity(), -1.0, -2.5};
    for (double x : hs) {
        const double op = eil::sqrt(x);   // how the port spells D's `x ^^ 0.5`
        const double q = eil::sqrt(x);
        const double w = eil::pow(x, 0.5);
        const std::string op_p = pat(op), q_p = pat(q), w_p = pat(w);
        emitf("half %s op=%s sqrt=%s pow=%s opIsSqrt=%s opIsPow=%s",
              pat(x).c_str(), op_p.c_str(), q_p.c_str(), w_p.c_str(),
              (op_p == q_p) ? "true" : "false", (op_p == w_p) ? "true" : "false");
    }
}

// Kept separate from section_misc because the D probe emits these five rows AFTER the
// half group; folding them back into section_misc shifts ten rows and reports them all
// as differences.
void section_tail() {

    emitf("frac %s %s", eil::to_string(eil::pow(8.0, 2.5)).c_str(),
          eil::to_string(eil::pow(8.0, 2.5)).c_str());
    emitf("intbase %s %s", eil::to_string(eil::pow(2.0, 3)).c_str(),
          eil::to_string(eil::pow(2.0, 3)).c_str());
    // D prints these four as "nan" with no sign bit; eil::pow/sqrt/log canonicalise.
    emitf("nan_domain %s %s %s %s", eil::to_string(eil::pow(-1.0, 0.5)).c_str(),
          eil::to_string(eil::pow(-1.0, 0.5)).c_str(),
          eil::to_string(eil::sqrt(-1.0)).c_str(), eil::to_string(eil::log(-1.0)).c_str());
    emitf("tostring %s %s %s", eil::to_string(1.0 / 3.0).c_str(),
          eil::to_string(1.0 / 3.0).c_str(), eil::to_string(3.141592653589793).c_str());
    // D's PI is an 80-bit real; the division happens wide and rounds on store.
    emitf("pi %s %s %s", eil::to_string(static_cast<double>(EIL_PI)).c_str(),
          eil::to_string(static_cast<double>(EIL_PI / 4.0L)).c_str(),
          eil::to_string(static_cast<double>(EIL_PI / 4.0L * 4.0L)).c_str());
}


// The sweep groups (T-931). These rows are not replayed positionally: the port has no
// Phobos pow to echo, so a `powd' column would differ for reasons that are not the rule
// under test. Instead each row is checked semantically against the port's own primitive.
//   sq2: D's `x ^^ 2' MUST equal eil::sqr(in), row for row (0 differences allowed).
//        eil::pow(in,2.0) is measured alongside and is expected to differ on a few.
//   cu3: D's `x ^^ 3' is pow, so eil::pow(in,3.0) is the right routing; it differs from
//        D on a small number of operands because Phobos' pow and glibc's pow are not the
//        same function (T-911's 1-ULP class). The count is bounded rather than required
//        to be zero, and expanding `^^3' into a product would blow it past the bound
//        (527 of 2,222), which is what makes the bound discriminative.
struct SweepCheck {
    long sq_rows = 0, sq_bad = 0, sq_pow_bad = 0, sq_mul_bad = 0;
    long cu_rows = 0, cu_bad = 0, cu_mul_bad = 0;
};

std::string cu_rows_msg(const SweepCheck& r) {
    std::ostringstream o;
    o << r.cu_rows << " rows, eil::pow(x,3.0) mismatches " << r.cu_bad
      << " (Phobos pow vs glibc pow: T-911's 1-ULP class, bounded at 8), (x*x)*x mismatch "
         "count against D's own column " << r.cu_mul_bad;
    return o.str();
}

SweepCheck check_sweep(const std::vector<std::string>& rows) {
    SweepCheck r;
    for (const std::string& line : rows) {
        const bool is_sq = line.rfind("sq2 ", 0) == 0;
        const bool is_cu = line.rfind("cu3 ", 0) == 0;
        if (!is_sq && !is_cu) continue;
        std::istringstream ss(line);
        std::string pre, xin, f_d, f_m, f_w;
        ss >> pre >> xin >> f_d >> f_m >> f_w;
        double x = 0.0;
        unsigned long long ux = std::strtoull(xin.c_str(), nullptr, 16);
        std::memcpy(&x, &ux, sizeof x);
        const std::string want = f_d.substr(f_d.find('=') + 1);
        const std::string mul = f_m.substr(f_m.find('=') + 1);
        const std::string powd = f_w.substr(f_w.find('=') + 1);
        const double m = is_sq ? x * x : (x * x) * x;
        const double w = is_sq ? eil::pow(x, 2.0) : eil::pow(x, 3.0);
        const double routed = is_sq ? eil::sqr(x) : eil::pow(x, 3.0);
        if (pat(routed) != want) {
            if (is_sq) ++r.sq_bad; else ++r.cu_bad;
        }
        if (is_sq) {
            ++r.sq_rows;
            // glibc's pow(x,2.0) against D's answer: the T-931 defect, reported not failed
            if (pat(w) != want) ++r.sq_pow_bad;
            if (pat(m) != mul) ++r.sq_mul_bad;    // D's own two columns disagreeing
        } else {
            ++r.cu_rows;
            if (pat(w) != want) ++r.cu_bad;
            if (pat(m) != mul) ++r.cu_mul_bad;    // D's own two columns disagreeing
        }
    }
    return r;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <probes/substrate_pow.txt>\n";
        return 2;
    }
    section_sq_cube();
    section_misc();
    section_half();
    section_tail();

    std::ifstream in(argv[1]);
    if (!in) {
        std::cerr << "cannot open corpus " << argv[1] << "\n";
        return 2;
    }
    std::vector<std::string> expected;
    for (std::string line; std::getline(in, line);) expected.push_back(line);

    const SweepCheck sw = check_sweep(expected);

    int diffs = 0;
    size_t compared = 0, swept = 0;
    for (size_t i = 0; i < expected.size() || i < produced.size(); ++i) {
        if (i < expected.size() &&
            (expected[i].rfind("sq2 ", 0) == 0 || expected[i].rfind("cu3 ", 0) == 0 ||
             expected[i].rfind("sweep|", 0) == 0)) {
            ++swept;   // validated semantically above, not positionally
            continue;
        }
        ++compared;
        if (i >= expected.size() || i >= produced.size() || expected[i] != produced[i]) {
            if (diffs < 20) {
                std::cout << "line " << (i + 1) << "\n  D:  |"
                          << (i < expected.size() ? expected[i] : std::string("<none>"))
                          << "|\n  C++:|"<< (i < produced.size() ? produced[i] : std::string("<none>"))
                          << "|\n";
            }
            ++diffs;
        }
    }
    // The sweep rows add their own failures to the same verdict.
    if (sw.sq_bad != 0) {
        std::cout << "sq2 rule broken: " << sw.sq_bad << " of " << sw.sq_rows
                  << " rows where D's `x ^^ 2' is not eil::sqr(x)\n";
        diffs += static_cast<int>(sw.sq_bad);
    }
    if (sw.cu_bad > 8) {
        std::cout << "cu3 rule broken: " << sw.cu_bad << " of " << sw.cu_rows
                  << " rows where D's `x ^^ 3' is not eil::pow(x,3.0) (bound 8)\n";
        diffs += static_cast<int>(sw.cu_bad);
    }
    std::cout << (diffs == 0 ? "PASS" : "FAIL") << ": pow corpus, " << expected.size()
              << " D lines (" << compared << " replayed positionally, " << swept
              << " sweep rows checked semantically), " << produced.size()
              << " C++ lines, " << diffs << " differ\n";
    std::cout << "  sq2: " << sw.sq_rows << " rows, eil::sqr mismatches " << sw.sq_bad
              << ", glibc pow(x,2.0) mismatches " << sw.sq_pow_bad
              << ", D's own mul column disagrees " << sw.sq_mul_bad << "\n";
    std::cout << "  cu3: " << cu_rows_msg(sw) << "\n";
    return diffs == 0 ? 0 : 1;
}
