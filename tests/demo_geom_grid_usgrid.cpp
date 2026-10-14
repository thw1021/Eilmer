// tests/demo_geom_grid_usgrid.cpp
//
// Port of geom/grid/usgrid_demo.d (103 lines) — the directory-level demo of
// UnstructuredGrid: a 2D grid paved from a StructuredGrid, gzip and raw-binary round
// trips, STL and VTK export, a 3D bin-sorted grid, SU2 and GridPro import, an OpenFOAM
// polyMesh export and a grid join.
//
// Gates: stdout = baselines/e587b235/probes/geom_grid_usgrid_demo.txt (56 lines) through
// harness/compare_text.sh (CTest name demo_equiv_geom_grid_usgrid); artefacts =
// baselines/e587b235/probes/geom_grid_usgrid_demo.artefacts.txt (28 rows: 26 `bytes`
// plus two `payload` rows for the .gz files, whose digest/size are of the DECOMPRESSED
// payload because the gzip container embeds the entry mtime) through
// harness/compare_artefacts.sh (CTest name demo_artefacts_geom_grid_usgrid).
// Measured 2026-10-14: 56 of 56 stdout lines and 28 of 28 artefact rows byte-identical,
// both gates registered and green in default, Release and Release+EIL_SLICE_CHECK.
//
// THE WORKING DIRECTORY IS PART OF THE CONTRACT (manifest header lines 6-15): D:65, :69
// and :73 read `sample-data/square-mesh.su2`, `sample-data/square-mesh-quads.su2` and
// `sample-data/cube-mesh-hex.su2` by relative path and die with a
// std.exception.ErrnoException until the layout exists, so the demo runs from
// <root>/src/eilmer with <root>/src/eilmer/sample-data symlinked to
// gdtk/src/geom/sample-data. The port passes D's literal paths through unchanged and
// CTest supplies the directory via WORKING_DIRECTORY.
//
// D source order is preserved statement for statement. The differences are spelling
// only:
//   * `import geom;` (D:5) → the module headers actually used.
//   * adapted-handles: `new UnstructuredGrid(my_grid)` (D:19) takes the StructuredGrid
//     handle, whose port parameter is a const reference (src/geom/grid/usgrid.hpp:300),
//     so the call passes `*my_grid`; `my_grid[5,5]` becomes `my_grid->opIndex(5,5)`.
//   * `Vector3[8] p` (D:32, :81) becomes eil::Array<Vector3> p(8) — D's static array
//     value-initializes, and `new TFIVolume(p)` takes the slice as eil::Slice<Vector3>
//     (tests/demo_geom_volume_volume.cpp:18-20).
//   * `writeln(..., *my_3Dgrid[5,5,5])` prints a Vector3 BY VALUE; eil::writeln has no
//     case for a class with a toString() method (substrate/include/eil/format.hpp:140,
//     backlog T-926 item 1), so each Vector3 is written through the explicit
//     .toString(). Baseline line 3 is `grid point 5 5 5 at p=Vector3(0.5, 0.35, 1.5)`.
//   * `usg3D_a.joinGrid(usg3D_b)` (D:98) discards D's returned Grid, as D does; the
//     join is observable through the following writeStats (baseline lines 40-56).
//   * Unlike tests/demo_geom_surface_surface.cpp:22-25, this demo takes no argv: D
//     writes all 28 artefacts relative to the cwd, so the artefact directory is the
//     working directory.
//
// Run: cd <root>/src/eilmer && ./demo_geom_grid_usgrid
//
// Regenerate the evidence (verified reproducible by two runs from different scratch
// roots — manifest header lines 12-15):
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip \
//     /home/tang/packages/gdtk/src/geom/grid/usgrid_demo.d \
//     -of=/tmp/demo_g6/usgrid_demo -od=/tmp/demo_obj_g6
//   mkdir -p /tmp/g6/src/eilmer
//   ln -s /home/tang/packages/gdtk/src/geom/sample-data /tmp/g6/src/eilmer/sample-data
//   cd /tmp/g6/src/eilmer && /tmp/demo_g6/usgrid_demo > out.txt
#include <memory>
#include <string>

#include "eil/eil.hpp"
#include "eil/array.hpp"
#include "eil/format.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/grid/sgrid.hpp"
#include "geom/grid/usgrid.hpp"
#include "geom/misc/univariatefunctions.hpp"
#include "geom/surface/aopatch.hpp"
#include "geom/volume/tfivolume.hpp"

namespace {

using eil::geom::AOPatch;
using eil::geom::ClusterF;
using eil::geom::LinearFunction;
using eil::geom::StructuredGrid;
using eil::geom::TFIVolume;
using eil::geom::UnstructuredGrid;
using eil::geom::Vector3;

} // namespace

int main() {
    eil::writeln("Begin unstructured-grid demo...");                    // D:9
    auto p00 = Vector3(0.0, 0.1);                                       // D:10
    auto p10 = Vector3(1.0, 0.4);                                       // D:11
    auto p11 = Vector3(1.0, 1.1);                                       // D:12
    auto p01 = Vector3(0.0, 1.1);                                       // D:13
    auto my_patch = std::make_shared<AOPatch>(p00, p10, p11, p01);      // D:14
    ClusterF cf;                                                        // D:15-16
    cf.append(std::make_shared<LinearFunction>());
    cf.append(std::make_shared<LinearFunction>());
    cf.append(std::make_shared<LinearFunction>());
    cf.append(std::make_shared<LinearFunction>());
    auto my_grid = std::make_shared<StructuredGrid>(*my_patch, 11, 21, cf);  // D:17
    eil::writeln("grid point 5 5 at x=", my_grid->opIndex(5, 5)->x,     // D:18
                 " y=", my_grid->opIndex(5, 5)->y);
    auto usg = std::make_shared<UnstructuredGrid>(*my_grid);            // D:19
    usg->write_to_vtk_file("test_grid.vtk");                            // D:20
    usg->write_to_stl_file("test_grid.stl", 25.4);  // D:21 inches to mm scale

    usg->write_to_gzip_file("test_grid.gz");                            // D:23
    auto usg2 = std::make_shared<UnstructuredGrid>("test_grid.gz", "gziptext", true);  // D:24
    usg2->write_to_vtk_file("test_grid_2.vtk");                         // D:25

    usg->write_to_raw_binary_file("test_grid.bin");                     // D:27
    auto usg2b = std::make_shared<UnstructuredGrid>("test_grid.bin", "rawbinary", true);  // D:28
    usg2b->write_to_vtk_file("test_grid_2b.vtk");                       // D:29

    eil::writeln("3D grid");                                            // D:31
    eil::Array<Vector3> p(8);                                           // D:32
    p[0] = Vector3(0.0, 0.1, 0.0);                                      // D:33
    p[1] = Vector3(1.0, 0.1, 0.0);                                      // D:34
    p[2] = Vector3(1.0, 1.1, 0.0);                                      // D:35
    p[3] = Vector3(0.0, 1.1, 0.0);                                      // D:36
    //
    p[4] = Vector3(0.0, 0.1, 3.0);                                      // D:38
    p[5] = Vector3(1.0, 0.1, 3.0);                                      // D:39
    p[6] = Vector3(1.0, 1.1, 3.0);                                      // D:40
    p[7] = Vector3(0.0, 1.1, 3.0);                                      // D:41
    //
    auto simple_box = std::make_shared<TFIVolume>(p.all());             // D:43
    auto my_3Dgrid = std::make_shared<StructuredGrid>(*simple_box, 11, 21, 11, cf);  // D:44
    eil::writeln("grid point 5 5 5 at p=", my_3Dgrid->opIndex(5, 5, 5)->toString());  // D:45
    auto usg3D = std::make_shared<UnstructuredGrid>(*my_3Dgrid);        // D:46
    usg3D->sort_cells_into_bins(10, 10, 10);                            // D:47
    Vector3 my_point = 0.5 * (p[0] + p[7]);                             // D:48
    size_t cell_indx = 0;                                               // D:49
    bool found = false;                                                 // D:49
    usg3D->find_enclosing_cell(my_point, cell_indx, found);             // D:50
    eil::writeln("Search for cell enclosing my_point= ", my_point.toString());  // D:51
    if (found) {                                                        // D:52
        eil::writeln("    cell found, index= ", cell_indx);             // D:53
        eil::writeln("    cell barycentre= ",
                     usg3D->cell_barycentre(cell_indx).toString());     // D:54
    } else {                                                            // D:55
        eil::writeln("    cell not found");                             // D:56
    }
    usg3D->write_to_vtk_file("test_3Dgrid.vtk");                        // D:58
    usg3D->write_to_gzip_file("test_3Dgrid.gz");                        // D:59
    auto usg3 = std::make_shared<UnstructuredGrid>("test_3Dgrid.gz", "gziptext", true);  // D:60
    usg3->write_to_vtk_file("test_3Dgrid_2.vtk");                       // D:61
    usg3->write_to_vtk_file("test_3Dgrid_2.su2");                       // D:62

    eil::writeln("su2 2D grid -- triangles");                           // D:64
    auto su2grid = std::make_shared<UnstructuredGrid>("sample-data/square-mesh.su2", "su2text", true);  // D:65
    su2grid->write_to_vtk_file("test_su2-square-mesh.vtk");             // D:66
    su2grid->write_to_su2_file("test_su2-square-mesh.su2");             // D:67
    eil::writeln("su2 2D grid -- quadrangles");                         // D:68
    auto su2grid2 = std::make_shared<UnstructuredGrid>("sample-data/square-mesh-quads.su2", "su2text", true);  // D:69
    su2grid2->write_to_vtk_file("test_su2-square-mesh-quads.vtk");      // D:70
    su2grid2->write_to_su2_file("test_su2-square-mesh-quads.su2");      // D:71
    eil::writeln("su2 3D grid -- hexagons");                             // D:72
    auto su2grid3 = std::make_shared<UnstructuredGrid>("sample-data/cube-mesh-hex.su2", "su2text", true);  // D:73
    su2grid3->write_to_vtk_file("test_su2-cube-mesh-hex.vtk");          // D:74
    su2grid3->write_to_su2_file("test_su2-cube-mesh-hex.su2");          // D:75
    su2grid3->write_openFoam_polyMesh("test_openFoam");                 // D:76

    eil::writeln("Try joining grids");                                  // D:78
    auto grid_a = std::make_shared<StructuredGrid>(*simple_box, 3, 3, 4, cf);  // D:79
    auto usg3D_a = std::make_shared<UnstructuredGrid>(*grid_a, "usga");  // D:80
    eil::Array<Vector3> pb(8);                                          // D:81
    pb[0] = Vector3(1.0, 0.1, 0.0);                                     // D:82
    pb[1] = Vector3(2.0, 0.1, 0.0);                                     // D:83
    pb[2] = Vector3(2.0, 1.1, 0.0);                                     // D:84
    pb[3] = Vector3(1.0, 1.1, 0.0);                                     // D:85
    //
    pb[4] = Vector3(1.0, 0.1, 3.0);                                     // D:87
    pb[5] = Vector3(2.0, 0.1, 3.0);                                     // D:88
    pb[6] = Vector3(2.0, 1.1, 3.0);                                     // D:89
    pb[7] = Vector3(1.0, 1.1, 3.0);                                     // D:90
    //
    auto simple_box_b = std::make_shared<TFIVolume>(pb.all());          // D:92
    auto grid_b = std::make_shared<StructuredGrid>(*simple_box_b, 3, 3, 4, cf);  // D:93
    // D:94 reads my_3Dgrid, not grid_b — transcribed, not "fixed".
    eil::writeln("grid point 1 1 2 at p=", my_3Dgrid->opIndex(1, 1, 2)->toString());  // D:94
    auto usg3D_b = std::make_shared<UnstructuredGrid>(*grid_b, "usgb");  // D:95
    usg3D_a->writeStats();                                              // D:96
    usg3D_b->writeStats();                                              // D:97
    usg3D_a->joinGrid(usg3D_b);                                         // D:98
    usg3D_a->writeStats();                                              // D:99
    usg3D_a->write_to_vtk_file("test-join-grid.vtk");                   // D:100

    eil::writeln("Done.");                                              // D:102
    return 0;
}
