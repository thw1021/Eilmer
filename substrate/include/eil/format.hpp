#pragma once
// eil/format.hpp — D-compatible text rendering.
//
// Why this exists: the regression gate compares normalized solver logs
// byte-for-byte, so every `writefln`/`writeln`/`to!string` in the solver has to
// reproduce D's output exactly, not "close enough". The rules below were
// measured against Phobos by tools/fmt_probe.d (corpus in tools/fmt_corpus.txt)
// rather than assumed:
//
//   * to!string(double) is %.6g, NOT shortest round-trip: 3.141592653589793
//     prints "3.14159" and 1.0/3.0 prints "0.333333". std::to_chars would have
//     produced "3.141592653589793" and broken every log comparison.
//   * writeln(a, b, c) concatenates with no separator and appends \n.
//   * %s on a numeric array yields "[1, 2.5, -3]" (elements through to!string,
//     comma+space, empty array is "[]").
//   * %g on an *integer* argument prints the decimal integer ("1000000"), where
//     C's %.6g would print "1e+06".
//   * inf/nan render lowercase, as glibc does.
//
// Specs the corpus does not pin down raise format_error at run time instead of
// silently producing different text.

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#include "eil/number.hpp"

namespace eil {

class format_error : public std::runtime_error {
  public:
    explicit format_error(const std::string& w) : std::runtime_error(w) {}
};

namespace detail {

// D renders NaN without a sign ("nan", never "-nan"), while glibc's printf
// emits "-nan" for a negative quiet NaN, which sqrt(-1) and 0.0/0.0 both are.
inline std::string non_finite_text(double v) {
    // Only infinities are overridden; NaN keeps whatever sign the producing
    // operation left in it, because D renders "nan" and "-nan" distinctly.
    return std::isnan(v) ? std::string(std::signbit(v) ? "-nan" : "nan")
                        : (v < 0.0 ? "-inf" : "inf");
}

inline std::string from_double(double v) {
    if (std::isinf(v)) return non_finite_text(v);
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.6g", v);
    return buf;
}

template <typename T> std::string integral_to_string(T v) {
    char buf[32];
    if constexpr (std::is_same_v<T, bool>) {
        return v ? "true" : "false";
    } else if constexpr (std::is_same_v<T, unsigned long long>) {
        std::snprintf(buf, sizeof buf, "%llu", v);
    } else if constexpr (std::is_same_v<T, unsigned long>) {
        std::snprintf(buf, sizeof buf, "%lu", v);
    } else if constexpr (std::is_same_v<T, unsigned int>) {
        std::snprintf(buf, sizeof buf, "%u", v);
    } else if constexpr (std::is_same_v<T, long long>) {
        std::snprintf(buf, sizeof buf, "%lld", v);
    } else if constexpr (std::is_same_v<T, long>) {
        std::snprintf(buf, sizeof buf, "%ld", v);
    } else if constexpr (std::is_signed_v<T>) {
        std::snprintf(buf, sizeof buf, "%d", static_cast<int>(v));
    } else {
        std::snprintf(buf, sizeof buf, "%u", static_cast<unsigned>(v));
    }
    return buf;
}

// One generic renderer, so no header has to be included in a particular order.
// Rules are those measured against Phobos (see tools/fmt_corpus.txt).
namespace td {

// "Indexable container" in the broadest sense: D's arrays expose length(), std
// containers expose size(); ported code mixes both.
template <typename C, typename = void> struct has_length : std::false_type {};
template <typename C>
struct has_length<C, std::void_t<decltype(std::declval<const C&>().length())>>
    : std::true_type {};

template <typename C, typename = void> struct has_indexing : std::false_type {};
template <typename C>
struct has_indexing<C, std::void_t<decltype(std::declval<const C&>()[size_t(0)])>>
    : std::true_type {};

template <typename C, typename = void> struct has_size : std::false_type {};
template <typename C>
struct has_size<C, std::void_t<decltype(std::declval<const C&>().size())>> : std::true_type {};

template <typename C>
struct is_range_like : std::bool_constant<(has_length<C>::value || has_size<C>::value)
                                              && has_indexing<C>::value> {};

template <typename C> size_t range_length(const C& c) {
    if constexpr (has_length<C>::value) return c.length();
    else return c.size();
}

template <typename T> struct is_string_like
    : std::is_convertible<T, std::string_view> {};

template <typename T> std::string to_string(const T& v) {
    if constexpr (std::is_same_v<T, bool>) {
        return v ? "true" : "false";
    } else if constexpr (std::is_floating_point_v<T>) {
        return from_double(static_cast<double>(v));
    } else if constexpr (std::is_integral_v<T>) {
        return integral_to_string(v);
    } else if constexpr (is_string_like<T>::value) {
        // A bare string is printed without quotes ("aa_int_key seven").
        return std::string(v);
    } else if constexpr (is_range_like<T>::value) {
        // Containers bracket, separate with ", ", and quote string elements
        // ("[\"alpha\", \"beta\"]") while leaving numbers bare ("[1, 2]").
        std::string out = "[";
        const size_t n = range_length(v);
        for (size_t i = 0; i < n; ++i) {
            if (i) out += ", ";
            using E = std::decay_t<decltype(v[i])>;
            if constexpr (is_string_like<E>::value) out += "\"" + std::string(v[i]) + "\"";
            else out += to_string(v[i]);
        }
        return out + "]";
    } else {
        static_assert(sizeof(T) == 0, "eil::to_string: no conversion for this type yet");
    }
}

} // namespace td

using td::to_string;

inline std::string incompat_float(const std::string& spec) {
    return "incompatible format character for floating point argument: %" + spec;
}
inline std::string orphan_args(size_t lo, size_t hi) {
    // Measured Phobos text for `format("%%", v)`: "Orphan format arguments: args[0..1]"
    return "Orphan format arguments: args[" + std::to_string(lo) + ".." + std::to_string(hi) + "]";
}

inline std::string as_integer_from_double(double v) {
    return integral_to_string(static_cast<long long>(v));
}

inline std::string tolower_spec(char c) { return std::string(1, static_cast<char>(std::tolower(c))); }

// Render one argument under one conversion spec.
template <typename T>
std::string emit(char spec, const std::string& flags, int width, int prec, const T& v) {
    std::string body;
    constexpr bool is_float = std::is_floating_point_v<T>;
    constexpr bool is_bool = std::is_same_v<std::decay_t<T>, bool>;
    // Measured: %d/%g/%x on a bool print 1/0 ("%s" still prints true/false).
    constexpr bool as_int_spec = std::is_integral_v<T> && !is_bool;
    if constexpr (is_bool) {
        const int b = v ? 1 : 0;
        switch (spec) {
          case 's': return to_string(v);
          case 'd': case 'g': case 'e': case 'f': case 'G': case 'E': case 'F':
            return integral_to_string(b);
          case 'x': return integral_to_string(b);
          default: throw format_error(std::string("unimplemented conversion spec ") + spec);
        }
    }
    constexpr bool is_int = as_int_spec;
    switch (spec) {
      case 's':
        body = td::to_string(v);
        break;
      case 'd':
        // Measured: Phobos refuses an integer spec on a floating argument with
        //   incompatible format character for floating point argument: %d
        if constexpr (is_float) throw format_error(incompat_float("d"));
        else body = td::to_string(v);
        break;
      case 'x':
        if constexpr (is_float) throw format_error(incompat_float("x"));
        else if constexpr (is_int) {
            char buf[32];
            std::snprintf(buf, sizeof buf, "%x", static_cast<unsigned>(v));
            body = buf;
        } else throw format_error("incompatible format character for argument: %x");
        break;
      case 'g': case 'e': case 'f': case 'G': case 'E': case 'F':
        if constexpr (is_float) {
            char cf[32];
            std::snprintf(cf, sizeof cf, "%%.%d%c", prec < 0 ? 6 : prec,
                          static_cast<char>(std::tolower(spec)));
            // %.6f of 1e300 expands to ~309 integer digits.
            char out[600];
            if (!std::isfinite(static_cast<double>(v))) {
                body = non_finite_text(static_cast<double>(v));
                // Width still applies to the substituted text.
            } else {
                std::snprintf(out, sizeof out, cf, static_cast<double>(v));
                body = out;
            }
        } else if constexpr (is_int) {
            // Measured: D prints an integer under %g as a plain integer.
            body = integral_to_string(v);
        } else throw format_error("numeric spec given a non-numeric argument");
        break;
      case 'a': case 'A': {
        if constexpr (is_float) {
            char cf[32];
            if (prec < 0) std::snprintf(cf, sizeof cf, "%%%c", spec);
            else std::snprintf(cf, sizeof cf, "%%.%d%c", prec, spec);
            char out[600];
            std::snprintf(out, sizeof out, cf, static_cast<double>(v));
            body = out;
        } else throw format_error("%a needs a floating argument");
        break;
      }
      default:
        throw format_error(std::string("unimplemented conversion spec ") + spec);
    }
    if (width > 0 && static_cast<int>(body.size()) < width) {
        const int pad = width - static_cast<int>(body.size());
        if (flags.find('-') != std::string::npos) {
            body.append(pad, ' ');
        } else if (flags.find('0') != std::string::npos && !body.empty() &&
                   (std::isdigit(static_cast<unsigned char>(body[0])) || body[0] == '-' ||
                    body[0] == '+')) {
            const size_t at = (body[0] == '-' || body[0] == '+') ? 1 : 0;
            body.insert(at, pad, '0');
        } else {
            body.insert(0, pad, ' ');
        }
    }
    return body;
}

template <typename Tuple, size_t I>
std::string emit_at(const Tuple& t, char spec, const std::string& flags, int width, int prec) {
    return emit(spec, flags, width, prec, std::get<I>(t));
}

template <typename Tuple, size_t... Is>
std::string pick_apply(const Tuple& t, char spec, const std::string& flags, int width, int prec,
                       size_t idx, std::index_sequence<Is...>) {
    std::string result;
    bool found = false;
    const int dummy[] = {0, (Is == idx
                                 ? (result = emit_at<Tuple, Is>(t, spec, flags, width, prec),
                                    found = true, 0)
                                 : 0)...};
    (void)dummy;
    if (!found) throw format_error("too few arguments for format string");
    return result;
}

} // namespace detail

// Ports call eil::to_string(); the implementation lives in detail::td so the
// generic template can be found without include-order concerns.
using detail::td::to_string;

// std.format.format for the subset the solver actually uses.
template <typename... Args>
std::string format(const std::string& fmt, const Args&... args) {
    const std::tuple<const Args&...> t{args...};
    std::string out;
    size_t i = 0;
    size_t argidx = 0;
    while (i < fmt.size()) {
        const char c = fmt[i];
        if (c != '%') {
            out += c;
            ++i;
            continue;
        }
        ++i;
        if (i < fmt.size() && fmt[i] == '%') {
            out += '%';
            ++i;
            continue;
        }
        std::string flags;
        while (i < fmt.size() && std::strchr("-+ #0", fmt[i])) flags += fmt[i++];
        int width = -1;
        while (i < fmt.size() && std::isdigit(static_cast<unsigned char>(fmt[i])))
            width = width < 0 ? fmt[i] - '0' : width * 10 + (fmt[i] - '0'), ++i;
        int prec = -1;
        if (i < fmt.size() && fmt[i] == '.') {
            ++i;
            prec = 0;
            while (i < fmt.size() && std::isdigit(static_cast<unsigned char>(fmt[i])))
                prec = prec * 10 + (fmt[i] - '0'), ++i;
        }
        if (i >= fmt.size()) throw format_error("format string ends inside a conversion");
        const char spec = fmt[i++];
        out += detail::pick_apply(t, spec, flags, width, prec, argidx,
                                  std::index_sequence_for<Args...>{});
        ++argidx;
    }
    if (argidx != sizeof...(Args)) {
        throw format_error(sizeof...(Args) > argidx ? detail::orphan_args(argidx, sizeof...(Args))
                                                   : "too few arguments for format string");
    }
    return out;
}

// std.stdio.writeln: arguments concatenated, no separators, newline appended.
template <typename... Args> void writeln(const Args&... args) {
    (std::printf("%s", detail::td::to_string(args).c_str()), ...);
    std::putchar('\n');
}
template <typename... Args> void write(const Args&... args) {
    (std::printf("%s", detail::td::to_string(args).c_str()), ...);
}
// std.stdio.writefln
template <typename... Args> void writefln(const std::string& fmt, const Args&... args) {
    const std::string s = format(fmt, args...);
    std::printf("%s\n", s.c_str());
}
// std.conv.text
template <typename... Args> std::string text(const Args&... args) {
    return (std::string() + ... + detail::td::to_string(args));
}

} // namespace eil
