// probes/geom/misc/probe_svg_default_doc.d — the document shape D writes for a
// default-constructed SVGContext with one line, which tests/test_geom_misc_svg.cpp
// asserts on. Counted here so the test's expectations come from D, not from guesswork.
//
// Build: ldc2 -i -I/home/tang/packages/gdtk/src probes/geom/misc/probe_svg_default_doc.d
//        -of=/tmp/svgd   (run in a scratch dir; it writes svgprobe_unittest.svg)
module probe_svg_default_doc;

import std.stdio, std.file, std.conv, std.string;
import geom.misc.svg;

void main() {
    auto s = new SVGContext();
    s.open("svgprobe_unittest.svg");
    s.line(0.0, 0.0, 1.0, 1.0);
    s.close();
    string txt = read("svgprobe_unittest.svg").to!string;
    int n = 0;
    foreach (line; splitLines(txt, KeepTerminator.no)) {
        ++n;
        writefln("%d|%s", n, line);
    }
    writefln("total_lines=%d", n);
    writefln("bytes=%d", txt.length);
}
