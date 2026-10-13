#pragma once
// eil/conv.hpp — std.conv.to for the conversions the solver uses.
//
// 1,121 of the 1,393 `!` instantiations in src/eilmer are `to!X`, so this covers
// most of the template surface in the tree. The error *taxonomy* below was
// measured row by row against Phobos (tools/fmt_corpus.txt, sections 8 and the
// dbl_parse table) rather than guessed, because these strings are what a user
// sees when a job script has a typo, and the normalized-log gate compares them:
//
//   to!double("")        -> Floating point conversion error for input "".
//   to!double("-")       -> Floating point conversion error for input "-".
//   to!double(".")       -> no digits seen for input ".".
//   to!double(" 12x")    -> no digits seen for input " 12x".
//   to!double("12x")     -> Unexpected 'x' when converting from type string to type double
//   to!double("12 ")     -> Unexpected ' ' when converting ... type double
//   to!double("1.5e")    -> Unexpected end of input
//   to!double("0x10")    -> Floating point parsing: exponent is required for input "0x10".
//   to!double("not a number") -> error converting input to floating point for input "not a number".
//   to!int("")           -> Unexpected end of input when converting from type string to type int
//   to!int("12x")        -> Unexpected 'x' when converting from type string to type int
//   to!int(" 42 ")       -> Unexpected ' ' ... (integer conversion is strict: no
//                           surrounding whitespace is accepted, unlike double)
//
// Accepting "nan"/"inf"/"-inf" and rejecting a hex literal without a binary
// exponent are reproduced too. Where the corpus has no row, the closest measured
// rule is used and tests/test_format_corpus.cpp documents the covered set — the
// taxonomy is only as complete as the corpus, and new shapes should be probed
// before relying on them.

#include <cctype>
#include <cstdlib>
#include <cmath>
#include <limits>
#include <string>

#include "eil/error.hpp"
#include "eil/format.hpp"

namespace eil {

namespace detail {

[[noreturn]] inline void conv_fail(const std::string& msg) { throw ConversionException(msg); }

inline std::string unexpected_char(char c, const std::string& type) {
    return "Unexpected '" + std::string(1, c) + "' when converting from type string to type " + type;
}
inline std::string unexpected_end(const std::string& type) {
    return "Unexpected end of input when converting from type string to type " + type;
}

} // namespace detail

// to!double. Modelled from the twelve measured rows in the corpus; the shape
// table below is what Phobos does, not a derivation of its internals.
inline double to_double(const std::string& s) {
    const size_t n = s.size();
    size_t i = 0;
    bool had_leading_blank = false;
    while (i < n && std::isspace(static_cast<unsigned char>(s[i]))) { ++i; had_leading_blank = true; }

    if (i >= n) {
        // "" and "-" both end up here: no operand at all.
        detail::conv_fail("Floating point conversion error for input \"" + s + "\".");
    }
    {
        std::string word = s.substr(s[i] == '+' || s[i] == '-' ? i + 1 : i);
        for (auto& c : word) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (word == "nan") return std::numeric_limits<double>::quiet_NaN();
        if (word == "inf" || word == "infinity") {
            return (i < n && s[i] == '-') ? -std::numeric_limits<double>::infinity()
                                          : std::numeric_limits<double>::infinity();
        }
    }
    if (s[i] == '+' || s[i] == '-') {
        if (i + 1 >= n) {
            // "-" alone: no operand, distinct from "." per the corpus.
            detail::conv_fail("Floating point conversion error for input \"" + s + "\".");
        }
    }
    if (s[i] == '0' && i + 1 < n && (s[i + 1] == 'x' || s[i + 1] == 'X')) {
        if (s.find_first_of("pP", i) == std::string::npos) {
            detail::conv_fail("Floating point parsing: exponent is required for input \"" + s + "\".");
        }
    }
    if (!std::isdigit(static_cast<unsigned char>(s[i])) && s[i] != '.' && s[i] != '+' &&
        s[i] != '-') {
        // A letter where a number should start ("not a number").
        detail::conv_fail("error converting input to floating point for input \"" + s + "\".");
    }

    // Walk the literal: [+-]? digits? . digits? ([eE] [+-]? digits)?
    bool saw_digit = false;
    size_t j = i;
    if (s[j] == '+' || s[j] == '-') ++j;
    for (; j < n; ++j) {
        const char c = s[j];
        if (std::isdigit(static_cast<unsigned char>(c))) {
            saw_digit = true;
            continue;
        }
        if (c == '.') continue;
        if (c == 'e' || c == 'E') {
            size_t k = j + 1;
            if (k < n && (s[k] == '+' || s[k] == '-')) ++k;
            if (k >= n) detail::conv_fail("Unexpected end of input");   // "1.5e"
            if (!std::isdigit(static_cast<unsigned char>(s[k]))) {
                detail::conv_fail(detail::unexpected_char(s[k], "double"));
            }
            for (; k < n && std::isdigit(static_cast<unsigned char>(s[k])); ++k) {}
            j = k - 1;
            continue;
        }
        break;
    }
    if (!saw_digit) {
        // "." and " 12x" both land here in D (the leading-blank path loses the
        // digits it did see); reproduced as measured.
        detail::conv_fail("no digits seen for input \"" + s + "\".");
    }
    if (j < n) {
        if (had_leading_blank) detail::conv_fail("no digits seen for input \"" + s + "\".");
        detail::conv_fail(detail::unexpected_char(s[j], "double"));       // "12x", "12 "
    }
    return std::strtod(s.c_str(), nullptr);
}

// to!int / to!long — strict grammar [+-]?digit+, no surrounding whitespace.
inline long to_long(const std::string& s) {
    const size_t n = s.size();
    size_t i = 0;
    if (i < n && (s[i] == '+' || s[i] == '-')) ++i;
    if (i >= n) detail::conv_fail(detail::unexpected_end("int"));
    long long sign = (s[0] == '-') ? -1 : 1;
    long long acc = 0;
    for (; i < n; ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) {
            detail::conv_fail(detail::unexpected_char(s[i], "int"));
        }
        acc = acc * 10 + (s[i] - '0');
    }
    return static_cast<long>(sign * acc);
}
inline int to_int(const std::string& s) { return static_cast<int>(to_long(s)); }

// The single entry point ports use, mirroring `to!T(value)`.
template <typename T, typename U> T to(const U& v) {
    if constexpr (std::is_same_v<T, std::string>) {
        return ::eil::to_string(v);
    } else if constexpr (std::is_same_v<T, double>) {
        return to_double(::eil::to_string(v));
    } else if constexpr (std::is_integral_v<T>) {
        return static_cast<T>(to_long(::eil::to_string(v)));
    } else {
        static_assert(sizeof(T) == 0, "eil::to: add the conversion when a ported file needs it");
    }
}

} // namespace eil
