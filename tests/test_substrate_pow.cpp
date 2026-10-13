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
