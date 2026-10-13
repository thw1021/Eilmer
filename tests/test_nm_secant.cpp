// tests/test_nm_secant.cpp — the unittest block from nm/secant.d:56-72.
//
// See tests/test_nm_ridder.cpp for the shared translation notes on pow and on
// binding D's closure delegate as a named function.

#include <cmath>
#include <cstdio>

#include "eil/eil.hpp"
#include "nm/number.hpp"   // the D unittest's `import nm.number;`
#include "nm/secant.hpp"

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

eil::nm::number test_fun_1(eil::nm::number x) {
    return std::pow(x,3.0) + x*x - 3*x - 3;
}

eil::nm::number test_fun_2(eil::nm::number x, eil::nm::number a) {
    return a*x + std::sin(x) - std::exp(x);
}

eil::nm::number test_fun_3(eil::nm::number x) { return test_fun_2(x, my_a); }

} // namespace

int main() {
    check(std::fabs(eil::nm::secant::solve<test_fun_1, eil::nm::number>(
              static_cast<eil::nm::number>(1.5), static_cast<eil::nm::number>(1)) - 1.732051)
          < 1.0e-5,
          "secant: root of x^3 + x^2 - 3x - 3 from 1.5 and 1");
    check(std::fabs(eil::nm::secant::solve<test_fun_3, eil::nm::number>(
              static_cast<eil::nm::number>(0), static_cast<eil::nm::number>(0.1)) - 0.3604217)
          < 1.0e-5,
          "secant: root of 3x + sin(x) - e^x from 0 and 0.1");

    std::printf("%s: nm.secant unittests (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
