// util/time_utils.d -> src/util/time_utils.cpp
//
// One std.string behaviour is reproduced by hand because the substrate does not
// cover it with matching text:
//
//   split(s, ":")      -- util/time_utils.d:28. Measured against Phobos: an empty
//                         input yields *no* tokens, while a trailing separator
//                         yields an empty last token ("5:" gives ["5", ""]).
//
// The `to!int(token)` sites (util/time_utils.d:31-33, 37) go straight through
// substrate's eil::to_int, which reproduces Phobos' strict integer grammar AND its
// range check: a grammar failure is std.conv.ConvException ("Unexpected ' ' ..."), an
// out-of-range value is std.conv.ConvOverflowException ("Overflow in integral
// conversion") — see substrate_conv_int_typename.txt. This file previously carried a
// third hand-written scanner; it was deleted under T-928 so all three string->int
// routes (this, usgrid, substrate) share one implementation and one set of classes.

#include "util/time_utils.hpp"

#include <cstddef>
#include <string>
#include <vector>

#include "eil/conv.hpp"
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
        int hours = eil::to_int(tokens[0]);
        int minutes = eil::to_int(tokens[1]);
        int seconds = eil::to_int(tokens[2]);
        outseconds = 3600 * hours + 60 * minutes + seconds;

    } else if (tokens.size() == 1) {
        outseconds = eil::to_int(tokens[0]);
    } else {
        // D: throw new Error(...). eil::ErrorBase is the port's object.Error, and
        // the file/line recorded are this site's, as D's `new Error(m)` does.
        throw eil::ErrorBase(eil::format("Invalid maxWallClock setting %s", timeString));
    }

    return outseconds;
}

}  // namespace util
}  // namespace eil
