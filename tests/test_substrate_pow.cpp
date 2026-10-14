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
        const double op = eil::pow(x, 2);
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

    int diffs = 0;
    for (size_t i = 0; i < expected.size() || i < produced.size(); ++i) {
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
    std::cout << (diffs == 0 ? "PASS" : "FAIL") << ": pow corpus, " << expected.size()
              << " D lines, " << produced.size() << " C++ lines, " << diffs << " differ\n";
    return diffs == 0 ? 0 : 1;
}
