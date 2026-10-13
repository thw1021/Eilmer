// tests/demo_geom_misc_svg.cpp
//
// Port of geom/misc/svg_demo.d (42 lines). stdout is byte-compared against the D
// binary's captured text, baselines/e587b235/probes/geom_misc_svg_demo.txt, through
// harness/compare_text.sh (CTest name demo_equiv_geom_misc_svg). That corpus is 2
// lines — the two writeln calls below — because svg_demo.d draws to files and prints
// nothing else; the document text it produces is gated line-for-line by
// tests/test_geom_misc_svg.cpp, whose probe dumps two documents with exactly these
// same call sequences (svgprobe_demo1.svg / svgprobe_demo2.svg).
//
// D source order is preserved line for line. The differences are spelling only:
//   * `import geom.misc.svg;` -> the module header (wave-02-brief section 2, item 5:
//     no aggregate package header is generated).
//   * `new SVGContext(...)` -> std::make_shared (adapted-handles: D class values are
//     GC references).
//   * D's array literals `[0.0, 10.0, 20.0, 30.0]` as `double[]` arguments become
//     eil::Array<double> plus `.all()`, which is the `T[]` formal-parameter mapping of
//     wave-02-brief section 3. The elements and their order are copied verbatim.
//   * The two documents keep D's file names (test.svg, test2.svg). The demo runs in
//     the build directory under ctest, so nothing in the source tree is written.
//
// Run: ./demo_geom_misc_svg            (writes test.svg, test2.svg into the cwd)
//
// Regenerate the baseline:
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -i -I/home/tang/packages/gdtk/src /home/tang/packages/gdtk/src/geom/misc/svg_demo.d -of=/tmp/svg_demo_d
//   mkdir -p /tmp/svgdemowork && cd /tmp/svgdemowork
//   /tmp/svg_demo_d > /home/tang/packages/Eilmer/baselines/e587b235/probes/geom_misc_svg_demo.txt
//   (verified 2026-10-14: that command reproduces the committed corpus byte for byte)
#include <memory>

#include "eil/eil.hpp"
#include "geom/misc/svg.hpp"

using eil::geom::SVGContext;

int main() {
    eil::writeln("Begin demonstration of the SVG rendering primitives.");
    auto s = std::make_shared<SVGContext>(100, 120.0, "mm", "test run");

    s->open("test.svg");
    s->line(0.0, 0.0, 100.0, 120.0);
    s->close();

    s->open("test2.svg");
    s->line(0.0, 0.0, 90.0, 120.0);
    s->setLineWidth(0.5);
    s->setFillColour("yellow");
    s->circle(25.0, 85.0, 12.3);
    s->setLineWidth(0.25);
    {
        eil::Array<double> xs, ys;
        for (double v : {0.0, 10.0, 20.0, 30.0}) xs.append(v);
        for (double v : {50.0, 60.0, 50.0, 60.0}) ys.append(v);
        s->polyline(xs.all(), ys.all(), true);
    }
    s->text(25.0, 85.0, "Circle", -30.0, "middle", 10);
    s->arc(90.0, 0.0, 60.0, 30.0, 60.0, 0.0);
    s->setLineWidth(0.75);
    s->arc(30.0, 0.0, 60.0, 30.0, 60.0, 0.0, true);
    s->bezier3(80.0, 80.0, 40.0, 80.0, 80.0, 100.0, 40.0, 100.0);
    s->setLineWidth(0.01);
    s->dotlabel(70.0, 20.0, "a");
    s->setLineWidth(0.1);
    {
        eil::Array<double> xs, ys;
        for (double v : {60.0, 95.0, 95.0, 60.0}) xs.append(v);
        for (double v : {10.0, 10.0, 50.0, 50.0}) ys.append(v);
        s->polygon(xs.all(), ys.all(), true, true, true);
    }
    s->close();

    eil::writeln("Done.");
    return 0;
}
