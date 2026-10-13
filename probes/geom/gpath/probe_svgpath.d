// probes/geom/gpath/probe_svgpath.d — what geom/gpath/svgpath.d produces.
//
// svgpath.d parses a subset of the SVG path mini-language with
// std.format.formattedRead("%g,...") and turns it into a Polyline of Line and
// Bezier segments. Three things need measuring rather than assuming:
//
//  1. std.format's %g on INPUT. Measured (the "fmt.*" rows below, and the
//     standalone grammar table): it does NOT skip leading whitespace
//     ("1.0, 2.0" with "%g,%g" throws 'no digits seen for input " 2.0".'), a
//     space *in the format* skips any run of whitespace in the input (so
//     "2.0,2.00.0,2.0" parses as 2 2 0 2 and "1.0,2.0  ,3.0" fails on ",3.0"
//     after the run is eaten), a missing literal throws
//     'parseToFormatSpec: Cannot find character ',' in the input string.',
//     input exhausted at a conversion stops silently (leaving that target and
//     every later one at .init), input exhausted at a LITERAL throws the same
//     'Cannot find character' error, and a hex literal throws
//     'Floating point parsing: exponent is required for input "0x10,2".'
//     Conversion failures are FormatException (an Exception); overflow
//     ("1.7976931348623157e309") reports 'Range error'.
//  2. what an EMPTY remainder does. Measured: `double x = 42;
//     formattedRead("", "%g", &x);` leaves x at 42 — std.format returns silently
//     at end of input. Combined with svgpath.d:109 declaring
//     `double x1, y1, x2, y2, x3, y3;` (which in D are double.init == NaN), a
//     command with no coordinates stores NaN into the point, and the resulting
//     path carries NaN into every downstream evaluation. That is D's behaviour and
//     it is dumped, not fixed (rows "svg.init.*").
//  3. svgpath.d:32-37 NEVER ASSIGNS its own `txt` field (svgpath.d:68): the
//     constructor's parameter is named txt and the private field of the same name
//     is only read by toString() (D:56) and by the copy constructor (D:43). So
//     toString() prints path="" for every SVGPath, and dup()/the copy constructor
//     re-parse the EMPTY string, which yields no segments, and Polyline's guard
//     (polyline.d:34) throws. Measured: rows "svg.dup"/"svg.copy" are Errors, not
//     copies. Preserved as written; do not "fix" the port.
//
// See probe_path.d for why the reference tree is copied before building (gdtk at
// e587b235 does not compile as committed with ldc2 1.40: @nogc functions that use
// `new` in geom/elements/projection.d, nm/bbla.d:709, geom/grid/grid.d, sgrid.d and
// nm/brent.d via geom.misc.univariatefunctions). The @nogc deletion is a
// compile-time-only attribute change; DMD 2.110 compiles the UNMODIFIED tree and
// agrees with the shadow-tree output byte for byte (see probes/geom/gpath/probe_path.d).
//
// Regenerate (exact commands; nothing under /home/tang/packages/gdtk is written):
//   cd /home/tang/packages/Eilmer
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
//   /home/tang/packages/dlang/bin/ldc2 -i -I/tmp/gdtk_shadow \
//     -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/gpath \
//     probes/geom/gpath/probe_svgpath.d -of=/tmp/probe_svgpath_d
//   /tmp/probe_svgpath_d > baselines/e587b235/probes/geom_gpath_svgpath.txt
import std.conv;
import std.format;
import std.math;
import std.stdio;
import std.string;
import std.array;

import geom.elements;
import geom.gpath.path;
import geom.gpath.line;
import geom.gpath.bezier;
import geom.gpath.polyline;
import geom.gpath.svgpath;
import nm.number;
import pathdump;

// A newline or carriage return inside an exception message would split one
// observation across two corpus lines, so both sides replace them with the
// escape spelling. Tabs are left as they are (they cannot break a line).
string esc(const(char)[] msg)
{
    string res = "";
    foreach (dchar c; msg) {
        if (c == '\n') res ~= "\\n";
        else if (c == '\r') res ~= "\\r";
        else res ~= c;
    }
    return res;
}

// D's SVGPath shadows `segments` and `closed` with private copies (svgpath.d:77-78),
// and private in D means module-private, so a probe cannot name them through an
// SVGPath variable. Taking the Polyline base view is what an outside caller can do,
// and it reads the array the constructor actually built.
void dump_segments(const char[] tag, const Polyline pl)
{
    const Path[] segs = pl.segments;
    const double[] tv = pl.t_values;
    writefln("%s nseg=%d", tag, segs.length);
    foreach (i; 0 .. segs.length) {
        writefln("%s seg[%d] class=%s p0 x=%.17g y=%.17g z=%.17g p1 x=%.17g y=%.17g z=%.17g",
                 tag, i, segs[i].classString(),
                 segs[i](0.0).x, segs[i](0.0).y, segs[i](0.0).z,
                 segs[i](1.0).x, segs[i](1.0).y, segs[i](1.0).z);
        auto bz = cast(Bezier) segs[i];
        if (bz !is null) {
            foreach (j; 0 .. bz.B.length) {
                writefln("%s seg[%d] B[%d] x=%.17g y=%.17g z=%.17g",
                         tag, i, j, bz.B[j].x, bz.B[j].y, bz.B[j].z);
            }
        }
    }
    foreach (i; 0 .. tv.length) {
        writefln("%s t_values[%d]=%.17g", tag, i, tv[i]);
    }
}

// Build a path and dump it, or report which branch caught. Every command in
// do_svg_path_command can fail through formattedRead, and both Z-with-a-gap and
// a path with only a Move reach Polyline's guard, so each construction is wrapped.
void build_and_dump(const char[] tag, string src, double xt = 0.0, double yt = 0.0,
                    double xs = 1.0, double ys = 1.0, double tol = 1.0e-10)
{
    try {
        auto p = new SVGPath(src, xt, yt, xs, ys, tol);
        dump_path(tag, p);
        dump_segments(tag, p);
        writefln("%s str=%s", tag, p.toString());
    } catch (Exception e) {
        writefln("%s asError=false asException=true msg=%s", tag, esc(e.msg));
    } catch (Error e) {
        writefln("%s asError=true asException=false msg=%s", tag, esc(e.msg));
    }
}

// The input-side %g grammar on its own. The number of pointers handed to
// formattedRead is the number of conversions in the format, which is what every
// svgpath.d call site does (svgpath.d:113-186); the six scratch doubles of
// svgpath.d:109-110 are all printed so the untouched .init values show up.
void fmt_case(const char[] tag, string src, string fmt, int nspec)
{
    double[6] v;
    v[] = double.init;
    try {
        if (nspec == 1) formattedRead(src, fmt, &v[0]);
        else if (nspec == 2) formattedRead(src, fmt, &v[0], &v[1]);
        else if (nspec == 4) formattedRead(src, fmt, &v[0], &v[1], &v[2], &v[3]);
        else formattedRead(src, fmt, &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]);
        writefln("%s ok v0=%.17g v1=%.17g v2=%.17g v3=%.17g v4=%.17g v5=%.17g",
                 tag, v[0], v[1], v[2], v[3], v[4], v[5]);
    } catch (Exception e) {
        writefln("%s asError=false asException=true msg=%s", tag, esc(e.msg));
    } catch (Error e) {
        writefln("%s asError=true asException=false msg=%s", tag, esc(e.msg));
    }
}

// The same call with MORE pointers than the format has conversions (the shape of
// the standalone probes in this file's first draft). D keeps pulling input for the
// orphan arguments and only stops at end of input, so "2.0,2.0 3.0,4.0" with
// "%g,%g" and six pointers fails on the leftovers, while the same call with two
// pointers ignores them. Both rows are captured so the port cannot guess.
void fmt_case_orphans(const char[] tag, string src, string fmt)
{
    double[6] v;
    v[] = double.init;
    try {
        formattedRead(src, fmt, &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]);
        writefln("%s ok v0=%.17g v1=%.17g v2=%.17g v3=%.17g v4=%.17g v5=%.17g",
                 tag, v[0], v[1], v[2], v[3], v[4], v[5]);
    } catch (Exception e) {
        writefln("%s asError=false asException=true msg=%s", tag, esc(e.msg));
    } catch (Error e) {
        writefln("%s asError=true asException=false msg=%s", tag, esc(e.msg));
    }
}

void split_case(const char[] tag, string src)
{
    auto parts = src.strip().split(";");
    writefln("%s len=%d", tag, parts.length);
    foreach (i, p; parts) writefln("%s part[%d]=|%s|", tag, i, p);
}

// Same, with no conversion arguments at all: only the literals are matched.
void fmt_case_noarg(const char[] tag, string src, string fmt)
{
    try {
        formattedRead(src, fmt);
        writefln("%s ok", tag);
    } catch (Exception e) {
        writefln("%s asError=false asException=true msg=%s", tag, esc(e.msg));
    } catch (Error e) {
        writefln("%s asError=true asException=false msg=%s", tag, esc(e.msg));
    }
}

// The five formats svgpath.d actually uses (L68 M/m/L/l, H/h/V/v take one).
void fmt_cases()
{
    fmt_case("fmt.M", "3.0,3.0", "%g,%g", 2);
    fmt_case("fmt.H", "4.0", "%g", 1);
    fmt_case("fmt.Q", "2.0,2.0 0.0,2.0", "%g,%g %g,%g", 4);
    fmt_case("fmt.C", "2.0,1.10457 1.10457,2.0 0.0,2.0", "%g,%g %g,%g %g,%g", 6);
    fmt_case("fmt.neg", "-1.0,-2.5", "%g,%g", 2);
    // Whitespace: the argument string arrives strip()ped (svgpath.d:104), so a
    // leading space only survives between numbers.
    fmt_case("fmt.leadws", "  1.0,2.0", "%g,%g", 2);
    fmt_case("fmt.aftercomma", "1.0, 2.0", "%g,%g", 2);
    fmt_case("fmt.beforecomma", "1.0 ,2.0", "%g,%g", 2);
    fmt_case("fmt.spacelit", "1.0,2.0  ,3.0", "%g,%g %g", 3);
    fmt_case("fmt.nosep", "2.0,2.00.0,2.0", "%g,%g %g,%g", 4);
    fmt_case("fmt.tab", "5.0,5.0\t6.0,4.0", "%g,%g %g,%g", 4);
    fmt_case("fmt.crlf", "1.0,\r\n2.0", "%g,%g", 2);
    // Exhausted input, at a conversion and at a literal.
    fmt_case("fmt.empty", "", "%g,%g", 2);
    fmt_case("fmt.short", "1.0", "%g,%g", 2);
    fmt_case("fmt.trailingcomma", "1.0,", "%g,%g", 2);
    fmt_case_noarg("fmt.litafterempty", "", ",x");
    fmt_case_noarg("fmt.litafterinput", "1.0", "%g,x");
    fmt_case("fmt.spaceonly", " ", "%g", 1);
    // Number grammar edges.
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
    // Sign, word forms and the tail that the error message quotes.
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
    // The orphan-argument shape: more pointers than conversions. svgpath.d never
    // calls formattedRead this way, but the difference is what tells a reader that
    // trailing input is genuinely ignored by the real call sites.
    fmt_case_orphans("fmt.orphans.M", "3.0,3.0", "%g,%g");
    fmt_case_orphans("fmt.orphans.extra", "2.0,2.0 3.0,4.0", "%g,%g");
    // Trailing input past the last conversion, exactly as an 'L' command sees it.
    fmt_case("fmt.trailing_input", "2.0,2.0 3.0,4.0", "%g,%g", 2);
}

void main()
{
    // ---- the parser corpus -------------------------------------------------
    fmt_cases();

    // ---- the splitter ------------------------------------------------------
    split_case("split.empty", "");
    split_case("split.trailing", "M1.0,1.0;");
    split_case("split.leading", ";M1.0,1.0");
    split_case("split.spaces", "  M1.0,1.0  ");
    split_case("split.two", "M3.0,3.0;L4.0,3.0;v1.0;h-1.0;Z");

    // ---- the three unittest paths (svgpath.d:209-227) ----------------------
    build_and_dump("svg.rect", "M3.0,3.0;L4.0,3.0;v1.0;h-1.0;Z");
    build_and_dump("svg.quad", "M2.0,0.0;Q2.0,2.0 0.0,2.0");
    build_and_dump("svg.cubic", "M2.0,0.0;C2.0,1.10457 1.10457,2.0 0.0,2.0");

    // Every command in do_svg_path_command, in one path: M m L l H h V v Q q C c Z.
    build_and_dump("svg.all",
        "M1.0,1.0;L2.0,1.0;l0.5,0.5;H4.0;h0.25;V3.0;v0.5;m-1.0,0.0;"
        ~ "Q5.0,5.0 6.0,4.0;q1.0,1.0 -1.0,2.0;C7.0,7.0 8.0,7.0 8.0,9.0;"
        ~ "c1.0,0.0 1.0,2.0 -1.0,2.0;Z");

    // The translation and scale arguments, applied in D's order (translate first,
    // then scale; relative coordinates are scaled but not translated).
    build_and_dump("svg.xform", "M1.0,2.0;L3.0,4.0;h1.0", 0.5, -0.25, 2.0, 3.0);
    // The default tolerance, 1.0e-10, decides whether Z adds a closing Line
    // (svgpath.d:188-195), so the same path with a tolerance larger than the gap
    // produces one fewer segment.
    build_and_dump("svg.tol2", "M0.0,0.0;L1.0,0.0;L1.0,1.0;Z", 0.0, 0.0, 1.0, 1.0, 2.0);
    build_and_dump("svg.toldefault", "M0.0,0.0;L1.0,0.0;L1.0,1.0;Z");

    // Error paths: an unknown command, and the formattedRead failures.
    build_and_dump("svg.err.unknown", "M0.0,0.0;X1.0,2.0");
    build_and_dump("svg.err.nocomma", "M1.0 2.0");
    build_and_dump("svg.err.spaceaftercomma", "M1.0, 2.0");
    build_and_dump("svg.err.toofew", "L1.0");
    build_and_dump("svg.err.emptysegment", "M1.0,1.0;");
    build_and_dump("svg.err.moveonly", "M1.0,1.0");
    build_and_dump("svg.err.empty", "");
    build_and_dump("svg.err.lowerz", "M0.0,0.0;z");
    // Trailing input after the last conversion is ignored, so an 'L' command with
    // extra coordinates builds a segment to the FIRST pair only (svgpath.d:122-127).
    build_and_dump("svg.err.trailing", "M1.0,1.0;L2.0,2.0 3.0,4.0;Z");

    // The empty-remainder case: "M" with no coordinates at all. D's x1/y1 stay at
    // double.init (NaN), so the starting point is NaN and every segment that
    // follows inherits it.
    build_and_dump("svg.init", "M;L1.0,1.0");

    // dup() and the copy constructor re-parse the stored text (svgpath.d:49-52,
    // :41-47), but the constructor never stores it (D:32-38), so both reach
    // Polyline's "No segments present." guard.
    {
        auto pth = new SVGPath("M3.0,3.0;L4.0,3.0;v1.0;h-1.0;Z");
        try {
            auto d = pth.dup();
            dump_path("svg.dup", d);
        } catch (Exception e) {
            writefln("svg.dup asError=false asException=true msg=%s", esc(e.msg));
        } catch (Error e) {
            writefln("svg.dup asError=true asException=false msg=%s", esc(e.msg));
        }
        try {
            auto c = new SVGPath(pth);
            dump_segments("svg.copy", c);
        } catch (Exception e) {
            writefln("svg.copy asError=false asException=true msg=%s", esc(e.msg));
        } catch (Error e) {
            writefln("svg.copy asError=true asException=false msg=%s", esc(e.msg));
        }
    }
} // end main
