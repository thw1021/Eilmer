// tests/test_nm_ridder.cpp — the unittest block from nm/ridder.d:76-92.
//
// Translation notes for the test bodies (they apply to every test in this
// cluster): `pow(x,3)` in Phobos resolves to the floating-point pow, which this
// build reproduces as std::pow(x, 3.0); `pow(x,2)` is Phobos' special case and
// measured bit-identical to x*x over 200000 samples, so it is written as x*x.
// D's unittest binds a closure delegate as the alias parameter (ridder.d:90);
// a C++ non-type template argument must name a function, so test_fun_3 is the
// named function over my_a — the same shape brent.d:140 and newton.d:164 use.

#include <cmath>
#include <cstdio>

#include "eil/eil.hpp"
#include "nm/number.hpp"   // the D unittest's `import nm.number;`
#include "nm/ridder.hpp"

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
    check(std::fabs(eil::nm::ridder::solve<test_fun_1, eil::nm::number>(
              static_cast<eil::nm::number>(1), static_cast<eil::nm::number>(2)) - 1.732051)
          < 1.0e-5,
          "ridder: root of x^3 + x^2 - 3x - 3 on [1, 2]");
    check(std::fabs(eil::nm::ridder::solve<test_fun_3, eil::nm::number>(
              static_cast<eil::nm::number>(0), static_cast<eil::nm::number>(1)) - 0.3604217)
          < 1.0e-5,
          "ridder: root of 3x + sin(x) - e^x on [0, 1]");

    std::printf("%s: nm.ridder unittests (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
