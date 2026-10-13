// tests/test_nm_schedule.cpp — the unittest block from nm/schedule.d:64-82.
//
// The D block compiles and passes in the reference tree, so each assertion below
// is the D assertion. D's `isClose(a, b)` uses std.math's default tolerances
// (relTol 1e-8, absTol 0), which is eil::isClose's default pair too (see
// substrate/include/eil/math.hpp and the 484-row corpus in tools/fmt_corpus.txt).
//
// `new Schedule!double([...], [...])` is spelled new_schedule<T>(...) for the
// reason given at the top of src/nm/schedule.hpp.

#include <cstdio>

#include "eil/eil.hpp"
#include "nm/schedule.hpp"

namespace {

namespace schedule = eil::nm::schedule;

int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    } else {
        std::printf("ok: %s\n", what);
    }
}

} // namespace

int main() {
    auto mysched = schedule::new_schedule<double>({0.0, 1.0, 1.5, 2.0}, {1.0, 1.0, 0.5, 0.0});
    check(eil::isClose(mysched.get_value(-1.0), 1.0), "schedule: get_value below the range");
    check(eil::isClose(mysched.interpolate_value(-1.2), 1.0), "schedule: interpolate below the range");
    check(eil::isClose(mysched.get_value(1.2), 1.0), "schedule: get_value inside the range");
    check(eil::isClose(mysched.interpolate_value(1.2), 0.8), "schedule: interpolate inside the range");
    check(eil::isClose(mysched.get_value(2.2), 0.0), "schedule: get_value above the range");
    check(eil::isClose(mysched.interpolate_value(2.2), 0.0), "schedule: interpolate above the range");

    auto isched = schedule::new_schedule<int>({0.0, 1.0, 1.5, 2.0}, {1, 1, 2, 0});
    check(isched.get_value(-1.0) == 1, "int schedule: get_value below the range");
    check(isched.interpolate_value(-1.2) == 1, "int schedule: interpolate below the range");
    check(isched.get_value(1.2) == 1, "int schedule: get_value inside the range");
    check(isched.interpolate_value(1.2) == 1, "int schedule: interpolate rounds");
    check(isched.get_value(2.2) == 0, "int schedule: get_value above the range");
    check(isched.interpolate_value(2.2) == 0, "int schedule: interpolate above the range");

    std::printf("%s: nm.schedule unittests (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
