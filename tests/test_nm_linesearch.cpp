// tests/test_nm_linesearch.cpp — the unittest block from nm/linesearch.d:52-68.

#include <cmath>
#include <cstdio>

#include "eil/eil.hpp"
#include "nm/linesearch.hpp"
#include "nm/number.hpp"   // the D unittest's `import nm.number;`

namespace {

int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    } else {
        std::printf("ok: %s\n", what);
    }
}

eil::nm::number fdemo(eil::nm::number x) {
    return std::exp(x) + 2.0 - std::cos(x);
}

} // namespace

int main() {
    eil::nm::number a = -3;
    eil::nm::number b = 1;
    eil::nm::linesearch::minimize<fdemo, eil::nm::number>(a, b, 1.0e-6);
    eil::nm::number xminimum = -0.588534;

    check(std::fabs(a - xminimum) < 1.0e-4, "linesearch: left end of the minimum bracket");
    check(std::fabs(b - xminimum) < 1.0e-4, "linesearch: right end of the minimum bracket");

    std::printf("%s: nm.linesearch unittests (%d failures)\n", failures ? "FAIL" : "PASS",
                failures);
    return failures ? 1 : 0;
}
