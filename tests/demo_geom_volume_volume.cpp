// tests/demo_geom_volume_volume.cpp
//
// Port of geom/volume/volume_demo.d (50 lines) — the directory-level demo of the
// ParametricVolume family. stdout is byte-compared against the D binary's captured
// text, baselines/e587b235/probes/geom_volume_volume_demo.txt (13 lines), through
// harness/compare_text.sh (CTest name demo_equiv_geom_volume_volume).
//
// D source order is preserved line for line. The differences are spelling only:
//   * `import geom;` (D:9) is the aggregate package header, which has no C++
//     counterpart (wave-02-brief section 4 item 5); the five modules the demo actually
//     constructs are included instead.
//   * `writeln("my_vol= ", my_vol)` passes a class handle, which Phobos renders through
//     its virtual toString(). eil::writeln has no case for a class with a toString()
//     method (substrate/include/eil/format.hpp:140 is a hard static_assert), so every
//     handle and every Vector3 is written through the explicit .toString(); the bytes
//     are the same, and D's to!string of a Vector3 is "Vector3(0.1, 0.2, 0.3)", %.6g per
//     field (wave-02-brief section 5).
//   * D's class handles become std::shared_ptr (adapted-handles), and D's `Vector3[8] p`
//     passed to `this(in Vector3[] p)` (tfivolume.d:91) becomes an eil::Slice over a
//     std::array, which is the T-012 mapping for a static array argument.
//   * This demo writes no file: D's volume_demo.d contains no write call (measured:
//     grep -c "write_.*vtk\|su2" over the module is 0), so there is nothing to place in
//     a temporary directory and no artefact can land in the source tree.
//
// Run: ./demo_geom_volume_volume
//
// Regenerate the baseline (verified 2026-10-14 to reproduce the committed 13 lines byte
// for byte, sha256 fa7e4db621023742060a54f7f24843aa8369e3dbc3e002d03d36e5d67e0ea217):
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   cd /home/tang/packages/Eilmer
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip \
//     /home/tang/packages/gdtk/src/geom/volume/volume_demo.d \
//     -of=/tmp/demo_v -od=/tmp/demo_obj_v
//   mkdir -p /tmp/demowork && cd /tmp/demowork && /tmp/demo_v > /tmp/volume_demo.txt
#include <array>
#include <memory>

#include "eil/eil.hpp"
#include "eil/array.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/line.hpp"
#include "geom/surface/coonspatch.hpp"
#include "geom/volume/subrangedvolume.hpp"
#include "geom/volume/sweptsurfacevolume.hpp"
#include "geom/volume/tfivolume.hpp"
#include "geom/volume/twosurfacevolume.hpp"

namespace {

using eil::geom::CoonsPatch;
using eil::geom::Line;
using eil::geom::ParametricSurface;
using eil::geom::TFIVolume;
using eil::geom::Vector3;

// D's `writeln(a, handle)` / `writeln(a, vector)` — render through toString().
template <typename T> void wln(const char* head, const T& v) {
    eil::writeln(head, v.toString());
}

} // namespace

int main() {
    eil::writeln("Begin demonstration of the ParametricVolume elements."); // D:13
    std::array<Vector3, 8> p = {Vector3(0.0, 0.1, 0.0), Vector3(1.0, 0.1, 0.0), // D:14-18
                                Vector3(1.0, 1.1, 0.0), Vector3(0.0, 1.1, 0.0),
                                Vector3(0.0, 0.1, 3.0), Vector3(1.0, 0.1, 3.0), // D:20-23
                                Vector3(1.0, 1.1, 3.0), Vector3(0.0, 1.1, 3.0)};

    eil::writeln("TFIVolume demo");                                       // D:25
    auto my_vol = std::make_shared<TFIVolume>(
        TFIVolume::PointSlice(p.data(), p.size(), nullptr, 0));           // D:26
    wln("my_vol= ", *my_vol);                                             // D:27
    auto c = (*my_vol)(0.1, 0.1, 0.1);                                    // D:28
    wln("my_vol(0.1, 0.1, 0.1)= ", c);                                    // D:29

    eil::writeln("SubRangedVolume demo");                                 // D:31
    auto srv = std::make_shared<eil::geom::SubRangedVolume>(my_vol, 0.0, 0.5, 0.0, 0.5, 0.0,
                                                            0.5);         // D:32
    wln("srv(0.2,0.2,0.2)= ", (*srv)(0.2, 0.2, 0.2));                     // D:33

    eil::writeln("SweptSurfaceVolume demo");                              // D:35
    ParametricSurface::Ptr face0123 = std::make_shared<CoonsPatch>(p[0], p[1], p[2], p[3]); // D:36
    auto edge04 = std::make_shared<Line>(p[0], p[4]);                     // D:37
    auto ssv = std::make_shared<eil::geom::SweptSurfaceVolume>(face0123, edge04); // D:38
    wln("ssv=", *ssv);                                                    // D:39
    wln("ssv(0.1, 0.1, 0.1)= ", (*ssv)(0.1, 0.1, 0.1));                   // D:40

    eil::writeln("TwoSurfaceVolume demo");                                // D:42
    face0123 = std::make_shared<CoonsPatch>(p[0], p[1], p[2], p[3]);       // D:43
    auto face4567 = std::make_shared<CoonsPatch>(p[4], p[5], p[6], p[7]);  // D:44
    auto tsv = std::make_shared<eil::geom::TwoSurfaceVolume>(face0123, face4567); // D:45
    wln("tsv=", *tsv);                                                    // D:46
    wln("tsv(0.1, 0.1, 0.1)= ", (*tsv)(0.1, 0.1, 0.1));                   // D:47

    eil::writeln("Done volume_demo.");                                    // D:49
    return 0;
}
