// tests/demo_geom_gpath_gpath_utils.cpp — port of geom/gpath/gpath_utils_demo.d
// (29 lines), the only demo in cmake/wave2_geom_gpath2.cmake.
//
// stdout is byte-compared against the D binary's captured text,
// baselines/e587b235/probes/geom_gpath_gpath_utils_demo.txt (43 lines), through
// harness/compare_text.sh (CTest name demo_equiv_geom_gpath_gpath_utils). The
// capture recipe is harness/capture_d_demos.sh.
//
// The one difference from D is how the input is named. D's demo opens
// "sample-data/cowl-spline-pts.dat" relative to its cwd (its makefile runs it from
// src/geom); the port is handed the same bytes as argv[1] —
// tests/test_data/cowl-spline-pts.dat, byte-identical to
// /home/tang/packages/gdtk/src/geom/sample-data/cowl-spline-pts.dat (verified with
// diff: 39 lines, no differences) — so it runs from any build directory
// (wave-01 lesson 3, and cmake/wave2_geom_gpath2.cmake's comment on this target).
//
// D source order is preserved line for line. The two spelling differences are
// behaviour-preserving:
//   * `import geom;` (D:10) becomes the headers the demo resolves
//     (geom.gpath.gpath_utils, whose D:19 overload set it calls); the package
//     aggregate has no C++ counterpart (wave-02-brief section 4 item 5).
//   * `writeln(p.x, " ", p.y)` and `writeln(t)` print doubles through D's
//     to!string, which is %.6g here (brief section 5, measured:
//     3.141592653589793 -> 3.14159) — eil::writeln does the same, so the bytes
//     match; %.17g would print 0.81342000000000003 where D prints 0.81342.
//
// D's `Bezier b_init_guess;` is a null class handle (gpath_utils_demo.d:16), which is
// why the fit takes D:85-98's auto-distributed initial guess rather than
// D:73-82's copy; the port leaves a default-constructed shared_ptr, and
// src/geom/gpath/gpath_utils.hpp:20-23 is what tests that against nullptr. The demo
// ignores `success` (D:18, never printed), so this file does too.
#include <cstdio>
#include <memory>
#include <string>

#include "eil/eil.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/bezier.hpp"
#include "geom/gpath/gpath_utils.hpp"
#include "nm/number.hpp"

using eil::geom::BezierPtr;
using eil::geom::Vector3;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <cowl-spline-pts.dat>\n", argv[0]);
        return 2;
    }
    eil::writeln("Demo of gpath_utils functions.");
    eil::writeln("Using optimiseBezierPoints.");
    BezierPtr b_init_guess;
    eil::Array<double> ts;
    bool success = false;
    const BezierPtr bezier = eil::geom::optimiseBezierPoints(std::string(argv[1]), 7, b_init_guess, ts,
                                                      success);
    (void)success; // D does not print it either (gpath_utils_demo.d:18)

    eil::writeln("Optimised control points are:");
    for (size_t i = 0; i < bezier->B.length(); ++i) {
        const Vector3& p = bezier->B[i];
        eil::writeln(p.x, " ", p.y);
    }
    for (size_t i = 0; i < ts.length(); ++i) {
        eil::writeln(ts[i]);
    }
    return 0;
}
