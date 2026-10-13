// tests/test_util_time_utils.cpp
//
// util/time_utils.d has no unittest block. This table is the reference behaviour
// captured from D (ldc2 against util/time_utils.d, /tmp/probe.d): the accepted
// forms, and the two *different* exception shapes for a bad --max-wall-clock value.
// eilmer/simcore.d:105 calls this during start-up and does not catch, so the text
// below is what a user sees; the distinction matters as well, because
// "Invalid maxWallClock setting" comes from object.Error while the conversion
// failures come from std.conv.ConvException, which a `catch (Exception)` would take.

#include <cstdio>
#include <string>
#include <vector>

#include "util/time_utils.hpp"

#include "eil/error.hpp"

namespace {
int failures = 0;
void check(bool ok, const std::string& what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what.c_str());
        ++failures;
    } else {
        std::printf("ok: %s\n", what.c_str());
    }
}

// D: object.Error for the wrong number of ':' fields, ConvException otherwise.
const char* kErrorPrefix = "Error";
const char* kConvPrefix = "Exception";

void expect_value(const std::string& in, int want) {
    const int got = eil::util::timeStringToSeconds(in);
    check(got == want, "timeStringToSeconds(\"" + in + "\") == " + std::to_string(want));
}

void expect_throw(const std::string& in, const std::string& kind, const std::string& msg) {
    std::string label = "timeStringToSeconds(\"" + in + "\") -> " + kind + " |" + msg + "|";
    try {
        const int v = eil::util::timeStringToSeconds(in);
        std::printf("FAIL: %s (returned %d instead)\n", label.c_str(), v);
        ++failures;
    } catch (const eil::ErrorBase& e) {
        check(std::string(kind) == kErrorPrefix && e.msg() == msg, label);
    } catch (const eil::ExceptionBase& e) {
        check(std::string(kind) == kConvPrefix && e.msg() == msg, label);
    }
}
}  // namespace

int main() {
    // Accepted forms: N seconds, and HH:MM:SS.
    expect_value("10", 10);
    expect_value("0", 0);
    expect_value("1:2:3", 3723);
    expect_value("0:0:0", 0);
    expect_value("23:59:59", 86399);
    expect_value("2:3:4", 7384);
    expect_value("0", 0);
    expect_value("2147483647", 2147483647);  // INT_MAX is still accepted
    expect_value("07", 7);    // D's to!int accepts a leading zero
    expect_value("+7", 7);    // and an explicit plus sign
    expect_value("-5", -5);   // and a negative, which the solver then treats oddly

    // Wrong field count -> object.Error, message includes the original text.
    expect_throw("1:30", kErrorPrefix, "Invalid maxWallClock setting 1:30");
    expect_throw("1:2:3:4", kErrorPrefix, "Invalid maxWallClock setting 1:2:3:4");
    expect_throw("", kErrorPrefix, "Invalid maxWallClock setting ");

    // Bad field content -> the std.conv message, character for character.
    expect_throw("abc", kConvPrefix,
                 "Unexpected 'a' when converting from type string to type int");
    expect_throw(" 7", kConvPrefix,
                 "Unexpected ' ' when converting from type string to type int");
    expect_throw("7 ", kConvPrefix,
                 "Unexpected ' ' when converting from type string to type int");
    expect_throw("1.5", kConvPrefix,
                 "Unexpected '.' when converting from type string to type int");
    expect_throw("1e3", kConvPrefix,
                 "Unexpected 'e' when converting from type string to type int");
    expect_throw("0x10", kConvPrefix,
                 "Unexpected 'x' when converting from type string to type int");
    expect_throw("-", kConvPrefix,
                 "Unexpected end of input when converting from type string to type int");
    expect_throw("2147483648", kConvPrefix, "Overflow in integral conversion");
    // The three-field form fails on each field in turn, hours first.
    expect_throw("x:2:3", kConvPrefix,
                 "Unexpected 'x' when converting from type string to type int");
    expect_throw("1:y:3", kConvPrefix,
                 "Unexpected 'y' when converting from type string to type int");
    expect_throw("1:2:z", kConvPrefix,
                 "Unexpected 'z' when converting from type string to type int");
    // "5:" splits to ["5", ""], which is two fields, so it is the shape error and
    // not a conversion error.
    expect_throw("5:", kErrorPrefix, "Invalid maxWallClock setting 5:");
    expect_throw("1:2:", kConvPrefix,
                 "Unexpected end of input when converting from type string to type int");

    std::printf("%s: util.time_utils (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
