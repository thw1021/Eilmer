// geom/gpath/svgpath.d -> src/geom/gpath/svgpath.cpp (module geom.gpath.svgpath).
//
// Definitions in D source order: the two constructors (D:27-47), the four overrides
// (D:49-65), then the parser (D:81-199). The command bodies copy D's switch cases in
// D's order, with the same operand order in every coordinate expression
// (D:114 (x1+xtrans)*xscale is the translate-then-scale rule of D:72-75; the relative
// commands multiply the offset by the scale BEFORE adding it to the current point,
// which is not the same arithmetic and is preserved).
//
// See the header for the non-1:1 notes; the two that show up in the code below are
// the parse-before-super reordering and the empty-command RangeError.
#include "geom/gpath/svgpath.hpp"

#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/number.hpp"
#include "geom/gpath/bezier.hpp"
#include "geom/gpath/line.hpp"
#include "geom/gpath/path.hpp"
#include "geom/gpath/polyline.hpp"

namespace eil {
namespace geom {

namespace {

const char* const white_chars = " \t\n\v\f\r";

bool is_white(char c) { return std::strchr(white_chars, c) != nullptr; }

// D's std.string.strip for the ASCII whitespace set std.ascii.isWhite uses.
std::string strip(const std::string& s) {
    size_t b = 0;
    while (b < s.size() && is_white(s[b])) ++b;
    size_t e = s.size();
    while (e > b && is_white(s[e - 1])) --e;
    return s.substr(b, e - b);
}

// D's std.array.split(s, ";"): n separators give n+1 pieces, but an EMPTY subject
// gives no pieces at all (measured: split.empty has len=0, while split.trailing
// keeps the empty piece after the final ';').
std::vector<std::string> split_semicolons(const std::string& s) {
    std::vector<std::string> out;
    if (s.empty()) return out;
    size_t start = 0;
    for (;;) {
        size_t sep = s.find(';', start);
        if (sep == std::string::npos) {
            out.push_back(s.substr(start));
            break;
        }
        out.push_back(s.substr(start, sep - start));
        start = sep + 1;
    }
    return out;
}

} // namespace

namespace svgpath_detail {

namespace {

// Phobos' three input-side messages, quoted exactly (they are printed by any caller
// that catches the exception, and the probe corpus compares them).
std::string msg_no_digits(const std::string& rest) {
    return "no digits seen for input \"" + rest + "\".";
}
std::string msg_missing_literal(char c) {
    return "parseToFormatSpec: Cannot find character '" + std::string(1, c) +
           "' in the input string.";
}
std::string msg_hex_needs_exponent(const std::string& rest) {
    return "Floating point parsing: exponent is required for input \"" + rest + "\".";
}

bool word_at(const std::string& s, size_t i, const char* w) {
    size_t n = std::strlen(w);
    if (i + n > s.size()) return false;
    for (size_t k = 0; k < n; ++k) {
        if (std::tolower(static_cast<unsigned char>(s[i + k])) != w[k]) return false;
    }
    return true;
}

bool is_digit(char c) { return c >= '0' && c <= '9'; }
bool is_xdigit(char c) {
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

// One number at `pos`, in the grammar std.format's %g conversion uses on input.
// Everything here is pinned by the fmt.* rows of
// baselines/e587b235/probes/geom_gpath_svgpath.txt:
//   * an optional sign, then inf/nan (case-insensitive) OR a numeral;
//   * the numeral is [0-9]* with at most one '.', then an optional e/E exponent;
//   * a sign and a dot are enough to start a number (".5" and "-.5" parse), but a
//     leading space is not, and the message quotes the whole remaining input;
//   * "1.0e" is 'No digits seen.' (the exponent clause found no digits);
//   * "0x10" is 'Floating point parsing: exponent is required ...', while "0x1p3"
//     parses as 8;
//   * a numeral that overflows is 'Range error' (an Exception), while the word "inf"
//     is not; "1e-400" underflows to (signed) zero without complaining.
void scan_number(const std::string& in, size_t& pos, double& value) {
    const size_t n = in.size();
    size_t i = pos;
    size_t j = i;
    if (j < n && (in[j] == '+' || in[j] == '-')) ++j;

    if (word_at(in, j, "inf") || word_at(in, j, "nan")) {
        const size_t len = word_at(in, j, "inf") ? 3 : 3;
        const std::string token = in.substr(i, (j + len) - i);
        value = std::strtod(token.c_str(), nullptr);
        pos = j + len;
        return;
    }

    if (j + 1 < n && in[j] == '0' && (in[j + 1] == 'x' || in[j + 1] == 'X')) {
        size_t k = j + 2;
        bool any_digits = false;
        bool seen_dot = false;
        while (k < n && (is_xdigit(in[k]) || (in[k] == '.' && !seen_dot))) {
            if (in[k] == '.') seen_dot = true;
            if (is_xdigit(in[k])) any_digits = true;
            ++k;
        }
        if (any_digits && k < n && (in[k] == 'p' || in[k] == 'P')) {
            size_t e = k + 1;
            if (e < n && (in[e] == '+' || in[e] == '-')) ++e;
            bool any_exp = false;
            while (e < n && is_digit(in[e])) {
                any_exp = true;
                ++e;
            }
            if (!any_exp) throw eil::ExceptionBase("No digits seen.", __FILE__, __LINE__);
            value = std::strtod(in.substr(i, e - i).c_str(), nullptr);
            pos = e;
            return;
        }
        throw eil::ExceptionBase(msg_hex_needs_exponent(in.substr(i)), __FILE__, __LINE__);
    }

    size_t k = j;
    bool any_digits = false;
    bool seen_dot = false;
    while (k < n && (is_digit(in[k]) || (in[k] == '.' && !seen_dot))) {
        if (in[k] == '.') seen_dot = true;
        if (is_digit(in[k])) any_digits = true;
        ++k;
    }
    if (!any_digits) throw eil::ExceptionBase(msg_no_digits(in.substr(i)), __FILE__, __LINE__);
    if (k < n && (in[k] == 'e' || in[k] == 'E')) {
        ++k;
        if (k < n && (in[k] == '+' || in[k] == '-')) ++k;
        if (k >= n || !is_digit(in[k])) {
            throw eil::ExceptionBase("No digits seen.", __FILE__, __LINE__);
        }
        while (k < n && is_digit(in[k])) ++k;
    }
    const std::string token = in.substr(i, k - i);
    value = std::strtod(token.c_str(), nullptr);
    if (std::isinf(value)) throw eil::ExceptionBase("Range error", __FILE__, __LINE__);
    pos = k;
}

} // namespace

void formatted_read_g(const std::string& in, const char* fmt,
                      const std::vector<double*>& targets) {
    const size_t nf = std::strlen(fmt);
    size_t ti = 0;
    size_t pos = 0;
    for (size_t f = 0; f < nf;) {
        const char c = fmt[f];
        if (c == '%') {
            // svgpath.d:108 imports only for the %g conversions; every format string
            // it uses is %g and literals, so anything else is a porting mistake.
            eil_assert(f + 1 < nf && fmt[f + 1] == 'g');
            f += 2;
            // The format has a conversion but the call has no argument for it. Measured
            // on the fmt.litafterinput row of geom_gpath_svgpath.txt: Phobos reports
            // 'Trailing characters in formattedRead format string' as a FormatException
            // (an Exception, not an Error) BEFORE it touches the input, since
            // "1.0" with "%g,x" and no arguments does not report 'no digits seen'. No
            // svgpath.d call site can reach this (D:113-186 always hand over exactly one
            // pointer per %g), but the probe measures it, so it is a message and not an
            // assertion.
            if (ti >= targets.size()) {
                throw eil::ExceptionBase("Trailing characters in formattedRead format "
                                         "string",
                                         __FILE__, __LINE__);
            }
            if (pos >= in.size()) return; // D stops silently at end of input (fmt.empty)
            double v = 0.0;
            scan_number(in, pos, v);
            *(targets[ti]) = v;
            ++ti;
        } else if (c == ' ') {
            // D's ' ' matches any run of whitespace, including none (fmt.spaceskip,
            // fmt.nosep and fmt.spacelit all measure this branch).
            ++f;
            while (pos < in.size() && is_white(in[pos])) ++pos;
        } else {
            if (pos >= in.size() || in[pos] != c) {
                throw eil::ExceptionBase(msg_missing_literal(c), __FILE__, __LINE__);
            }
            ++pos;
            ++f;
        }
    }
    // D keeps pulling input for arguments that have no conversion left in the format
    // (the shape of fmt_case_orphans in the probe). Measured: with six pointers and
    // "%g,%g", "3.0,3.0" fills two and stops silently at end of input, leaving the
    // other four at .init (fmt.orphans.M), while "2.0,2.0 3.0,4.0" goes on to report
    // 'no digits seen for input " 3.0,4.0".' — an orphan is read as a %g at the
    // current position, with no whitespace skipped, exactly like a conversion.
    while (ti < targets.size()) {
        if (pos >= in.size()) return;
        double v = 0.0;
        scan_number(in, pos, v);
        *(targets[ti]) = v;
        ++ti;
    }
    // Trailing input past the last argument is ignored (fmt.trailing_input: an 'L'
    // with extra coordinates uses only the first pair).
} // end formatted_read_g()

} // namespace svgpath_detail

// ---------------------------------------------------------------------------
// D:68-79, held by the parse state rather than by the half-constructed object so
// that D's "parse first, then call super()" order (D:32-38) is preserved.
struct SVGPath::Parsed {
    std::string txt; // D:68 the original text specification
    Vector3 p0;      // D:69 Starting point.
    Vector3 cp;      // D:70 Current point.
    // D:71-75 Apply the translation first, to absolute coordinates.
    //           Apply the scales second, to both absolute and relative coordinates.
    double xtrans = eil::d_init<number>();
    double ytrans = eil::d_init<number>();
    double xscale = eil::d_init<number>();
    double yscale = eil::d_init<number>();
    // D:76-77 A place to store the generated gpath segments.
    PathList segments;
    bool closed = false; // D:78
    double tolerance = eil::d_init<number>(); // D:79

    // D:81-91.
    void interpret_svg_path(std::string txt) {
        txt = strip(txt);
        for (std::string cmdStr : split_semicolons(txt)) {
            cmdStr = strip(cmdStr);
            // D:86 `char cmd = cmdStr[0]` on an empty piece is a RangeError in D
            // ("index [0] is out of bounds for array of length 0", measured on the
            // svg.err.emptysegment row). Reading std::string::operator[](0) of an
            // empty string would quietly yield '\0' instead, so the check is explicit.
            if (cmdStr.empty()) {
                throw eil::RangeError(0, 0, __FILE__, __LINE__);
            }
            char cmd = cmdStr[0];
            std::string argStr = "";
            if (cmdStr.length() > 1) { argStr = cmdStr.substr(1); } // D:88 cmdStr[1..$]
            do_svg_path_command(cmd, argStr);
        }
    } // end interpret_svg_path()

    // D:93-199.
    void do_svg_path_command(char cmd, std::string argStr) {
        // We will interpret a subset of the SVG path commands
        // that seems to be enough to follow glyph outlines
        // as written by Inkscape.
        //
        // My reference has been:
        // J. David Eisenberg & Amelia Bellamy-Royds
        // SVG Essentials
        // 2nd Edition O'Reilly
        //
        argStr = strip(argStr);
        // import std.stdio: writeln;
        // writeln("cmd=", cmd, " argStr=", argStr);
        //
        // D:109-110. Uninitialised locals are .init in D, i.e. NaN, and the empty
        // remainder case (svg.init) stores exactly those NaNs into the path.
        double x1 = eil::d_init<number>(), y1 = eil::d_init<number>(),
               x2 = eil::d_init<number>(), y2 = eil::d_init<number>(),
               x3 = eil::d_init<number>(), y3 = eil::d_init<number>();
        Vector3 p1, p2, p3;
        switch (cmd) {
        case 'M': // Move absolute, resets starting point.
            svgpath_detail::formatted_read_g(argStr, "%g,%g", {&x1, &y1});
            p0.set((x1 + xtrans) * xscale, (y1 + ytrans) * yscale);
            cp.set(p0);
            break;
        case 'm': // Move relative to current position, resets starting point.
            svgpath_detail::formatted_read_g(argStr, "%g,%g", {&x1, &y1});
            p0.set(cp.x + x1 * xscale, cp.y + y1 * yscale);
            cp.set(p0);
            break;
        case 'L':
            svgpath_detail::formatted_read_g(argStr, "%g,%g", {&x1, &y1});
            p1.set((x1 + xtrans) * xscale, (y1 + ytrans) * yscale);
            segments.append(std::make_shared<Line>(cp, p1));
            cp.set(p1);
            break;
        case 'l':
            svgpath_detail::formatted_read_g(argStr, "%g,%g", {&x1, &y1});
            p1.set(cp.x + x1 * xscale, cp.y + y1 * yscale);
            segments.append(std::make_shared<Line>(cp, p1));
            cp.set(p1);
            break;
        case 'H':
            svgpath_detail::formatted_read_g(argStr, "%g", {&x1});
            // D:136 writes to!number(...). With number == double (nm/number.hpp) that
            // conversion is the identity — std.conv does not round-trip a value whose
            // type already matches — so the expression is carried unchanged rather than
            // through eil::to<double>, which would go via the %.6g to_string().
            p1.set((x1 + xtrans) * xscale, cp.y);
            segments.append(std::make_shared<Line>(cp, p1));
            cp.set(p1);
            break;
        case 'h':
            svgpath_detail::formatted_read_g(argStr, "%g", {&x1});
            p1.set(cp.x + x1 * xscale, cp.y);
            segments.append(std::make_shared<Line>(cp, p1));
            cp.set(p1);
            break;
        case 'V':
            svgpath_detail::formatted_read_g(argStr, "%g", {&y1});
            p1.set(cp.x, (y1 + ytrans) * yscale); // D:148 to!number is the identity
            segments.append(std::make_shared<Line>(cp, p1));
            cp.set(p1);
            break;
        case 'v':
            svgpath_detail::formatted_read_g(argStr, "%g", {&y1});
            p1.set(cp.x, cp.y + y1 * yscale);
            segments.append(std::make_shared<Line>(cp, p1));
            cp.set(p1);
            break;
        case 'Q':
            svgpath_detail::formatted_read_g(argStr, "%g,%g %g,%g", {&x1, &y1, &x2, &y2});
            p1.set((x1 + xtrans) * xscale, (y1 + ytrans) * yscale);
            p2.set((x2 + xtrans) * xscale, (y2 + ytrans) * yscale);
            segments.append(std::make_shared<Bezier>(eil::Array<Vector3>{cp, p1, p2}));
            cp.set(p2);
            break;
        case 'q':
            svgpath_detail::formatted_read_g(argStr, "%g,%g %g,%g", {&x1, &y1, &x2, &y2});
            p1.set(cp.x + x1 * xscale, cp.y + y1 * yscale);
            p2.set(cp.x + x2 * xscale, cp.y + y2 * yscale);
            segments.append(std::make_shared<Bezier>(eil::Array<Vector3>{cp, p1, p2}));
            cp.set(p2);
            break;
        case 'C':
            svgpath_detail::formatted_read_g(argStr, "%g,%g %g,%g %g,%g",
                                             {&x1, &y1, &x2, &y2, &x3, &y3});
            p1.set((x1 + xtrans) * xscale, (y1 + ytrans) * yscale);
            p2.set((x2 + xtrans) * xscale, (y2 + ytrans) * yscale);
            p3.set((x3 + xtrans) * xscale, (y3 + ytrans) * yscale);
            segments.append(std::make_shared<Bezier>(eil::Array<Vector3>{cp, p1, p2, p3}));
            cp.set(p3);
            break;
        case 'c':
            svgpath_detail::formatted_read_g(argStr, "%g,%g %g,%g %g,%g",
                                             {&x1, &y1, &x2, &y2, &x3, &y3});
            p1.set(cp.x + x1 * xscale, cp.y + y1 * yscale);
            p2.set(cp.x + x2 * xscale, cp.y + y2 * yscale);
            p3.set(cp.x + x3 * xscale, cp.y + y3 * yscale);
            segments.append(std::make_shared<Bezier>(eil::Array<Vector3>{cp, p1, p2, p3}));
            cp.set(p3);
            break;
        case 'Z':
            p1.set(p0);
            if (distance_between(cp, p1) > tolerance) {
                segments.append(std::make_shared<Line>(cp, p1));
                cp.set(p1);
            }
            closed = true;
            break;
        default:
            throw eil::ExceptionBase("Unknown SVG command: " + std::string(1, cmd),
                                     __FILE__, __LINE__);
        } // end switch
    } // end do_svg_path_command()
}; // end struct SVGPath::Parsed

// ---------------------------------------------------------------------------
// D:27-39. Note that D does NOT store the text it is given (svgpath.d:32-37 assigns
// cp, p0, xtrans .. tolerance and calls interpret_svg_path(txt), where txt is the
// parameter). The field stays null, which is what makes toString() print path="" and
// dup() throw. The parameter names here differ from D's (spec/xt/...) only so the
// mem-init list can tell field from parameter.
SVGPath::SVGPath(const std::string& spec, double xt, double yt, double xs, double ys,
                 double tol)
    : SVGPath(parse(spec, xt, yt, xs, ys, tol), spec, xt, yt, xs, ys, tol) {}

SVGPath::Parsed SVGPath::parse(const std::string& spec, double xt, double yt, double xs,
                               double ys, double tol) {
    Parsed st;
    st.cp.clear(); // D:32 Start with current-point at origin.
    st.p0.clear(); // D:33
    st.xtrans = xt; st.ytrans = yt; // D:34
    st.xscale = xs; st.yscale = ys; // D:35
    st.tolerance = tol;             // D:36
    st.interpret_svg_path(spec);    // D:37
    return st;
}

// D:38 `super(segments, closed, tolerance)`, then the field initialisations D:68-79
// leave at .init. Polyline's constructor dups every segment (polyline.d:37), so the
// base owns copies and this object's shadow `segments` keeps the originals, as in D.
SVGPath::SVGPath(const Parsed& p, const std::string& spec, double xt, double yt, double xs,
                 double ys, double tol)
    : Polyline(p.segments, p.closed, p.tolerance),
      txt(), // D:68 the field is never assigned; string.init is the empty string.
      p0(p.p0),
      cp(p.cp),
      xtrans(xt),
      ytrans(yt),
      xscale(xs),
      yscale(ys),
      segments(p.segments),
      closed(p.closed),
      tolerance(tol) {}

// D:41-47, `this(ref const(SVGPath) other)`. other.txt is empty (see above), so this
// re-parses "" and Polyline's guard throws — measured row svg.copy.
SVGPath::SVGPath(const SVGPath& other)
    : SVGPath(other.txt, other.xtrans, other.ytrans, other.xscale, other.yscale,
              other.tolerance) {}

// D:49-52. Re-parses the stored text, which is empty, so it throws (measured row
// svg.dup). Kept as written: adding the missing assignment would change what dup()
// of a reference object returns.
std::shared_ptr<Path> SVGPath::dup() const {
    return std::make_shared<SVGPath>(txt, xtrans, ytrans, xscale, yscale, tolerance);
}

// D:54-60.
std::string SVGPath::toString() const {
    return "SVGPath(path=\"" + txt + "\"" + eil::format(", xtrans=%g, ytrans=%g", xtrans, ytrans) +
           eil::format(", xscale=%g, yscale=%g", xscale, yscale) +
           eil::format(", tolerance=%g)", tolerance);
}

// D:62-65.
std::string SVGPath::classString() const { return "SVGPath"; }

} // namespace geom
} // namespace eil
