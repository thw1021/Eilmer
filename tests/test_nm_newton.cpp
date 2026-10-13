// tests/test_nm_newton.cpp — the unittest block from nm/newton.d:141-173.
//
// Like brent's, the D block cannot be compiled by ldc2 1.40: nm.newton.solve is
// `@nogc` and throws `new NumericalMethodException` (newton.d:84, :137), so every
// instantiation is rejected, this unittest included. The assertions are ported
// unchanged and the reference values came from a text-identical copy of the
// solve() body with only the `@nogc` attribute removed.
//
// See tests/test_nm_ridder.cpp for the shared translation notes on pow.

#include <cmath>
#include <cstdio>

#include "eil/eil.hpp"
#include "nm/newton.hpp"
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

eil::nm::number test_fun_1(eil::nm::number x) {
    return std::pow(x, 3.0) + x*x - 3 * x - 3;
}

eil::nm::number test_dfun_1(eil::nm::number x) {
    return 3.0 * x*x + 2.0 * x - 3.0;
}

eil::nm::number test_fun_2(eil::nm::number x, eil::nm::number a) {
    return a * x + std::sin(x) - std::exp(x);
}

eil::nm::number test_dfun_2(eil::nm::number x, eil::nm::number a) {
    return a + std::cos(x) - std::exp(x);
}

eil::nm::number test_fun_3(eil::nm::number x) {
    return test_fun_2(x, my_a);
}

eil::nm::number test_dfun_3(eil::nm::number x) {
    return test_dfun_2(x, my_a);
}

} // namespace

int main() {
    check(std::fabs(eil::nm::newton::solve<test_fun_1, test_dfun_1, eil::nm::number>(
              static_cast<eil::nm::number>(1.0), static_cast<eil::nm::number>(-5.0),
              static_cast<eil::nm::number>(5.0)) - 1.732051) < 1.0e-5,
          "newton: root of x^3 + x^2 - 3x - 3 with bracket (-5, 5)");
    check(std::fabs(eil::nm::newton::solve<test_fun_3, test_dfun_3, eil::nm::number>(
              static_cast<eil::nm::number>(0.2), static_cast<eil::nm::number>(0.0),
              static_cast<eil::nm::number>(1.0)) - 0.3604217) < 1.0e-5,
          "newton: root of 3x + sin(x) - e^x with bracket (0, 1)");

    std::printf("%s: nm.newton unittests (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
