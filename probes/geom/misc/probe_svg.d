// probes/geom/misc/probe_svg.d — dump the SVG documents geom/misc/svg.d emits.
//
// Why this probe exists: svg.d is pure text generation, so the gate is byte
// equality of the emitted document, not a tolerance. D's SVGContext writes through a
// std.stdio.File, so the probe draws everything into real files and prints the files
// back, delimited by "--- doc <name> ---" markers, after all writing is finished
// (stdout and the File buffer must not interleave). svg_demo.d's own two-line stdout
// is captured separately as geom_misc_svg_demo.txt; the drawing here is a superset
// that reaches every primitive and every styleStr branch.
//
// Covered: the 4 unit lengths and the unknown-unit Exception, all three default
// arguments of the constructor, line (solid/dashed), polyline, polygon (fill/stroke
// combinations), arc (clockwise and counter-clockwise, plus the "Radii don't match"
// Exception), circle, bezier3, text (angle 0 and non-zero), dotlabel (with and
// without a label), begin_group/end_group (with and without an id), add_comment, and
// styleStr called directly over its 8 boolean combinations and both opacity defaults.
//
// Build and capture:
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   cd /home/tang/packages/Eilmer
//   ldc2 -i -I/home/tang/packages/gdtk/src probes/geom/misc/probe_svg.d -of=/tmp/svg_d
//   (cd /tmp && /tmp/svg_d > ../home.../baselines/e587b235/probes/geom_misc_svg.txt)
// (run it in a scratch directory: the probe writes svgprobe1..3.svg there)
import std.conv;
import std.file;
import std.format;
import std.stdio;
import std.string;

import geom.misc.svg;

/// Print one emitted document, one stdout line per document line. A document that
/// ends with a newline does not produce a trailing empty line (measured rule, and the
/// port applies the same split).
void dump_doc(string name) {
    string text = readText(name);
    writefln("--- doc %s bytes=%d ---", name, text.length);
    auto lines = split(text, "\n");
    if (lines.length > 0 && lines[$ - 1].length == 0) lines = lines[0 .. $ - 1];
    foreach (l; lines) writeln(l);
}

void main() {
    // ---- unit handling and the constructor defaults ----------------------
    // width/height are printed by open() as %.2f of width*unitToPixels, so each unit
    // length is observable in the document text.
    foreach (unit; ["in", "mm", "cm", "m"]) {
        auto s = new SVGContext(12.0, 9.5, unit, "units " ~ unit, "one document per unit");
        s.open(format("svgprobe_u_%s.svg", unit));
        s.line(0.0, 0.0, 12.0, 9.5);
        s.close();
    }
    try {
        auto bad = new SVGContext(1.0, 1.0, "furlong");
        writeln("unit unknown built=1");
    }
    catch (Exception e) {
        writefln("unit unknown built=0 isError=0 msg=[%s]", e.msg);
    }
    catch (Error e) {
        writefln("unit unknown built=0 isError=1 msg=[%s]", e.msg);
    }

    // ---- svg_demo.d's two documents, byte for byte -----------------------
    auto s = new SVGContext(100, 120.0, "mm", "test run");
    s.open("svgprobe_demo1.svg");
    s.line(0.0, 0.0, 100.0, 120.0);
    s.close();

    s.open("svgprobe_demo2.svg");
    s.line(0.0, 0.0, 90.0, 120.0);
    s.setLineWidth(0.5);
    s.setFillColour("yellow");
    s.circle(25.0, 85.0, 12.3);
    s.setLineWidth(0.25);
    s.polyline([0.0, 10.0, 20.0, 30.0], [50.0, 60.0, 50.0, 60.0], true);
    s.text(25.0, 85.0, "Circle", -30.0, "middle", 10);
    s.arc(90.0, 0.0, 60.0, 30.0, 60.0, 0.0);
    s.setLineWidth(0.75);
    s.arc(30.0, 0.0, 60.0, 30.0, 60.0, 0.0, true);
    s.bezier3(80.0, 80.0, 40.0, 80.0, 80.0, 100.0, 40.0, 100.0);
    s.setLineWidth(0.01);
    s.dotlabel(70.0, 20.0, "a");
    s.setLineWidth(0.1);
    s.polygon([60.0, 95.0, 95.0, 60.0], [10.0, 10.0, 50.0, 50.0], true, true, true);
    s.close();

    // ---- a document that reaches every remaining branch ------------------
    auto t = new SVGContext(); // all defaults: 120x120 mm, "Untitled", "No description"
    t.open("svgprobe_full.svg");
    t.add_comment("first comment");
    t.add_comment("");
    t.begin_group();
    t.line(1.0, 1.0, 2.0, 3.0);
    t.line(1.0, 1.0, 2.0, 3.0, true);
    t.line(1.0, 1.0, 2.0, 3.0, false, 0.25);
    t.end_group();
    t.begin_group("group-with-id", 0.5);
    t.polyline([0.0, 1.0, 2.0], [0.0, 1.0, 0.0]);
    t.polyline([0.0, 1.0, 2.0], [0.0, 1.0, 0.0], true, 0.3);
    // A deliberately uneven pair of lists: polygon/polyline loop over min(length).
    t.polyline([0.0, 1.0, 2.0, 3.0], [0.0, 1.0]);
    t.end_group();
    t.setLineColour("red");
    t.setFillColour("#00ff00");
    t.polygon([0.0, 10.0, 10.0], [0.0, 0.0, 10.0]); // defaults fill=true, stroke=true
    t.polygon([0.0, 10.0, 10.0], [0.0, 0.0, 10.0], true, false);
    t.polygon([0.0, 10.0, 10.0], [0.0, 0.0, 10.0], false, true, true);
    t.polygon([0.0, 10.0, 10.0], [0.0, 0.0, 10.0], true, true, false, "poly-id", 0.125);
    t.clearFillColour();
    t.circle(20.0, 20.0, 5.0);
    t.circle(20.0, 20.0, 5.0, false, true, true, "c2", 0.75);
    t.circle(20.0, 20.0, 5.0, true, false);
    // Both arc directions: crossz sign decides sweep_flag (svg.d:256-257).
    t.arc(0.0, 10.0, 10.0, 10.0, 5.0, 0.0);
    t.arc(10.0, 10.0, 0.0, 10.0, 5.0, 0.0);
    t.arc(0.0, 10.0, 10.0, 10.0, 5.0, 0.0, true, 0.4);
    t.bezier3(0.0, 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, true, 0.6);
    t.text(30.0, 30.0, "no angle");
    t.text(30.0, 30.0, "rotated", 45.0, "end", 12, "blue", "serif");
    t.text(30.0, 30.0, "", -45.0);
    t.dotlabel(40.0, 40.0);
    t.dotlabel(40.0, 40.0, "lbl", "start", 3.0, 8, "green", "monospace");
    // dashArray with explicit lengths, then back to the defaults.
    t.setDashArray(5.0, 1.5);
    t.line(0.0, 60.0, 10.0, 60.0, true);
    t.setDashArray();
    t.line(0.0, 70.0, 10.0, 70.0, true);
    t.close();

    // ---- an arc whose radii disagree (svg.d:253-255) ---------------------
    try {
        auto u = new SVGContext(50.0, 50.0, "mm", "arc failure");
        u.open("svgprobe_arc.svg");
        u.arc(0.0, 10.0, 10.0, 10.0, 5.0, 0.0);
        u.arc(0.0, 10.0, 20.0, 30.0, 5.0, 0.0); // r0 = 5.099, r1 = 33.5 -> throws
        u.close();
        writeln("arc threw=0");
    }
    catch (Exception e) {
        writefln("arc threw=1 msg=[%s]", e.msg);
    }
    catch (Error e) {
        writefln("arc threw=1 isError=1 msg=[%s]", e.msg);
    }

    // ---- styleStr over its whole flag cross-product ----------------------
    auto v = new SVGContext(60.0, 60.0, "mm", "styles");
    foreach (fill; [false, true]) {
        foreach (stroke; [false, true]) {
            foreach (dashed; [false, true]) {
                writefln("style fill=%s stroke=%s dashed=%s s=[%s]", to!string(fill),
                         to!string(stroke), to!string(dashed), v.styleStr(fill, stroke, dashed));
                writefln("style fill=%s stroke=%s dashed=%s o=[%s]", to!string(fill),
                         to!string(stroke), to!string(dashed),
                         v.styleStr(fill, stroke, dashed, 0.25, 0.75));
            }
        }
    }
    // The coordinate transforms, which every primitive above routes through.
    writefln("px x0=%.17g x100=%.17g y0=%.17g y60=%.17g", v.toPixelsX(0.0), v.toPixelsX(100.0),
             v.toPixelsY(0.0), v.toPixelsY(60.0));
    v.setLineWidth(0.25);
    v.setLineColour("black");
    v.clearFillColour();

    // ---- now print the documents ----------------------------------------
    foreach (name; ["svgprobe_demo1.svg", "svgprobe_demo2.svg", "svgprobe_full.svg"]) {
        dump_doc(name);
    }
    foreach (unit; ["in", "mm", "cm", "m"]) {
        dump_doc(format("svgprobe_u_%s.svg", unit));
    }
    writeln("done=1");
}
