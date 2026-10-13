// tests/test_nm_bracketing.cpp — the unittest block from nm/bracketing.d:71-84.
//
// See tests/test_nm_ridder.cpp for the shared translation notes on pow and on
// binding D's closure delegate as a named function.

#include <cmath>
#include <cstdio>

#include "eil/eil.hpp"
#include "nm/bracketing.hpp"
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

const eil::nm::number my_a = 3.0;

eil::nm::number test_fun_2(eil::nm::number x, eil::nm::number a) {
    return a*x + std::sin(x) - std::exp(x);
}

eil::nm::number test_fun_3(eil::nm::number x) { return test_fun_2(x, my_a); }

} // namespace

int main() {
    eil::nm::number x1 = 0.4;
    eil::nm::number x2 = 0.5;
    check(eil::nm::bracket<test_fun_3, eil::nm::number>(x1, x2)
              == static_cast<eil::nm::number>(0),
          "bracket: (0.4, 0.5) is widened to bracket the root of 3x + sin(x) - e^x");

    std::printf("%s: nm.bracketing unittests (%d failures)\n", failures ? "FAIL" : "PASS",
                failures);
    return failures ? 1 : 0;
}
