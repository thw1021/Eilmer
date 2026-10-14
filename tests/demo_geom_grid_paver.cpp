// tests/demo_geom_grid_paver.cpp
//
// Port of geom/grid/paver_demo.d (115 lines) — Heather Muir's original 2D paver demo:
// it walks the closed boundary at the parameter values construct_boundary samples from
// the gpath segments, hands the point list plus one BoundaryFaceSet per edge to the
// deprecated paving constructor (usgrid.d:514-551) and writes the paved grid.
// Its stdout gate is baselines/e587b235/probes/geom_grid_paver_demo.txt (129 lines,
// 5,097 bytes — every line of it comes out of geom/grid/paver.d, not out of this
// demo's own source, which contains no writeln) compared through
// harness/compare_text.sh (CTest name demo_equiv_geom_grid_paver); measured 2026-10-14:
// 129 of 129 lines byte-identical. Its artefact, the 141,490 byte paved_grid.vtk stored
// at baselines/e587b235/probes/geom_grid_paver_demo.paved_grid.vtk, does NOT reproduce —
// 398 of its 1,490 vertex lines differ in the last digits (first difference at vertex 228;
// the first 214 vertices, which are construct_boundary's output, are byte-identical) — so
// that gate is not registered. See cmake/wave2_grid_g6.cmake for the numbers.
//
// D source order is preserved statement for statement. The differences are spelling
// only:
//   * `import geom;` (D:9) → the specific module headers used (wave-02-brief section 4
//     item 5).
//   * D's `Path[]`/`size_t[]`/`string[]` slice parameters (D:12, :36) become const
//     eil::Array references; `BoundaryFaceSet[]` (D:39) is D's class array, so the port
//     element type is std::shared_ptr<BoundaryFaceSet> (adapted-handles, same shape as
//     the UnstructuredGrid constructor's parameter at src/geom/grid/usgrid.hpp:296).
//   * `boundaries[$-1].face_id_list = ...` (D:51-52) writes through the handle, so the
//     port uses `boundaries.back()->...`.
//   * D's loop counters `for(int j; ...)` (D:24, :46) start at 0 and are compared
//     against a size_t bound; the port writes the comparison as size_t so -Wextra's
//     signedness rule cannot change what is iterated.
//   * `edge.opCall(val)` (D:29) is D's call operator → `(*edge)(val)`, which is
//     `Path::operator()` at src/geom/gpath/path.hpp:78.
//   * Like paver2d_demo, this demo takes no argv and writes into the cwd (D:110), so
//     the working directory is supplied by CTest's WORKING_DIRECTORY.
//
// Run: cd <work-dir> && ./demo_geom_grid_paver      (writes ./paved_grid.vtk)
//
// Regenerate the baseline (both the 129 stdout lines and the .vtk are verified
// reproducible — cmake/wave2_grid_g3.cmake:20-22):
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip \
//     /home/tang/packages/gdtk/src/geom/grid/paver_demo.d \
//     -of=/tmp/demo_g6/paver_demo -od=/tmp/demo_obj_g6
//   mkdir -p /tmp/demowork_g6 && cd /tmp/demowork_g6 && /tmp/demo_g6/paver_demo > out.txt
#include <memory>
#include <string>

#include "eil/eil.hpp"
#include "eil/array.hpp"
#include "eil/error.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/arc.hpp"
#include "geom/gpath/line.hpp"
#include "geom/gpath/path.hpp"
#include "geom/grid/usgrid.hpp"

namespace {

using eil::geom::Arc3;
using eil::geom::BoundaryFaceSet;
using eil::geom::Line;
using eil::geom::PathPtr;
using eil::geom::UnstructuredGrid;
using eil::geom::Vector3;

// D:12-34 Vector3[] construct_boundary(Path[] edges, size_t[] n)
eil::Array<Vector3> construct_boundary(const eil::Array<PathPtr>& edges,
                                       const eil::Array<size_t>& n) {
    eil_assert(edges.length() == n.length());              // D:14
    eil::Array<Vector3> boundary;                          // D:15
    size_t i = 0;                                          // D:16
    eil::Array<double> t_list;                             // D:17
    double t;                                              // D:18
    double t_incr;                                         // D:19
    for (size_t e = 0; e < edges.length(); ++e) {          // D:20 foreach (edge; edges)
        const PathPtr& edge = edges[e];
        t = 0;                                             // D:21
        t_incr = 1.0 / (n[i] - 1);                         // D:22
        t_list = eil::Array<double>();                     // D:23 `t_list = []`
        for (size_t j = 0; j < static_cast<size_t>(n[i] - 1); ++j) {   // D:24
            t_list.append(t);                              // D:25
            t += t_incr;                                   // D:26
        }
        for (size_t v = 0; v < t_list.length(); ++v) {     // D:28 foreach (val; t_list)
            boundary.append((*edge)(t_list[v]));           // D:29 `edge.opCall(val)`
        }
        ++i;                                               // D:31
    }
    return boundary;                                       // D:33
}

// D:36-56 BoundaryFaceSet[] construct_boundary_faces(string[] BC_list, size_t[] n)
eil::Array<std::shared_ptr<BoundaryFaceSet>> construct_boundary_faces(
    const eil::Array<std::string>& BC_list, const eil::Array<size_t>& n) {
    eil_assert(BC_list.length() == n.length());            // D:38
    eil::Array<std::shared_ptr<BoundaryFaceSet>> boundaries;  // D:39
    size_t index = 0;                                      // D:40
    size_t list_index = 0;                                 // D:41
    for (size_t b = 0; b < BC_list.length(); ++b) {        // D:42 foreach (BC; BC_list)
        const std::string& BC = BC_list[b];
        boundaries.append(std::make_shared<BoundaryFaceSet>(BC));  // D:43
        eil::Array<size_t> face_ID_list;                   // D:44
        eil::Array<int> outsign_list;                      // D:45
        for (size_t i = 0; i < n[list_index]; ++i) {       // D:46
            face_ID_list.append(index);                    // D:47
            outsign_list.append(-1);                       // D:48
            ++index;                                       // D:49
        }
        boundaries.back()->face_id_list = face_ID_list;    // D:51
        boundaries.back()->outsign_list = outsign_list;    // D:52
        ++list_index;                                      // D:53
    }
    return boundaries;                                     // D:55
}

} // namespace

int main() {
    /*notes:                                    // D:61-76, kept verbatim
        - define boundary by points connected by paths from gpath
        - boundary must be anti-clockwise
        - boundary must contain an even number of nodes
        - try to avoid large jumps in cell size

      P3--------<--------P2
       |                 |
       |                 |
       v   (clockwise)   ^
       |                 |
       |                 |
      PO-------->--------P1

    */

    //points:                                  // D:78
    Vector3 P0 = Vector3(-0.5, 0);             // D:79
    Vector3 P1 = Vector3(0, 0);                // D:80
    Vector3 P2 = Vector3(0.5, 1.0);            // D:81
    Vector3 P3 = Vector3(3, 3);                // D:82
    Vector3 P4 = Vector3(3, 5.5);              // D:83
    Vector3 P5 = Vector3(0, 1.5);              // D:84

    Vector3 M0 = Vector3(0.108, 0.54);         // D:86
    Vector3 M1 = Vector3(-0.3, 1.04);          // D:87

    //paths:                            node counts:      boundary conditions:   // D:90
    auto P0P1 = std::make_shared<Line>(P0, P1);       size_t n0 = 10;  std::string BC0 = "slip-wall";           // D:91
    auto P1P2 = std::make_shared<Arc3>(P1, M0, P2);   size_t n1 = 30;  std::string BC1 = "slip-wall";           // D:92
    auto P2P3 = std::make_shared<Line>(P2, P3);       size_t n2 = 50;  std::string BC2 = "slip-wall";           // D:93
    auto P3P4 = std::make_shared<Line>(P3, P4);       size_t n3 = 30;  std::string BC3 = "outflow-boundary";    // D:94
    auto P4P5 = std::make_shared<Line>(P4, P5);       size_t n4 = 70;  std::string BC4 = "inflow-boundary";     // D:95
    auto P5P0 = std::make_shared<Arc3>(P5, M1, P0);   size_t n5 = 30;  std::string BC5 = "inflow-boundary";     // D:96

    // construct the list of boundary points:      // D:99
    eil::Array<Vector3> boundary_points = construct_boundary(  // D:100-101
        {P0P1, P1P2, P2P3, P3P4, P4P5, P5P0}, {n0, n1, n2, n3, n4, n5});

    // construct the boundary condition sets:      // D:103
    eil::Array<std::shared_ptr<BoundaryFaceSet>> boundaries = construct_boundary_faces(  // D:104-105
        {BC0, BC1, BC2, BC3, BC4, BC5}, {n0, n1, n2, n3, n4, n5});

    // use the old paver constructor to make an unstructured grid:   // D:108
    auto grid = std::make_shared<UnstructuredGrid>(boundary_points, boundaries, "PavedGrid1");  // D:109
    grid->write_to_vtk_file("paved_grid.vtk");   // D:110
    return 0;
} // end main()
