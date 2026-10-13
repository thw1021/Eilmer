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

// Phobos renders the offending character inside these messages with exactly one
// escape: '\n' arrives as the two characters backslash-n, while every other byte
// goes through raw — including '\t', which is why the corpus rows for "1\t" and
// "1\n" differ in length by one (measured with ldc2 1.40.0: len 59 vs 60 for int,
// 62 vs 63 for double). A multi-byte character is re-encoded whole, so the leading
// byte's entire UTF-8 sequence is copied, not just that byte (measured: "1é" prints
// both bytes 0xC3 0xA9).
//
// The same rule holds for to!int and to!double, so it lives here once.
inline std::string render_unexpected(char c, const std::string& s, size_t at) {
    if (c == '\n') return "\\n";
    std::string out(1, c);
    if ((static_cast<unsigned char>(c) & 0xE0) == 0xC0 && at + 1 < s.size()) {
        const size_t want = (static_cast<unsigned char>(c) & 0xF0) == 0xE0 ? 3
                          : (static_cast<unsigned char>(c) & 0xF8) == 0xF0 ? 4 : 2;
        for (size_t k = 1; k < want && at + k < s.size(); ++k) {
            if ((static_cast<unsigned char>(s[at + k]) & 0xC0) != 0x80) break;
            out += s[at + k];
        }
    }
    return out;
}

inline std::string unexpected_char(char c, const std::string& type) {
    return "Unexpected '" + std::string(1, c) + "' when converting from type string to type " + type;
}
inline std::string unexpected_char_at(const std::string& s, size_t at, const std::string& type) {
    return "Unexpected '" + render_unexpected(s[at], s, at) +
           "' when converting from type string to type " + type;
}
inline std::string unexpected_end(const std::string& type) {
    return "Unexpected end of input when converting from type string to type " + type;
}

} // namespace detail

// to!double — the Phobos grammar, transcribed from measurement rather than from
// std.conv's source, because five of its branches are not what a C reader expects.
//
// Every row of probes/substrate/probe_conv_failtext.d (365 rows: the converted value
// or the exact exception text, captured from ldc2 1.40.0) is what this function is
// checked against by tests/test_substrate_conv_failtext.cpp. The surprises, all
// measured:
//
//  1. NaN and infinity are word-matched with different privileges. "nan" is accepted
//     behind '+' or '-' ("+nan" is NaN), but "inf" is accepted only behind '-' or
//     nothing: "+inf" reports `no digits seen for input "+inf".` while "-inf" is
//     -inf. Case-insensitive, and matching only the three letters: "infinity" and
//     "INFINITY" parse as inf and then fail on the leftover with `Unexpected 'i'`.
//     A trailing junk after a word is also `Unexpected` ("nanabc" -> 'a', "nan " -> ' ').
//  2. An operand whose first character is 'n'/'N' or 'i'/'I' but which does not
//     complete the word reports `error converting input to floating point`, and
//     EVERY OTHER non-digit opener reports `no digits seen for input "X".`. The old
//     implementation inverted that for anything not starting with n/i, which is how
//     `to!double("#c")` came out wrong (backlog T-913): D says
//     `no digits seen for input "#c".`, the port said `error converting input ...`.
//     The words are tried from the position after the optional sign, but '+' before
//     'i' skips straight to the digits path, which is why "+i" and "+in" are
//     `no digits seen` while "-i" and "-in" are `error converting`.
//  3. `1e` is `Unexpected end of input`, but `1e+` is `No digits seen.` — the sign
//     alone after the exponent marker changes the message.
//  4. '_' is a digit separator, allowed only immediately after a digit: `1_2` is 12,
//     `1_` is 1, `1_e5` is 100000, but `1__2`, `1._5`, `_1`, `1e5_` and `0x_10p1` all
//     fail. Exponent digits take no separator (`1e_5` is `No digits seen.`).
//  5. Hex floats need a binary exponent, and the missing-exponent message is the
//     dedicated `Floating point parsing: exponent is required for input "X".` — but
//     only when the hex mantissa actually ended where a 'p' was expected:
//     `0x10` and `0x1.8` give that, `0xp1` and `0xg1p0` give `no digits seen`, and
//     `0x1.2.3p1` / `0x1__2p1` give `exponent is required` even though a 'p' is
//     present later, because the walk stops at the second '.' / second '_'.
//  6. An empty operand, "+" alone and "-" alone are `Floating point conversion error
//     for input "X".`; "." and "-." are `no digits seen`.
//  7. Overflow is a `Range error` (`1e1000`, `0x1p10000`); underflow is not (`1e-500`
//     and `0x1p-10000` are 0).
inline double to_double(const std::string& s) {
    const size_t n = s.size();
    // (6) no operand at all.
    if (n == 0) detail::conv_fail("Floating point conversion error for input \"\".");

    size_t i = 0;
    bool neg = false;
    const bool has_sign = (s[0] == '+' || s[0] == '-');
    if (has_sign) {
        neg = (s[0] == '-');
        if (n == 1) detail::conv_fail("Floating point conversion error for input \"" + s + "\".");
        i = 1;
    }

    const char lead = s[i];
    if (lead == 'n' || lead == 'N') {
        // (1,2) nan is reachable behind either sign.
        if (i + 3 <= n) {
            const std::string w = s.substr(i, 3);
            if (std::tolower(static_cast<unsigned char>(w[0])) == 'n' &&
                std::tolower(static_cast<unsigned char>(w[1])) == 'a' &&
                std::tolower(static_cast<unsigned char>(w[2])) == 'n') {
                if (n > i + 3)
                    detail::conv_fail(detail::unexpected_char_at(s, i + 3, "double"));
                return std::numeric_limits<double>::quiet_NaN();
            }
        }
        // (2) started like the word, did not complete it.
        detail::conv_fail("error converting input to floating point for input \"" + s + "\".");
    }
    if ((lead == 'i' || lead == 'I') && !(has_sign && s[0] == '+')) {
        // (1,2) inf behind '-' or behind nothing, never behind '+'.
        if (i + 3 <= n) {
            const std::string w = s.substr(i, 3);
            if (std::tolower(static_cast<unsigned char>(w[0])) == 'i' &&
                std::tolower(static_cast<unsigned char>(w[1])) == 'n' &&
                std::tolower(static_cast<unsigned char>(w[2])) == 'f') {
                if (n > i + 3)
                    detail::conv_fail(detail::unexpected_char_at(s, i + 3, "double"));
                return neg ? -std::numeric_limits<double>::infinity()
                           : std::numeric_limits<double>::infinity();
            }
        }
        detail::conv_fail("error converting input to floating point for input \"" + s + "\".");
    }

    // (2) any other non-digit opener, and every whitespace, is "no digits seen".
    if (!(std::isdigit(static_cast<unsigned char>(lead)) || lead == '.')) {
        detail::conv_fail("no digits seen for input \"" + s + "\".");
    }

    // Hex float: 0x / 0X, then the mantissa, then a mandatory binary exponent.
    bool is_hex = false;
    if (lead == '0' && i + 1 < n && (s[i + 1] == 'x' || s[i + 1] == 'X')) {
        if (i + 2 >= n ||
            !(std::isxdigit(static_cast<unsigned char>(s[i + 2])) || s[i + 2] == '.')) {
            // (5) "0x", "0xp1", "0xg1p0", "0x_10p1": no hex mantissa at all.
            detail::conv_fail("no digits seen for input \"" + s + "\".");
        }
        is_hex = true;
        i += 2;
    }

    // Walk the mantissa: hex or decimal digits, one optional '.', and '_' as a digit
    // separator. (4) Measured separator rule: D accepts '_' only directly after a
    // digit, so `1_2` and `1_e5` convert but `1__2`, `1._5`, `_1` and `1e5_` fail;
    // the flag (rather than "the last character was a digit") is what rejects the
    // second underscore of `1__`.
    //
    // In a hex mantissa 'p'/'P' terminate the digits even though they are hex digits,
    // because they are the binary-exponent marker: `0x1p1p1` reports
    // `Unexpected 'p'`, which is only reachable if the first 'p' ended the mantissa.
    bool saw_digit = false, saw_dot = false, prev_digit = false;
    for (; i < n; ++i) {
        const char c = s[i];
        if (is_hex && (c == 'p' || c == 'P')) break;
        const bool is_dig = is_hex ? std::isxdigit(static_cast<unsigned char>(c)) != 0
                                   : std::isdigit(static_cast<unsigned char>(c)) != 0;
        if (is_dig) { saw_digit = true; prev_digit = true; continue; }
        if (c == '.' && !saw_dot) { saw_dot = true; prev_digit = false; continue; }
        if (c == '_' && prev_digit) { prev_digit = false; continue; }
        break;
    }
    if (!saw_digit) {
        // (6) "." and "-." land here; a hex mantissa of only "." was caught above.
        detail::conv_fail("no digits seen for input \"" + s + "\".");
    }

    // Exponent.
    if (i < n && (s[i] == 'e' || s[i] == 'E' || (is_hex && (s[i] == 'p' || s[i] == 'P')))) {
        ++i;
        if (i >= n) detail::conv_fail("Unexpected end of input");   // "1e", "0x10p"
        if (s[i] == '+' || s[i] == '-') {
            ++i;
            if (i >= n) detail::conv_fail("No digits seen.");       // (3) "1e+", "0x10p-"
        }
        if (i >= n || !std::isdigit(static_cast<unsigned char>(s[i]))) {
            detail::conv_fail("No digits seen.");                   // "1e 2", "1.5eX", "1e_5"
        }
        // Exponent digits take no separator: `1e_5` is caught above, `1e5_` falls out
        // of this loop and is reported as `Unexpected '_'`.
        while (i < n && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
    } else if (is_hex) {
        // (5) the hex mantissa ended somewhere a 'p' was expected.
        detail::conv_fail("Floating point parsing: exponent is required for input \"" + s + "\".");
    }

    if (i < n) detail::conv_fail(detail::unexpected_char_at(s, i, "double"));

    // Values come from strtod on the operand with its digit separators removed and
    // the sign applied here, which is the same reduction Phobos' own parser makes.
    std::string clean;
    clean.reserve(n);
    for (size_t k = has_sign ? 1 : 0; k < n; ++k) {
        if (s[k] != '_') clean += s[k];
    }
    errno = 0;
    char* endp = nullptr;
    const double v = std::strtod(clean.c_str(), &endp);
    // (7) overflow only; underflow returns 0 (`1e-500` and `0x1p-10000` are both 0).
    if (errno == ERANGE && std::isinf(v)) detail::conv_fail("Range error");
    return neg ? -v : v;
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
