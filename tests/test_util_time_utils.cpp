// tests/test_util_time_utils.cpp
//
// util/time_utils.d has no unittest block. This table is the reference behaviour
// captured from D (ldc2 against util/time_utils.d, /tmp/probe.d): the accepted
// forms, and the *different* exception shapes for a bad --max-wall-clock value.
// eilmer/simcore.d:105 calls this during start-up and does not catch, so both the
// text and the class below are what a user (and a `catch` in the caller) sees.
//
// expect_throw asserts the D CLASS NAME, not a coarse "Error"/"Exception" label.
// That is deliberate (backlog T-928 item 4): the previous version caught
// ErrorBase/ExceptionBase and compared only .msg, so the class was never checked —
// ConversionException derives from ExceptionBase, which meant a grammar ConvException,
// an overflow ConvOverflowException and a plain ExceptionBase all landed in the same
// branch and a wrong-class regression was invisible (the same blind spot as T-922).
// Now the port's exception is mapped to D's class the way the corpus harnesses do
// (ConvOverflowException tested before its parent) and compared to the expected name:
//   wrong ':' field count -> object.Error           ("Invalid maxWallClock setting ...")
//   bad field grammar     -> std.conv.ConvException ("Unexpected ... type int")
//   out-of-range field    -> std.conv.ConvOverflowException ("Overflow in integral conversion")

#include <cstdio>
#include <string>

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

// The D class names this table expects (measured: substrate_throw_identity.txt for the
// object.Error / std.conv.* mapping, substrate_conv_int_typename.txt for the conv texts).
const char* kErr = "object.Error";
const char* kConv = "std.conv.ConvException";
const char* kOverflow = "std.conv.ConvOverflowException";

// Map the port's exception type to D's class name, testing the overflow child before its
// parent ConversionException (else the parent would swallow it).
std::string d_class_of(const eil::ThrowableBase& e) {
    if (dynamic_cast<const eil::ConvOverflowException*>(&e) != nullptr)
        return "std.conv.ConvOverflowException";
    if (dynamic_cast<const eil::ConversionException*>(&e) != nullptr)
        return "std.conv.ConvException";
    if (dynamic_cast<const eil::ErrorBase*>(&e) != nullptr) return "object.Error";
    if (dynamic_cast<const eil::ExceptionBase*>(&e) != nullptr) return "object.Exception";
    return "UNMAPPED";
}

void expect_value(const std::string& in, int want) {
    const int got = eil::util::timeStringToSeconds(in);
    check(got == want, "timeStringToSeconds(\"" + in + "\") == " + std::to_string(want));
}

void expect_throw(const std::string& in, const std::string& want_class, const std::string& msg) {
    const std::string label =
        "timeStringToSeconds(\"" + in + "\") -> " + want_class + " |" + msg + "|";
    try {
        const int v = eil::util::timeStringToSeconds(in);
        std::printf("FAIL: %s (returned %d instead)\n", label.c_str(), v);
        ++failures;
    } catch (const eil::ThrowableBase& e) {
        const std::string got = d_class_of(e);
        const bool ok = (got == want_class) && (e.msg() == msg);
        check(ok, ok ? label : label + "  [got " + got + " |" + e.msg() + "|]");
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
    expect_throw("1:30", kErr, "Invalid maxWallClock setting 1:30");
    expect_throw("1:2:3:4", kErr, "Invalid maxWallClock setting 1:2:3:4");
    expect_throw("", kErr, "Invalid maxWallClock setting ");

    // Bad field content -> std.conv.ConvException, message character for character.
    expect_throw("abc", kConv, "Unexpected 'a' when converting from type string to type int");
    expect_throw(" 7", kConv, "Unexpected ' ' when converting from type string to type int");
    expect_throw("7 ", kConv, "Unexpected ' ' when converting from type string to type int");
    expect_throw("1.5", kConv, "Unexpected '.' when converting from type string to type int");
    expect_throw("1e3", kConv, "Unexpected 'e' when converting from type string to type int");
    expect_throw("0x10", kConv, "Unexpected 'x' when converting from type string to type int");
    expect_throw("-", kConv, "Unexpected end of input when converting from type string to type int");
    // Out of int range -> std.conv.ConvOverflowException, a DIFFERENT class from the
    // grammar failures above even though a `catch (Exception)` takes both.
    expect_throw("2147483648", kOverflow, "Overflow in integral conversion");
    expect_throw("-2147483649", kOverflow, "Overflow in integral conversion");
    // The three-field form fails on each field in turn, hours first.
    expect_throw("x:2:3", kConv, "Unexpected 'x' when converting from type string to type int");
    expect_throw("1:y:3", kConv, "Unexpected 'y' when converting from type string to type int");
    expect_throw("1:2:z", kConv, "Unexpected 'z' when converting from type string to type int");
    // "5:" splits to ["5", ""], which is two fields, so it is the shape error and
    // not a conversion error.
    expect_throw("5:", kErr, "Invalid maxWallClock setting 5:");
    expect_throw("1:2:", kConv,
                 "Unexpected end of input when converting from type string to type int");

    std::printf("%s: util.time_utils (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
