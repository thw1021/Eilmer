// tests/demo_geom_misc_sketch.cpp
//
// Port of geom/misc/sketch_demo.d (83 lines). stdout is byte-compared against the D
// binary's captured text, baselines/e587b235/probes/geom_misc_sketch_demo.txt, through
// harness/compare_text.sh (CTest name demo_equiv_geom_misc_sketch). That corpus is
// exactly 2 lines and 62 bytes, with 0 bytes on stderr; the three documents the demo
// writes are versioned next to it and byte-compared too
// (demo_artifact_geom_misc_sketch_svg1/2/3), so the whole demo is gated on bytes, not on
// a tolerance.
//
// D source order is preserved line for line (the three documents, the same continuing
// Sketch for the first two, the reassignments of a/b/c/abc at D:62-65). The differences
// are spelling only:
//   * `import geom;` -> the module headers actually used (wave-02-brief section 4
//     item 5: no aggregate header is generated).
//   * `new Sketch(...)` / `new Arc(...)` / `new CoonsPatch(...)` -> std::make_shared
//     (adapted-handles: D class values are GC references).
//   * `main(string[] args)` with `if (args.length > 1) { renderer = args[1]; }` ->
//     argv[1] keeps D's meaning (the renderer name), so the D-shaped call
//     `./demo_geom_misc_sketch svg` prints the same first line as `./sketch_demo`.
//   * argv[2] is NEW and is only an output directory: D writes test.svg/test2.svg/
//     test3.svg into the current directory (sketch_demo.d:24, :40, :69), which under
//     ctest would drop artefacts into the build tree next to the sources. The port
//     prefixes the three names with argv[2] when it is given, so the registered test can
//     point the demo at a private build-tree directory (reports/wave-02-sketch-brief.md
//     section 5). File names and their contents are unchanged.
//   * D's array literals `[0.0,0.0,120.0,120.0]` and `[Vector3(0,50), ...]` become
//     eil::Array plus `.all()`, the `T[]` formal-parameter mapping of wave-02-brief
//     section 3, with the elements and their order copied verbatim.
//
// Run: ./demo_geom_misc_sketch [renderer] [output-dir]
//
// Regenerate the baseline:
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   cd /home/tang/packages/Eilmer
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip \
//     /tmp/gdtk_shadow/geom/misc/sketch_demo.d -of=/tmp/sketch_demo_d
//   mkdir -p /tmp/demowork && cd /tmp/demowork
//   /tmp/sketch_demo_d > /home/tang/packages/Eilmer/baselines/e587b235/probes/geom_misc_sketch_demo.txt
//   cp test.svg test2.svg test3.svg \
//     /home/tang/packages/Eilmer/baselines/e587b235/probes/  (as *.test*.svg)
#include <memory>
#include <string>
#include <vector>

#include "eil/array.hpp"
#include "eil/eil.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/arc.hpp"
#include "geom/misc/sketch.hpp"
#include "geom/surface/coonspatch.hpp"

using eil::geom::Arc;
using eil::geom::CoonsPatch;
using eil::geom::Sketch;
using eil::geom::Vector3;

namespace {

eil::Array<double> canvas(double x0, double y0, double x1, double y1) {
    eil::Array<double> a;
    a.append(x0);
    a.append(y0);
    a.append(x1);
    a.append(y1);
    return a;
}

eil::Array<Vector3> points(std::initializer_list<Vector3> ps) {
    eil::Array<Vector3> a;
    for (const Vector3& p : ps) a.append(p);
    return a;
}

} // namespace

int main(int argc, char** argv) {
    std::string renderer = "svg"; // other is "xplot"
    if (argc > 1) { renderer = argv[1]; }
    // Only ever a prefix for the three D file names (see the header note).
    const std::string outdir = (argc > 2) ? std::string(argv[2]) + "/" : std::string();
    eil::writeln("Begin demonstration of the geometric rendering with ", renderer);
    //
    auto s = std::make_shared<Sketch>(renderer, "xyortho",
                                      canvas(0.0, 0.0, 120.0, 120.0).all());
    s->viewport.set(-2.0, -2.0, 2.0, 2.0);
    auto a = Vector3(2.0, 0.0);
    auto b = Vector3(1.0, 1.0);
    auto c = Vector3(1.0, 0.0);
    auto abc = std::make_shared<Arc>(a, b, c);
    s->start(outdir + "test.svg");
    s->line(Vector3(-1.0, 1.0, 0.0), Vector3(1.0, -1.0, 0.0));
    s->render(abc);
    s->rule("x", -1.2, 1.2, 0.4, Vector3(0.0, -1.3), 0.03, "%.1f", 0.12, 0.0, 8);
    s->rule("y", -1.2, 1.2, 0.4, Vector3(-1.3, 0.0), 0.03, "%.1f", 0.06, 0.0, 8);
    s->setFillColour("green");
    auto p00 = Vector3(0.0, 0.1);
    auto p10 = Vector3(1.0, 0.1);
    auto p11 = Vector3(1.0, 1.1);
    auto p01 = Vector3(0.0, 1.1);
    auto my_patch = std::make_shared<CoonsPatch>(p00, p10, p11, p01);
    s->dotlabel(p00, "p00");
    s->dotlabel(p10, "p10");
    s->dotlabel(p11, "p11");
    s->dotlabel(p01, "p01");
    s->render(my_patch);
    s->finish();
    //
    s->start(outdir + "test2.svg");
    s->viewport.set(0.0, 0.0, 120.0, 120.0);
    s->line(Vector3(0.0, 0.0), Vector3(90.0, 120.0));
    s->setLineWidth(0.5);
    s->setFillColour("yellow");
    s->circle(Vector3(25.0, 85.0), 12.3);
    s->setLineWidth(0.25);
    {
        eil::Array<Vector3> pl =
            points({Vector3(0, 50), Vector3(10, 60), Vector3(20, 50), Vector3(30, 60)});
        s->polyline(pl.all(), true);
    }
    s->text(Vector3(25.0, 85.0), "Circle", -30.0, "middle", 10);
    s->arc(Vector3(90.0, 0.0), Vector3(60.0, 30.0), Vector3(60.0, 0.0));
    s->setLineWidth(0.75);
    s->arc(Vector3(30.0, 0.0), Vector3(60.0, 30.0), Vector3(60.0, 0.0), true);
    s->bezier3(Vector3(80.0, 80.0), Vector3(40.0, 80.0),
               Vector3(80.0, 100.0), Vector3(40.0, 100.0));
    s->setLineWidth(0.01);
    s->dotlabel(Vector3(70.0, 20.0), "a");
    s->setLineWidth(0.5);
    {
        eil::Array<Vector3> pg =
            points({Vector3(60, 10), Vector3(95, 10), Vector3(95, 50), Vector3(60, 50)});
        s->polygon(pg.all(), true, true, true);
    }
    s->finish();
    //
    // D:60 reassigns the same handle; shared_ptr assignment is the equivalent.
    s = std::make_shared<Sketch>(renderer, "isometric",
                                 canvas(0.0, 0.0, 120.0, 120.0).all());
    s->viewport.set(-2.0, -2.0, 2.0, 2.0);
    // Turn the world around so we view from the original -z axis.
    s->look_at(Vector3(0.0, 0.0, -1.0), Vector3(0.0, 0.0, 0.0), Vector3(0.0, 1.0, 0.0));
    a = Vector3(2.0, 0.0);
    b = Vector3(1.0, 1.0);
    c = Vector3(1.0, 0.0);
    abc = std::make_shared<Arc>(a, b, c);
    s->start(outdir + "test3.svg");
    s->line(Vector3(-1.0, 1.0, 0.0), Vector3(1.0, -1.0, 0.0));
    s->render(abc);
    s->rule("x", -1.2, 1.2, 0.4, Vector3(0.0, 0.0, 0.0), 0.03, "%.1f", 0.12, -30.0, 8);
    s->text(Vector3(1.0, 0.1), "x", 0.0, "middle", 10);
    s->rule("y", -1.2, 1.2, 0.4, Vector3(0.0, 0.0, 0.0), 0.03, "%.1f", 0.20, -30.0, 8);
    s->text(Vector3(0.1, 1.0), "y", 0.0, "middle", 10);
    s->rule("z", -1.2, 1.2, 0.4, Vector3(0.0, 0.0, 0.0), 0.03, "%.1f", 0.16, 30.0, 8);
    s->text(Vector3(0.0, 0.1, 1.0), "z", 0.0, "middle", 10);
    s->finish();
    //
    eil::writeln("Done.");
    return 0;
}
