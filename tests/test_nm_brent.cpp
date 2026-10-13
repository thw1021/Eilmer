// tests/test_nm_brent.cpp — the unittest block from nm/brent.d:127-144.
//
// The D block cannot be compiled by ldc2 1.40: nm.brent.solve is `@nogc` and
// throws `new Exception` (brent.d:51, :123), so every instantiation, including
// this unittest, is rejected. The assertions are therefore ported and exercised
// here; the reference values for them came from a text-identical copy of the
// solve() body with only the `@nogc` attribute removed (see the port notes).
//
// See tests/test_nm_ridder.cpp for the shared translation notes on pow and on
// binding D's closure delegate as a named function.

#include <cmath>
#include <cstdio>

#include "eil/eil.hpp"
#include "nm/brent.hpp"
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
    return std::pow(x,3.0) + x*x - 3*x - 3;
}

eil::nm::number test_fun_2(eil::nm::number x, eil::nm::number a) {
    return a*x + std::sin(x) - std::exp(x);
}

eil::nm::number test_fun_3(eil::nm::number x) { return test_fun_2(x, my_a); }

} // namespace

int main() {
    check(std::fabs(eil::nm::brent::solve<test_fun_1, eil::nm::number>(
              static_cast<eil::nm::number>(1), static_cast<eil::nm::number>(2)) - 1.732051)
          < 1.0e-5,
          "brent: root of x^3 + x^2 - 3x - 3 on [1, 2]");
    check(std::fabs(eil::nm::brent::solve<test_fun_3, eil::nm::number>(
              static_cast<eil::nm::number>(0), static_cast<eil::nm::number>(1)) - 0.3604217)
          < 1.0e-5,
          "brent: root of 3x + sin(x) - e^x on [0, 1]");

    std::printf("%s: nm.brent unittests (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
