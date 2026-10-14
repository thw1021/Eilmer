// tests/demo_geom_grid_paver2d.cpp
//
// Port of geom/grid/paver2d_demo.d (51 lines) — the 2D "paving" demo that builds a
// closed boundary out of gpath Line/Arc3 segments, paves it and writes a VTK file.
// Its stdout gate is baselines/e587b235/probes/geom_grid_paver2d_demo.txt (2 lines,
// 98 bytes) compared through harness/compare_text.sh (CTest name
// demo_equiv_geom_grid_paver2d), and its one artefact — the 20,112 byte
// paved_grid2.vtk, stored as bytes at
// baselines/e587b235/probes/geom_grid_paver2d_demo.paved_grid2.vtk — is compared by
// demo_artefacts_geom_grid_paver2d through harness/compare_artefacts.sh.
// Measured 2026-10-14: 2 of 2 stdout lines and 1 of 1 artefact row byte-identical; both
// gates are registered and pass in default, Release and Release+EIL_SLICE_CHECK.
//
// D source order is preserved statement for statement. The differences are spelling
// only:
//   * `import geom;` (D:8) has no C++ counterpart (wave-02-brief section 4 item 5);
//     the modules the demo actually uses are included instead.
//   * D's `Path[]` array literal of mixed Line/Arc3 handles (D:36-37) is an array of
//     the common base class, so it becomes eil::Array<PathPtr> (adapted-handles).
//   * `bndryArray ~= discretize_path(...)` (D:43) appends the WHOLE returned
//     Vector3[] as one element of Vector3[][] — eil::Array::append of an
//     eil::Array<Vector3>, not append_slice.
//   * The demo takes no argv and writes into the cwd, exactly as D does (D:46): the
//     working directory is part of the artefact contract, so CTest gives it a
//     build-tree directory via WORKING_DIRECTORY rather than the surface demo's
//     argv[1] convention (tests/demo_geom_surface_surface.cpp:22-25).
//
// Run: cd <work-dir> && ./demo_geom_grid_paver2d      (writes ./paved_grid2.vtk)
//
// Regenerate the baseline (both the 2 stdout lines and the .vtk are verified
// reproducible — see reports/wave-02-grid-brief.md section 9.5 and cmake/wave2_grid_g3.cmake:20-22):
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip /home/tang/packages/gdtk/src/geom/grid/paver2d_demo.d -of=/tmp/demo_g6/paver2d_demo -od=/tmp/demo_obj_g6
//   mkdir -p /tmp/demowork_g6 && cd /tmp/demowork_g6 && /tmp/demo_g6/paver2d_demo > out.txt
#include <memory>

#include "eil/eil.hpp"
#include "eil/array.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/arc.hpp"
#include "geom/gpath/gpath_utils.hpp"
#include "geom/gpath/line.hpp"
#include "geom/gpath/path.hpp"
#include "geom/grid/usgrid.hpp"
#include "geom/misc/univariatefunctions.hpp"

namespace {

using eil::geom::Arc3;
using eil::geom::Line;
using eil::geom::PathPtr;
using eil::geom::UnstructuredGrid;
using eil::geom::Vector3;

} // namespace

int main() {
    /*notes:                                    // D:13-25, kept verbatim
        - define boundary by points connected by paths from gpath
        - boundary must be anti-clockwise
        - boundary must contain an even number of segments [TODO] check assertion
        - try to avoid large jumps in cell size
      P3--------<--------P2
       |                 |
       |                 |
       v   (clockwise)   ^
       |                 |
       |                 |
      PO-------->--------P1
    */

    Vector3 P0 = Vector3(-0.5, 0);              // D:27
    Vector3 P1 = Vector3(0, 0);                 // D:28
    Vector3 P2 = Vector3(0.5, 1.0);             // D:29
    Vector3 P3 = Vector3(3, 3);                 // D:30
    Vector3 P4 = Vector3(3, 5.5);               // D:31
    Vector3 P5 = Vector3(0, 1.5);               // D:32
    Vector3 M0 = Vector3(0.108, 0.54);          // D:33
    Vector3 M1 = Vector3(-0.3, 1.04);           // D:34

    eil::Array<PathPtr> pths = {                // D:36-37
        std::make_shared<Line>(P0, P1), std::make_shared<Arc3>(P1, M0, P2),
        std::make_shared<Line>(P2, P3), std::make_shared<Line>(P3, P4),
        std::make_shared<Line>(P4, P5), std::make_shared<Arc3>(P5, M1, P0)};
    eil::Array<size_t> nn = {11, 31, 51, 31, 71, 31};   // D:38

    // Discretize the bounding paths.            // D:40
    eil::Array<eil::Array<Vector3>> bndryArray;  // D:41 `Vector3[][] bndryArray`
    auto cf = std::make_shared<eil::geom::LinearFunction>();  // D:42
    for (size_t i = 0; i < pths.length(); ++i) {  // D:43 foreach (i, pth; pths)
        bndryArray.append(eil::geom::discretize_path(pths[i], nn[i], cf));
    }
    // use the new paver constructor to make an unstructured grid.   // D:44
    auto grid2 = std::make_shared<UnstructuredGrid>(bndryArray, "PavedGrid2");  // D:45
    grid2->write_to_vtk_file("paved_grid2.vtk");  // D:46
    return 0;
} // end main()
