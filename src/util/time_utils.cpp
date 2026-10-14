// util/time_utils.d -> src/util/time_utils.cpp
//
// Two std.conv/std.string behaviours have to be reproduced by hand because the
// substrate does not cover them with matching text:
//
//   split(s, ":")      -- util/time_utils.d:28. Measured against Phobos: an empty
//                         input yields *no* tokens, while a trailing separator
//                         yields an empty last token ("5:" gives ["5", ""]).
//   to!int(token)      -- util/time_utils.d:31-33, 37. eil::to_int reports the
//                         floating-point wording ("error converting input to
//                         floating point for input ..."), which is not what D
//                         prints and reaches the user through the uncaught
//                         exception text, so the integer rules are written out
//                         here from the measured Phobos messages.

#include "util/time_utils.hpp"

#include <cctype>
#include <climits>
#include <cstddef>
#include <string>
#include <vector>

#include "eil/error.hpp"
#include "eil/format.hpp"

namespace eil {
namespace util {
namespace {

// std.string.split(s, ":") for the separator this module uses.
std::vector<std::string> split_on_colon(const std::string& s) {
    std::vector<std::string> tokens;
    if (s.empty()) return tokens;  // D: "".split(":") has length 0
    size_t pos = 0;
    while (true) {
        const size_t nxt = s.find(':', pos);
        if (nxt == std::string::npos) {
            tokens.push_back(s.substr(pos));
            break;
        }
        tokens.push_back(s.substr(pos, nxt - pos));
        pos = nxt + 1;
        if (pos == s.size()) {  // a trailing separator still yields a field
            tokens.push_back(std::string());
            break;
        }
    }
    return tokens;
}

// std.conv.to!int(string), with Phobos' messages. D accepts an optional sign then
// one or more digits and requires the whole input to be used: " 7", "7 ", "1.5" and
// "0x10" all fail, "+3", "-0" and "07" all succeed.
int to_int_d(const std::string& s) {
    size_t i = 0;
    bool negative = false;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
        negative = (s[i] == '-');
        ++i;
    }
    if (i >= s.size()) {
        throw eil::ExceptionBase(
            "Unexpected end of input when converting from type string to type int");
    }
    long long acc = 0;
    bool any_digit = false;
    for (; i < s.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (!std::isdigit(c)) {
            throw eil::ExceptionBase(std::string("Unexpected '") + s[i] +
                                     "' when converting from type string to type int");
        }
        any_digit = true;
        acc = acc * 10 + (c - '0');
        // Beyond this the value cannot be brought back into int range, and D
        // reports the same overflow regardless of how much further it reads.
        if (acc > 1000000000000000000LL) {
            throw eil::ExceptionBase("Overflow in integral conversion");
        }
    }
    if (!any_digit) {
        throw eil::ExceptionBase(
            "Unexpected end of input when converting from type string to type int");
    }
    const long long value = negative ? -acc : acc;
    if (value > INT_MAX || value < INT_MIN) {
        throw eil::ExceptionBase("Overflow in integral conversion");
    }
    return static_cast<int>(value);
}

}  // namespace

// util/time_utils.d:14-43
int timeStringToSeconds(const std::string& timeString) {
    /*
        Convert the command line argument max-wall-clock from a string to an amount of seconds.

        Inputs:
         - timeString: may be an amount of seconds or HH:MM:SS format.

        Outputs:
         - The max wall clock time, in seconds, as an integer.

        Notes:
        @author: Nick Gibbons
    */
    int outseconds = 0;
    auto tokens = split_on_colon(timeString);

    if (tokens.size() == 3) {
        int hours = to_int_d(tokens[0]);
        int minutes = to_int_d(tokens[1]);
        int seconds = to_int_d(tokens[2]);
        outseconds = 3600 * hours + 60 * minutes + seconds;

    } else if (tokens.size() == 1) {
        outseconds = to_int_d(tokens[0]);
    } else {
        // D: throw new Error(...). eil::ErrorBase is the port's object.Error, and
        // the file/line recorded are this site's, as D's `new Error(m)` does.
        throw eil::ErrorBase(eil::format("Invalid maxWallClock setting %s", timeString));
    }

    return outseconds;
}

}  // namespace util
}  // namespace eil
