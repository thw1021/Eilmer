#pragma once
/**
 * time_utils.d
 * Some routines for converting and handling times
 *
 * @author: Nick Gibbons
 */
// util/time_utils.d -> src/util/time_utils.hpp (module util.time_utils).
//
// Header note: this module holds no clock. It only parses the --max-wall-clock
// command-line value into seconds (eilmer/simcore.d:105 is the call site), so the
// monotonic-versus-wall-clock question and the elapsed-time text that goes into the
// solver log live in simcore.d/globaldata.d, not here. Nothing in this file prints
// time; the only text it produces is the two error messages below.
//
// Both are user-facing and kept byte-identical to D:
//   wrong number of ':' fields -> object.Error "Invalid maxWallClock setting %s"
//   a field that is not an int -> std.conv.ConvException
//                                 "Unexpected '<c>' when converting from type
//                                  string to type int"
//                                 "Unexpected end of input when converting from
//                                  type string to type int"
//                                 "Overflow in integral conversion"
// The last three are measured Phobos text (see the probe in /tmp). They cannot come
// from eil/conv.hpp, whose to_int reports the floating-point wording instead, so
// the conversion is done locally in time_utils.cpp. The first is thrown as
// eil::ErrorBase, D's `Error`, which `catch (Exception)` does not catch.

#include <string>

namespace eil {
namespace util {

int timeStringToSeconds(const std::string& timeString);

}  // namespace util
}  // namespace eil
