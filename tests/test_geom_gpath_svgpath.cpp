// tests/test_geom_gpath_svgpath.cpp
//
// Two layers, mirroring tests/test_geom_gpath_xspline.cpp:
//
//  A. the `unittest` block of geom/gpath/svgpath.d (D:204-228) ported assertion for
//     assertion, in source order -- the rect, the quadratic and the quarter-circle
//     cubic, each evaluated at t=0.5 under approxEqualVectors' own defaults.
//
//  B. a line-for-line replay of probes/geom/gpath/probe_svgpath.d against
//     baselines/e587b235/probes/geom_gpath_svgpath.txt: 818 lines, 2802 printed
//     quantities. The groups are the input-side %g grammar (fmt.*, 50 lines), the
//     strip/split pair (split.*, 15 lines), the three unittest paths, the four
//     command/transform/tolerance paths, the nine error paths (one of which,
//     svg.err.trailing, builds), the empty-remainder path (svg.init.*, 76 lines) and the
//     two copy paths (svg.dup, svg.copy).
//
// The three things this corpus exists to pin are the three the probe header lists as
// measured rather than assumed, and all three are reproduced here unchanged:
//
//  1. %g ON INPUT is not scanf. It never skips leading whitespace (fmt.leadws,
//     fmt.aftercomma, fmt.crlf all report 'no digits seen for input "<tail>".' with the
//     tail quoted from where the conversion started), a space *in the format* eats any
//     run of whitespace in the input (fmt.spacelit, fmt.tab, and fmt.nosep -- where
//     "2.0,2.00.0,2.0" legitimately parses as 2, 2, 0, 2 because the second numeral
//     stops at its second dot and the third conversion starts at ".0"), a missing
//     literal or input exhausted at a literal throws
//     'parseToFormatSpec: Cannot find character ',' in the input string.' (fmt.beforecomma,
//     fmt.short, fmt.twodots, fmt.junkafter), input exhausted at a conversion returns
//     SILENTLY and leaves that target and every later one at double.init (fmt.empty,
//     fmt.trailingcomma), a hex literal without its exponent throws
//     'Floating point parsing: exponent is required for input "0x10,2".' (fmt.hex, while
//     fmt.hexp's "0x1p3" parses as 8), an exponent with no digits throws 'No digits
//     seen.' (fmt.badexp), and overflow reports 'Range error' -- as an Exception, not an
//     Error (fmt.overflow). Rows 1-50 of the baseline are that table; the reader that
//     answers them is eil::geom::svgpath_detail::formatted_read_g.
//  2. An EMPTY remainder leaves its target untouched, so "M" with no coordinates stores
//     double.init == NaN into p0 and every downstream number of the path is NaN
//     (svg.init.*, 76 lines: length, opCall, dpdt, d2pdt2, plen, pfl, cd, the one Line's
//     two endpoints and t_values[0]). Reproduced, not sanitised.
//  3. svgpath.d:32-37 never assigns its own private `txt` field, so toString() prints
//     path="" for every SVGPath (every svg.*.str row) and dup()/the copy constructor
//     re-parse the EMPTY string, get no segments, and hit Polyline's guard: rows
//     svg.dup and svg.copy are Error "Polyline() No segments present.", not copies. The
//     port keeps that as written (see svgpath.hpp:24-33), and this test asserts nothing
//     about it beyond replaying the two rows.
//
// Measured against the module as committed, two of the 818 rows were not reachable with
// formatted_read_g's original argument-count handling, both in
// src/geom/gpath/svgpath.cpp (the file this slice owns) and neither reachable from any
// svgpath.d call site, which always passes exactly one pointer per %g:
//   fmt.litafterinput -- "%g,x" with no conversion arguments. D throws an Exception
//     carrying 'Trailing characters in formattedRead format string'; the port had
//     eil_assert(ti < targets.size()), which reported asError=true and the assertion
//     text. Now a real ExceptionBase with D's text.
//   fmt.orphans.extra -- six pointers against two conversions. D keeps pulling input
//     for the orphan arguments and reported 'no digits seen for input " 3.0,4.0".'; the
//     port stopped at the end of the format and reported ok v0=2 v1=2. The orphan reads
//     are now a %g each, with the same silent stop at end of input (fmt.orphans.M).
//
// Run: ./test_geom_gpath_svgpath ../baselines/e587b235/probes/geom_gpath_svgpath.txt
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"
#include "geom/gpath/polyline.hpp"
#include "geom/gpath/bezier.hpp"
#include "geom/gpath/svgpath.hpp"
#include "nm/number.hpp"

#include "gpath3_dump.hpp"

using eil::geom::SVGPath;

namespace {

using namespace eil_test_gpath3;

// probe_svgpath.d:69-78. A newline or carriage return inside an exception message
// would split one observation across two corpus lines, so both sides replace them
// with the escape spelling. Tabs are left as they are.
std::string esc(const std::string& msg) {
    std::string res;
    for (char c : msg) {
        if (c == '\n') res += "\\n";
        else if (c == '\r') res += "\\r";
        else res += c;
    }
    return res;
}

// The one line every error row in this probe prints (probe_svgpath.d:119-121 and
// friends): which side of the D Error/Exception sibling pair caught, plus the
// message. `classify` already splits them the way D's two catch clauses do.
void emit_error(const std::string& tag, const ErrorTaxonomy& t) {
    emitf("%s asError=%s asException=%s msg=%s", tag.c_str(), bools(t.asError),
          bools(t.asException), esc(t.msg).c_str());
}

// -------------------------------------------------------------------- fmt.* rows
// probe_svgpath.d:129-145. `nspec` is the number of pointers handed to
// formattedRead, which D's helper picks out of {1,2,4,6}; anything else (the 3 of
// fmt.spacelit) takes the six-pointer branch, so that row also exercises orphan
// arguments.
void fmt_case(const char* tag, const std::string& src, const std::string& fmt, int nspec) {
    double v[6];
    for (int i = 0; i < 6; ++i) v[i] = eil::d_init<double>();
    std::vector<double*> targets;
    if (nspec == 1) targets = {&v[0]};
    else if (nspec == 2) targets = {&v[0], &v[1]};
    else if (nspec == 4) targets = {&v[0], &v[1], &v[2], &v[3]};
    else targets = {&v[0], &v[1], &v[2], &v[3], &v[4], &v[5]};
    const ErrorTaxonomy t =
        classify([&]() { eil::geom::svgpath_detail::formatted_read_g(src, fmt.c_str(), targets); });
    if (!t.asError && !t.asException) {
        emitf("%s ok v0=%.17g v1=%.17g v2=%.17g v3=%.17g v4=%.17g v5=%.17g", tag, v[0], v[1], v[2],
              v[3], v[4], v[5]);
    } else {
        emit_error(tag, t);
    }
}

// probe_svgpath.d:152-165 — six pointers whatever the format has.
void fmt_case_orphans(const char* tag, const std::string& src, const std::string& fmt) {
    double v[6];
    for (int i = 0; i < 6; ++i) v[i] = eil::d_init<double>();
    std::vector<double*> targets = {&v[0], &v[1], &v[2], &v[3], &v[4], &v[5]};
    const ErrorTaxonomy t =
        classify([&]() { eil::geom::svgpath_detail::formatted_read_g(src, fmt.c_str(), targets); });
    if (!t.asError && !t.asException) {
        emitf("%s ok v0=%.17g v1=%.17g v2=%.17g v3=%.17g v4=%.17g v5=%.17g", tag, v[0], v[1], v[2],
              v[3], v[4], v[5]);
    } else {
        emit_error(tag, t);
    }
}

// probe_svgpath.d:175-185 — literals only, no conversion arguments.
void fmt_case_noarg(const char* tag, const std::string& src, const std::string& fmt) {
    std::vector<double*> targets;
    const ErrorTaxonomy t =
        classify([&]() { eil::geom::svgpath_detail::formatted_read_g(src, fmt.c_str(), targets); });
    if (!t.asError && !t.asException) {
        emitf("%s ok", tag);
    } else {
        emit_error(tag, t);
    }
}

// probe_svgpath.d:188-249 — the five formats svgpath.d uses, then the grammar edges.
void fmt_cases() {
    fmt_case("fmt.M", "3.0,3.0", "%g,%g", 2);
    fmt_case("fmt.H", "4.0", "%g", 1);
    fmt_case("fmt.Q", "2.0,2.0 0.0,2.0", "%g,%g %g,%g", 4);
    fmt_case("fmt.C", "2.0,1.10457 1.10457,2.0 0.0,2.0", "%g,%g %g,%g %g,%g", 6);
    fmt_case("fmt.neg", "-1.0,-2.5", "%g,%g", 2);
    fmt_case("fmt.leadws", "  1.0,2.0", "%g,%g", 2);
    fmt_case("fmt.aftercomma", "1.0, 2.0", "%g,%g", 2);
    fmt_case("fmt.beforecomma", "1.0 ,2.0", "%g,%g", 2);
    fmt_case("fmt.spacelit", "1.0,2.0  ,3.0", "%g,%g %g", 3);
    fmt_case("fmt.nosep", "2.0,2.00.0,2.0", "%g,%g %g,%g", 4);
    fmt_case("fmt.tab", "5.0,5.0\t6.0,4.0", "%g,%g %g,%g", 4);
    fmt_case("fmt.crlf", "1.0,\r\n2.0", "%g,%g", 2);
    fmt_case("fmt.empty", "", "%g,%g", 2);
    fmt_case("fmt.short", "1.0", "%g,%g", 2);
    fmt_case("fmt.trailingcomma", "1.0,", "%g,%g", 2);
    fmt_case_noarg("fmt.litafterempty", "", ",x");
    fmt_case_noarg("fmt.litafterinput", "1.0", "%g,x");
    fmt_case("fmt.spaceonly", " ", "%g", 1);
    fmt_case("fmt.dot", ".5,.5", "%g,%g", 2);
    fmt_case("fmt.explow", "1e3,2", "%g,%g", 2);
    fmt_case("fmt.expup", "1E3,2", "%g,%g", 2);
    fmt_case("fmt.plus", "+1.0,+2.0", "%g,%g", 2);
    fmt_case("fmt.inf", "inf,nan", "%g,%g", 2);
    fmt_case("fmt.garbage", "abc,2.0", "%g,%g", 2);
    fmt_case("fmt.hex", "0x10,2", "%g,%g", 2);
    fmt_case("fmt.badexp", "1.0e,2", "%g,%g", 2);
    fmt_case("fmt.twodots", "1..0,2", "%g,%g", 2);
    fmt_case("fmt.junkafter", "1.0x,2", "%g,%g", 2);
    fmt_case("fmt.overflow", "1.7976931348623157e309,2", "%g,%g", 2);
    fmt_case("fmt.underflow", "1e-400,2", "%g,%g", 2);
    fmt_case("fmt.ints", "123,456", "%g,%g", 2);
    fmt_case("fmt.minf", "inf,2", "%g,%g", 2);
    fmt_case("fmt.neginf", "-inf,2", "%g,%g", 2);
    fmt_case("fmt.nanword", "nan,2", "%g,%g", 2);
    fmt_case("fmt.NANUP", "NAN,2", "%g,%g", 2);
    fmt_case("fmt.dashabc", "-abc,2", "%g,%g", 2);
    fmt_case("fmt.dashdot", "-.5,2", "%g,%g", 2);
    fmt_case("fmt.tail", "1.,2", "%g,%g", 2);
    fmt_case("fmt.commafirst", ",2", "%g,%g", 2);
    fmt_case("fmt.spaceskip", " 1.0,2.0", " %g,%g", 2);
    fmt_case("fmt.twospaceskip", " 1.0,2.0", "  %g,%g", 2);
    fmt_case("fmt.eplus", "1e+3,2", "%g,%g", 2);
    fmt_case("fmt.esign", "1e-3,2", "%g,%g", 2);
    fmt_case("fmt.ebaredigit", "e5,2", "%g,%g", 2);
    fmt_case("fmt.hexp", "0x1p3,2", "%g,%g", 2);
    fmt_case("fmt.veryneg", "-1e-400,2", "%g,%g", 2);
    fmt_case("fmt.dotspace", "1.0, 2.0 ", "%g,%g", 2);
    fmt_case_orphans("fmt.orphans.M", "3.0,3.0", "%g,%g");
    fmt_case_orphans("fmt.orphans.extra", "2.0,2.0 3.0,4.0", "%g,%g");
    fmt_case("fmt.trailing_input", "2.0,2.0 3.0,4.0", "%g,%g", 2);
}

// ------------------------------------------------------------------ the splitter
// probe_svgpath.d:167-172. src.strip() then std.array.split(";"): the ASCII whitespace
// set std.ascii.isWhite uses, and D's split of an EMPTY subject yields no pieces at all
// (split.empty has len=0) while an empty piece between two separators survives
// (split.trailing keeps the piece after the final ';'). interpret_svg_path (svgpath.d
// :83-84) does exactly this, so the svg.* rows are the module's own check of the same
// two primitives.
std::string d_strip(const std::string& s) {
    const char* white = " \t\n\v\f\r";
    size_t b = 0;
    while (b < s.size() && std::strchr(white, s[b]) != nullptr) ++b;
    size_t e = s.size();
    while (e > b && std::strchr(white, s[e - 1]) != nullptr) --e;
    return s.substr(b, e - b);
}

std::vector<std::string> d_split_semicolons(const std::string& s) {
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

void split_case(const char* tag, const std::string& src) {
    const std::vector<std::string> parts = d_split_semicolons(d_strip(src));
    emitf("%s len=%d", tag, static_cast<int>(parts.size()));
    for (size_t i = 0; i < parts.size(); ++i) {
        emitf("%s part[%d]=|%s|", tag, static_cast<int>(i), parts[i].c_str());
    }
}

// probe_svgpath.d:257-261
void splits() {
    split_case("split.empty", "");
    split_case("split.trailing", "M1.0,1.0;");
    split_case("split.leading", ";M1.0,1.0");
    split_case("split.spaces", "  M1.0,1.0  ");
    split_case("split.two", "M3.0,3.0;L4.0,3.0;v1.0;h-1.0;Z");
}

// ------------------------------------------------------------------- path dumping
// probe_svgpath.d:84-105. D's SVGPath shadows `segments` and `closed` with private
// copies (svgpath.d:77-78) and private-in-D is module-private, so the probe reads the
// Polyline base view — which is the array Polyline's constructor actually built, its
// own dups of the parsed segments (polyline.d:37). The C++ shadow (svgpath.hpp:110-111
// mirrors D:77-78) is private here too, so binding to `const Polyline&` reads the same
// object the D probe reads.
void dump_segments(const char* tag, const eil::geom::Polyline& pl) {
    const eil::geom::PathList& segs = pl.segments;
    const eil::Array<double>& tv = pl.t_values;
    emitf("%s nseg=%d", tag, static_cast<int>(segs.length()));
    for (size_t i = 0; i < segs.length(); ++i) {
        const Vector3 a = (*segs[i])(0.0);
        const Vector3 b = (*segs[i])(1.0);
        emitf("%s seg[%d] class=%s p0 x=%.17g y=%.17g z=%.17g p1 x=%.17g y=%.17g z=%.17g", tag,
              static_cast<int>(i), segs[i]->classString().c_str(), a.x, a.y, a.z, b.x, b.y, b.z);
        const eil::geom::Bezier* bz = dynamic_cast<const eil::geom::Bezier*>(segs[i].get());
        if (bz != nullptr) {
            for (size_t j = 0; j < bz->B.length(); ++j) {
                emitf("%s seg[%d] B[%d] x=%.17g y=%.17g z=%.17g", tag, static_cast<int>(i),
                      static_cast<int>(j), bz->B[j].x, bz->B[j].y, bz->B[j].z);
            }
        }
    }
    for (size_t i = 0; i < tv.length(); ++i) {
        emitf("%s t_values[%d]=%.17g", tag, static_cast<int>(i), tv[i]);
    }
}

// probe_svgpath.d:110-123. Every command in do_svg_path_command can fail through
// formattedRead, and both a Z with a gap beyond the tolerance and a path with only a
// Move reach Polyline's guard, so each construction is wrapped. The dump happens after
// the whole path is built (D:114 constructs before D:115 prints), so a throw during
// parsing contributes exactly the one error row.
void build_and_dump(const char* tag, const std::string& src, double xt = 0.0, double yt = 0.0,
                    double xs = 1.0, double ys = 1.0, double tol = 1.0e-10) {
    const ErrorTaxonomy t = classify([&]() {
        auto p = std::make_shared<SVGPath>(src, xt, yt, xs, ys, tol);
        dump_path(tag, *p);
        dump_segments(tag, *p);
        emitf("%s str=%s", tag, p->toString().c_str());
    });
    if (t.asError || t.asException) emit_error(tag, t);
}

// probe_svgpath.d:264-266 — the three paths svgpath.d's own unittest block builds.
void unittest_paths() {
    build_and_dump("svg.rect", "M3.0,3.0;L4.0,3.0;v1.0;h-1.0;Z");
    build_and_dump("svg.quad", "M2.0,0.0;Q2.0,2.0 0.0,2.0");
    build_and_dump("svg.cubic", "M2.0,0.0;C2.0,1.10457 1.10457,2.0 0.0,2.0");
}

// probe_svgpath.d:269-281 — every command in do_svg_path_command in one path, then
// the translation/scale arguments in D's order (translate first for the absolute
// commands, scale for both absolute and relative), then the two tolerance arms of the
// Z case (svgpath.d:188-195).
void commands_and_arguments() {
    build_and_dump("svg.all",
                   "M1.0,1.0;L2.0,1.0;l0.5,0.5;H4.0;h0.25;V3.0;v0.5;m-1.0,0.0;"
                   "Q5.0,5.0 6.0,4.0;q1.0,1.0 -1.0,2.0;C7.0,7.0 8.0,7.0 8.0,9.0;"
                   "c1.0,0.0 1.0,2.0 -1.0,2.0;Z");
    build_and_dump("svg.xform", "M1.0,2.0;L3.0,4.0;h1.0", 0.5, -0.25, 2.0, 3.0);
    build_and_dump("svg.tol2", "M0.0,0.0;L1.0,0.0;L1.0,1.0;Z", 0.0, 0.0, 1.0, 1.0, 2.0);
    build_and_dump("svg.toldefault", "M0.0,0.0;L1.0,0.0;L1.0,1.0;Z");
}

// probe_svgpath.d:284-294 — an unknown command, the formattedRead failures, the two
// paths that reach Polyline's guard, and the trailing-input case where an 'L' uses only
// the first coordinate pair (svgpath.d:122-127).
void error_paths() {
    build_and_dump("svg.err.unknown", "M0.0,0.0;X1.0,2.0");
    build_and_dump("svg.err.nocomma", "M1.0 2.0");
    build_and_dump("svg.err.spaceaftercomma", "M1.0, 2.0");
    build_and_dump("svg.err.toofew", "L1.0");
    build_and_dump("svg.err.emptysegment", "M1.0,1.0;");
    build_and_dump("svg.err.moveonly", "M1.0,1.0");
    build_and_dump("svg.err.empty", "");
    build_and_dump("svg.err.lowerz", "M0.0,0.0;z");
    build_and_dump("svg.err.trailing", "M1.0,1.0;L2.0,2.0 3.0,4.0;Z");
}

// probe_svgpath.d:299 — the empty-remainder case. "M" with no coordinates: D's
// formattedRead returns silently at end of input, so x1/y1 keep double.init (NaN), the
// starting point is NaN and every segment that follows inherits it. Dumped, not
// sanitised (see the probe header, item 2).
void empty_remainder() { build_and_dump("svg.init", "M;L1.0,1.0"); }

// probe_svgpath.d:301-322 — dup() and the copy constructor re-parse the stored text
// (svgpath.d:49-52, :41-47), but the constructor never stores it (D:32-38), so both
// reach Polyline's "No segments present." guard. These rows are Errors, not copies, in
// D as written; the port keeps that.
void copy_paths() {
    auto pth = std::make_shared<SVGPath>("M3.0,3.0;L4.0,3.0;v1.0;h-1.0;Z");
    {
        // D:307-308 auto d = pth.dup(); dump_path("svg.dup", d);
        const ErrorTaxonomy t = classify([&]() {
            auto d = pth->dup();
            dump_path("svg.dup", *d);
        });
        if (t.asError || t.asException) emit_error("svg.dup", t);
    }
    {
        // D:315-316 auto c = new SVGPath(pth); dump_segments("svg.copy", c);
        const ErrorTaxonomy t = classify([&]() {
            auto c = std::make_shared<SVGPath>(*pth);
            dump_segments("svg.copy", *c);
        });
        if (t.asError || t.asException) emit_error("svg.copy", t);
    }
}

// ============================================================================
// A. the `unittest` block of geom/gpath/svgpath.d (D:204-228), assertion for
//    assertion, in source order. Three paths, each evaluated at t=0.5 and compared
//    with approxEqualVectors' own defaults (vector3.d:698-699: maxRelDiff 1e-2,
//    maxAbsDiff 1e-5 -- the same defaults the C++ declaration carries at
//    src/geom/elements/vector3.hpp:145-146). The commented-out writeflns (D:211-213,
//    218-219, 225-226) print nothing, so the block contributes assertions only and the
//    same three paths appear as corpus rows svg.rect / svg.quad / svg.cubic in layer B.
// ============================================================================
void port_d_unittest() {
    // D:209 auto pth1 = new SVGPath("M3.0,3.0;L4.0,3.0;v1.0;h-1.0;Z");
    // D:210 auto p1 = pth1(0.5);
    auto pth1 = std::make_shared<SVGPath>("M3.0,3.0;L4.0,3.0;v1.0;h-1.0;Z");
    Vector3 p1 = (*pth1)(0.5);
    // D:214 assert(approxEqualVectors(p1, Vector3(4.0,4.0,0.0)));
    CHECK3(eil::geom::approxEqualVectors(p1, Vector3(4.0, 4.0, 0.0)),
           "D:214 approxEqualVectors(p1, Vector3(4.0,4.0,0.0))");

    // D:216 auto pth2 = new SVGPath("M2.0,0.0;Q2.0,2.0 0.0,2.0");
    // D:217 auto p2 = pth2(0.5);
    auto pth2 = std::make_shared<SVGPath>("M2.0,0.0;Q2.0,2.0 0.0,2.0");
    Vector3 p2 = (*pth2)(0.5);
    // D:220 assert(approxEqualVectors(p2, Vector3(1.5,1.5,0.0)));
    CHECK3(eil::geom::approxEqualVectors(p2, Vector3(1.5, 1.5, 0.0)),
           "D:220 approxEqualVectors(p2, Vector3(1.5,1.5,0.0))");

    // D:223 auto pth3 = new SVGPath("M2.0,0.0;C2.0,1.10457 1.10457,2.0 0.0,2.0");
    // D:224 auto p3 = pth3(0.5);
    auto pth3 = std::make_shared<SVGPath>("M2.0,0.0;C2.0,1.10457 1.10457,2.0 0.0,2.0");
    Vector3 p3 = (*pth3)(0.5);
    // D:227 assert(approxEqualVectors(p3, Vector3(1.41421,1.41421,0.0)));
    // The quarter-circle approximation: the cubic reaches 1.41421375 at t=0.5 (row
    // svg.cubic opCall), which is within 1e-2 relative of D's literal.
    CHECK3(eil::geom::approxEqualVectors(p3, Vector3(1.41421, 1.41421, 0.0)),
           "D:227 approxEqualVectors(p3, Vector3(1.41421,1.41421,0.0))");
}

// ============================================================================
// B. the probe replay, in probe order.
// ============================================================================
void replay_probe() {
    fmt_cases();
    splits();
    unittest_paths();
    commands_and_arguments();
    error_paths();
    empty_remainder();
    copy_paths();
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <probes/geom_gpath_svgpath.txt>\n";
        return 2;
    }
    port_d_unittest();
    replay_probe();
    return eil_test_gpath3::report("svgpath", argv[1]);
}
