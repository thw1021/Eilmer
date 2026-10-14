// tests/demo_geom_surface_surface.cpp
//
// Port of geom/surface/surface_demo.d (55 lines) — the directory-level demo of the
// ParametricSurface family. stdout is byte-compared against the D binary's captured
// text, baselines/e587b235/probes/geom_surface_surface_demo.txt (18 lines), through
// harness/compare_text.sh (CTest name demo_equiv_geom_surface_surface).
//
// D source order is preserved line for line. The differences are spelling only:
//   * `import geom;` (D:9) is the aggregate package header, which has no C++
//     counterpart (wave-02-brief section 4 item 5); the ports of the five surfaces the
//     demo actually constructs are included instead, listed below.
//   * `writeln("my_patch= ", my_patch)` passes a class handle, which Phobos renders
//     through its virtual toString(). eil::writeln has no case for a class with a
//     toString() method (substrate/include/eil/format.hpp:140 is a hard
//     static_assert), so each handle and each Vector3 is written through the explicit
//     .toString() — the same convention tests/demo_geom_elements_vector3.cpp:13-14
//     records. The bytes on stdout are unchanged: D's to!string of a Vector3 is
//     "Vector3(0.1, 0.2, 3)", %.6g per field (wave-02-brief section 5).
//   * D's class handles become std::shared_ptr (adapted-handles).
//   * The one file the demo writes (D:52, writeSurfaceAsVtkXml into "aopatch.vtk")
//     keeps D's file name but is placed in the directory given as argv[1], defaulting
//     to the cwd. CTest passes a build-tree directory so no artefact can land in the
//     source tree. Nothing about that path reaches stdout, so the compared corpus is
//     unaffected.
//
// Run: ./demo_geom_surface_surface [output-dir]
//
// Regenerate the baseline (verified 2026-10-14 to reproduce the committed 18 lines
// byte for byte, sha256 c1c1abbf302bb54fb87aaa50b2a388f46a2f3f567c5c19be41b8648fbbbeeb58):
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   cd /home/tang/packages/Eilmer
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip \
//     /home/tang/packages/gdtk/src/geom/surface/surface_demo.d \
//     -of=/tmp/demo_s -od=/tmp/demo_obj_s
//   mkdir -p /tmp/demowork && cd /tmp/demowork && /tmp/demo_s > /tmp/surface_demo.txt
#include <memory>
#include <string>

#include "eil/eil.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/line.hpp"
#include "geom/surface/aopatch.hpp"
#include "geom/surface/channelpatch.hpp"
#include "geom/surface/coonspatch.hpp"
#include "geom/surface/parametricsurface.hpp"
#include "geom/surface/subrangedsurface.hpp"
#include "geom/surface/sweptpathpatch.hpp"

namespace {

// D's `writeln(a, handle)` / `writeln(a, vector)` — render through toString().
template <typename T> void wln(const char* head, const T& v) {
    eil::writeln(head, v.toString());
}

} // namespace

int main(int argc, char** argv) {
    const std::string outdir = (argc > 1 ? std::string(argv[1]) : std::string("."));

    eil::writeln("Begin demonstration of the ParametricSurface elements."); // D:13
    auto p00 = eil::geom::Vector3(0.0, 0.1, 3.0);                            // D:14
    auto p10 = eil::geom::Vector3(1.0, 0.1, 3.0);                            // D:15
    auto p11 = eil::geom::Vector3(1.0, 1.1, 3.0);                            // D:16
    auto p01 = eil::geom::Vector3(0.0, 1.1, 3.0);                            // D:17

    eil::writeln("CoonsPatch demo"); // D:19
    auto my_patch = std::make_shared<eil::geom::CoonsPatch>(p00, p10, p11, p01); // D:20
    wln("my_patch= ", *my_patch);                                                // D:21
    auto c = (*my_patch)(0.1, 0.1);                                             // D:22
    wln("my_patch(0.1, 0.1)= ", c);                                             // D:23

    eil::writeln("AOPatch demo");                       // D:25
    p10 = eil::geom::Vector3(1.0, 0.4, 3.0);            // D:26
    auto my_AOpatch = std::make_shared<eil::geom::AOPatch>(p00, p10, p11, p01); // D:27
    wln("my_AOpatch= ", *my_AOpatch);                   // D:28
    c = (*my_AOpatch)(0.1, 0.1);                        // D:29
    wln("my_AOpatch(0.1, 0.1)= ", c);                   // D:30

    eil::writeln("SubRangedSurface demo");                                     // D:32
    auto srp = std::make_shared<eil::geom::SubRangedSurface>(my_AOpatch, 0.0, 0.5, 0.0, 0.5); // D:33
    wln("srp= ", *srp);                            // D:34
    wln("srp(0.2,0.2)= ", (*srp)(0.2, 0.2));       // D:35

    eil::writeln("ChannelPatch demo");                                           // D:37
    auto cA = std::make_shared<eil::geom::Line>(eil::geom::Vector3(0.0, 0.0, 0.0),
                                                eil::geom::Vector3(1.0, 0.0, 0.0)); // D:38
    auto cB = std::make_shared<eil::geom::Line>(eil::geom::Vector3(0.0, 0.25, 0.0),
                                                eil::geom::Vector3(1.0, 1.0, 0.0)); // D:39
    auto chanp = std::make_shared<eil::geom::ChannelPatch>(cA, cB);                // D:40
    wln("chanp= ", *chanp);                            // D:41
    wln("chanp(0.5,0.5)= ", (*chanp)(0.5, 0.5));      // D:42

    eil::writeln("SweptPathPatch demo");                                         // D:44
    cA = std::make_shared<eil::geom::Line>(eil::geom::Vector3(0.0, 0.0, 0.0),
                                           eil::geom::Vector3(0.0, 1.0, 0.0));    // D:45
    cB = std::make_shared<eil::geom::Line>(eil::geom::Vector3(1.0, 0.25, 0.0),
                                           eil::geom::Vector3(2.0, 0.25, 0.0));   // D:46
    auto spp = std::make_shared<eil::geom::SweptPathPatch>(cA, cB);               // D:47
    wln("spp= ", *spp);                              // D:48
    wln("spp(0.5,0.5)= ", (*spp)(0.5, 0.5));         // D:49

    eil::writeln("Generating VTK-XML of AOPatch.");  // D:51
    eil::geom::write_surface_as_vtk_xml(*my_AOpatch, outdir + "/aopatch.vtk", 25, 50); // D:52

    eil::writeln("Done surface_demo.");              // D:54
    return 0;
}
