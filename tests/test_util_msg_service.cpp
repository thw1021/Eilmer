// tests/test_util_msg_service.cpp
//
// util/msg_service.d has no unittest block, so this file pins the contract the
// solver depends on: the three message templates, byte for byte, and the rule that
// the default file/line arguments name the *caller* (measured against Phobos, and
// the reason the port exposes EIL_BROKEN_*_CONDITION macros rather than C++ default
// arguments). The expected texts come from the D reference run, not from the D
// source being assumed correct.

#include <cstdio>
#include <cstring>
#include <string>

#include "util/msg_service.hpp"

#include "eil/error.hpp"
#include "eil/format.hpp"

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
}  // namespace

int main() {
    using eil::util::brokenPreCondition;
    using eil::util::brokenPostCondition;
    using eil::util::failedUnitTest;

    // msg_service.d:17-18, 25-26, 32-33 with the location supplied explicitly, as
    // gas/diffusion/sutherland_viscosity.d:38 does. Trailing newline included.
    const std::string pre = brokenPreCondition("varA", 111, "some/file.d");
    check(pre == "Pre-condition contract broken for varA on line 111 in file some/file.d\n",
          "brokenPreCondition text matches D");
    const std::string post = brokenPostCondition("varB", 222, "some/file.d");
    check(post == "Post-condition contract broken for varB on line 222 in file some/file.d\n",
          "brokenPostCondition text matches D");
    const std::string unit = failedUnitTest(333, "some/file.d");
    check(unit == "Unit test failure on line 333 in file some/file.d\n",
          "failedUnitTest text matches D");

    // The variable text is arbitrary in D (`brokenPreCondition("Inconsistent array
    // lengths.")` at gas/gas_model.d:211), so it is not restricted to identifiers.
    check(brokenPreCondition("Inconsistent array lengths.", 1, "gas/gas_model.d") ==
              "Pre-condition contract broken for Inconsistent array lengths. on line 1 in "
              "file gas/gas_model.d\n",
          "non-identifier variable name passes through");

    // Defaults: D resolves __LINE__/__FILE__ at the call site, so the macro must
    // report *this* file and *this* line.
    const size_t here = __LINE__ + 1;
    const std::string d = EIL_BROKEN_PRE_CONDITION("varC");
    const std::string want =
        "Pre-condition contract broken for varC on line " + std::to_string(here) +
        " in file " + __FILE__ + "\n";
    check(d == want, "default location is the caller's file and line");

    const size_t here2 = __LINE__ + 1;
    const std::string d2 = EIL_BROKEN_POST_CONDITION("varD");
    check(d2 == "Post-condition contract broken for varD on line " + std::to_string(here2) +
                    " in file " + __FILE__ + "\n",
          "post-condition default location is the caller's too");

    const size_t here3 = __LINE__ + 1;
    const std::string d3 = EIL_FAILED_UNIT_TEST();
    check(d3 == "Unit test failure on line " + std::to_string(here3) + " in file " +
                    __FILE__ + "\n",
          "unit-test default location is the caller's");

    // Use site in the D tree: assert(cond, brokenPreCondition(...)) -- the string is
    // what a failing assert prints, so it must survive as-is when used that way.
    bool cond = false;
    try {
        if (!cond) throw eil::ErrorBase(brokenPreCondition("T_ref", 38, "x.d"), __FILE__, 38);
        check(false, "assert-style throw should have happened");
    } catch (const eil::ErrorBase& e) {
        check(std::strcmp(e.what(),
                          "Pre-condition contract broken for T_ref on line 38 in file x.d\n") == 0,
              "message reaches the exception text unchanged");
    }

    std::printf("%s: util.msg_service (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
