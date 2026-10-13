#pragma once
// eil/conv.hpp — std.conv.to for the conversions the solver uses.
//
// 1,121 of the 1,393 `!` instantiations in src/eilmer are `to!X`, and the
// remainder are mostly isNumeric/Complex, so this one header covers most of the
// template surface of the whole solver.
//
// Failure text is the measured Phobos message (tools/fmt_corpus.txt):
//   error converting input to floating point for input "not a number".
// D's to!double tolerates leading/trailing whitespace and rejects trailing
// junk; both behaviours are reproduced because globalconfig.d parses option
// strings through it and the error text reaches the user.

#include <cstdlib>
#include <cstring>
#include <string>

#include "eil/error.hpp"
#include "eil/format.hpp"

namespace eil {

// to!string
template <typename T> std::string to_string_via_conv(const T& v) { return detail::to_string(v); }

// to!double
inline double to_double(const std::string& s) {
    const char* begin = s.c_str();
    char* end = nullptr;
    errno = 0;
    const double v = std::strtod(begin, &end);
    while (end && *end && std::isspace(static_cast<unsigned char>(*end))) ++end;
    if (end == begin || *end != '\0' || errno == EINVAL) throw ConversionException(s);
    return v;
}

// to!int / to!long — D's integer conversion is strict about surrounding space.
inline long to_long(const std::string& s) {
    const char* begin = s.c_str();
    char* end = nullptr;
    errno = 0;
    const long v = std::strtol(begin, &end, 10);
    while (end && *end && std::isspace(static_cast<unsigned char>(*end))) ++end;
    if (end == begin || *end != '\0' || errno == ERANGE) {
        throw ConversionException("error converting input " + s + " to type long");
    }
    return v;
}
inline int to_int(const std::string& s) { return static_cast<int>(to_long(s)); }

// The single entry point ports use, mirroring `to!T(value)`.
template <typename T, typename U> T to(const U& v) {
    if constexpr (std::is_same_v<T, std::string>) {
        return detail::to_string(v);
    } else if constexpr (std::is_same_v<T, double>) {
        return to_double(detail::to_string(v));
    } else if constexpr (std::is_integral_v<T>) {
        return static_cast<T>(to_long(detail::to_string(v)));
    } else {
        static_assert(sizeof(T) == 0, "eil::to: add the conversion when a ported file needs it");
    }
}

} // namespace eil
