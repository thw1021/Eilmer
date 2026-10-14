// tests/demo_geom_grid_sgrid.cpp
//
// Port of geom/grid/sgrid_demo.d (101 lines) — the directory-level demo of
// StructuredGrid: surface- and volume-generated grids, internal r,s redistribution,
// GMOPatch-driven generation, gzip/raw-binary round trips, slab and wedge extrusions,
// bin-sorted cell location, a boundary sub-grid and a GridPro import.
//
// Gates: stdout = baselines/e587b235/probes/geom_grid_sgrid_demo.txt (26 lines) through
// harness/compare_text.sh (CTest name demo_equiv_geom_grid_sgrid) — measured 26 of 26
// lines byte-identical. artefacts =
// baselines/e587b235/probes/geom_grid_sgrid_demo.artefacts.txt (16 rows: 15 `bytes`
// plus one `payload` row for the .gz, whose digest/size are of the DECOMPRESSED payload
// because the gzip container embeds the entry mtime) through
// harness/compare_artefacts.sh — 10 of the 16 rows reproduce, so that gate is NOT
// registered (see cmake/wave2_grid_g6.cmake for the two measured reasons: the missing
// test_gmo_grid-2D.vtk, and 1-5 vertex lines per gpgrid block off by one binary ULP
// because Phobos' `to!double` is not correctly rounded where the port's strtod is).
//
// THE WORKING DIRECTORY IS PART OF THE CONTRACT (spelled out in the manifest header at
// baselines/e587b235/probes/geom_grid_sgrid_demo.artefacts.txt:7-15): D:96 resolves
// `../../examples/eilmer/3D/gridpro-import/blk.tmp` relative to the cwd and dies with a
// std.exception.ErrnoException if it is not there, so the demo is run from
// <root>/src/eilmer with <root>/examples symlinked to the gdtk examples tree. The port
// does the same (it passes D's literal path through unchanged), and CTest supplies the
// directory via WORKING_DIRECTORY; nothing about that path reaches stdout.
//
// D source order is preserved statement for statement. The differences are spelling
// only:
//   * `import geom;` (D:5) → the module headers actually used.
//   * adapted-handles: StructuredGrid/AOPatch/TFIVolume/UnivariateFunction handles
//     become std::shared_ptr, so constructor arguments that D passes as handles are
//     passed as `*handle` (src/geom/grid/sgrid.hpp:26-28) and `my_grid[...]` becomes
//     `my_grid->opIndex(...)`.
//   * `Vector3[8] p` (D:64) becomes eil::Array<Vector3> p(8), which value-initializes
//     the elements exactly as D's static array does (Vector3.init); `new TFIVolume(p)`
//     takes the D slice as eil::Slice<Vector3>, the T-012 mapping for a static array
//     argument (tests/demo_geom_volume_volume.cpp:18-20).
//   * `writeln(..., *my_3Dgrid[5,5,5])` prints a Vector3 BY VALUE; eil::writeln has no
//     case for a class with a toString() method (substrate/include/eil/format.hpp:140
//     is a hard static_assert; backlog T-926 item 1), so every Vector3 is written
//     through the explicit .toString(). The bytes are the same: baseline line 13 is
//     `grid point 5 5 5 at p=Vector3(0.5, 0.35, 1.5)`.
//   * `writeln("r_grid=", r_grid)` (D:37-38) prints a static array of static arrays.
//     eil::to_string renders any indexable container as `[e1, e2]` with elements
//     through to!string (substrate/include/eil/format.hpp:130-139), which for
//     RSGrid = std::array<std::array<double,4>,4> (src/geom/grid/sgrid.hpp:93) gives
//     baseline line 7's `[[0, 0, 0, 0], [0.588865, ...], ...]` byte for byte.
//   * `"gpgrid-"~to!string(i)~".vtk"` (D:98) → eil::to_string on the same size_t index.
//   * The three GMOPatch statements (D:42-44) are compiled only when
//     geom/surface/gmopatch.hpp exists: GMOPatch is stage G6b's deliverable
//     (cmake/wave2_grid_g6b.cmake:16-18) and is not in this tree yet, and this file may
//     not define it. D:41's writeln is outside the guard, so all 26 stdout lines match
//     today; only the test_gmo_grid-2D.vtk artefact row is missing, and it is the reason
//     the artefact gate stays off.
//
// Run: cd <root>/src/eilmer && ./demo_geom_grid_sgrid
//
// Regenerate the evidence (verified reproducible by two runs from different scratch
// roots — manifest header lines 12-15):
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip /home/tang/packages/gdtk/src/geom/grid/sgrid_demo.d -of=/tmp/demo_g6/sgrid_demo -od=/tmp/demo_obj_g6
//   mkdir -p /tmp/g6/src/eilmer && ln -s /home/tang/packages/gdtk/examples /tmp/g6/examples
//   ln -s /home/tang/packages/gdtk/src/geom/sample-data /tmp/g6/src/eilmer/sample-data
//   cd /tmp/g6/src/eilmer && /tmp/demo_g6/sgrid_demo > out.txt
#include <array>
#include <memory>
#include <string>

#include "eil/eil.hpp"
#include "eil/array.hpp"
#include "eil/format.hpp"
#include "geom/elements/nomenclature.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/grid/sgrid.hpp"
#include "geom/misc/univariatefunctions.hpp"
#include "geom/surface/aopatch.hpp"
#include "geom/volume/tfivolume.hpp"

// Stage G6b owns geom/surface/gmopatch.{hpp,cpp}; until it lands, D:42-44 cannot be
// compiled here (see the header note). __has_include keeps this file complete.
#if defined(__has_include)
#  if __has_include("geom/surface/gmopatch.hpp")
#    include "geom/surface/gmopatch.hpp"
#    define EIL_G6A_HAVE_GMOPATCH 1
#  endif
#endif
#ifndef EIL_G6A_HAVE_GMOPATCH
#  define EIL_G6A_HAVE_GMOPATCH 0
#endif

namespace {

using eil::geom::AOPatch;
using eil::geom::ClusterF;
using eil::geom::Face;
using eil::geom::LinearFunction;
using eil::geom::RSGrid;
using eil::geom::StructuredGrid;
using eil::geom::TFIVolume;
using eil::geom::Vector3;

} // namespace

int main() {
    eil::writeln("Begin structured-grid demo...");                    // D:9
    auto p00 = Vector3(0.0, 0.1);                                     // D:10
    auto p10 = Vector3(1.0, 0.4);                                     // D:11
    auto p11 = Vector3(1.0, 1.1);                                     // D:12
    auto p01 = Vector3(0.0, 1.1);                                     // D:13
    auto my_patch = std::make_shared<AOPatch>(p00, p10, p11, p01);    // D:14
    ClusterF cf;                                                      // D:15-16
    cf.append(std::make_shared<LinearFunction>());
    cf.append(std::make_shared<LinearFunction>());
    cf.append(std::make_shared<LinearFunction>());
    cf.append(std::make_shared<LinearFunction>());
    auto my_grid = std::make_shared<StructuredGrid>(*my_patch, 11, 21, cf, "linear");  // D:17
    eil::writeln("grid point 5 5 at x=", my_grid->opIndex(5, 5)->x,   // D:18
                 " y=", my_grid->opIndex(5, 5)->y);
    my_grid->write_to_vtk_file("test_grid-2D.vtk");                   // D:19
    eil::writeln("badness=", my_grid->measure_of_badness());          // D:20

    eil::writeln("Start playing with internal redistribution of r,s parameters.");  // D:22
    // ldc2 evaluates the constant expressions of D:23-30 during CTFE in 80-bit `real` and
    // narrows on store, so two of the eight distinct entries are NOT what gcc folds from
    // the same text. Measured with this ldc2 1.40.0 by printing D's initialized arrays with
    // %a: `1.0/3` = 0x1.5555555555555p-2 and `2.0/3` = 0x1.5555555555555p-1 (gcc agrees),
    // but `1.0/3-0.3` = 0x1.1111111111111p-5 (gcc: 0x1.111111111111p-5) and `2.0/3+0.3` =
    // 0x1.eeeeeeeeeeeefp-1 (gcc: 0x1.eeeeeeeeeeeeep-1). The two divergent entries are written
    // as D's bit patterns, the same device src/geom/grid/paver.hpp:48-62,:201 uses for
    // paver.d's two folded expressions (backlog T-909). Evidence that this is observable and
    // that pinning it is required: with gcc's folded values, artefact rows
    // test_grid_b-2D.vtk (sha256 cfb59729... expected, 434e32c4... produced) and
    // test_grid_better-2D.vtk differ in 51 of 231 vertices, while stdout lines 5-8 agree
    // either way because %.6g hides a few ULP.
    RSGrid r_grid = {{{0.0, 1.0 / 3, 2.0 / 3, 1.0},                                // D:23
                      {0.0, 0x1.1111111111111p-5, 0x1.eeeeeeeeeeeefp-1, 1.0},      // D:24
                      {0.0, 0x1.1111111111111p-5, 0x1.eeeeeeeeeeeefp-1, 1.0},      // D:25
                      {0.0, 1.0 / 3, 2.0 / 3, 1.0}}};                              // D:26
    RSGrid s_grid = {{{0.0, 0.0, 0.0, 0.0},                                        // D:27
                      {1.0 / 3, 0x1.1111111111111p-5, 0x1.1111111111111p-5, 1.0 / 3},   // D:28
                      {2.0 / 3, 0x1.eeeeeeeeeeeefp-1, 0x1.eeeeeeeeeeeefp-1, 2.0 / 3},   // D:29
                      {1.0, 1.0, 1.0, 1.0}}};                                      // D:30
    auto my_grid_b = std::make_shared<StructuredGrid>(*my_patch, 11, 21, cf, r_grid,
                                                      s_grid);          // D:31
    eil::writeln("grid point 5 5 at x=", my_grid_b->opIndex(5, 5)->x,  // D:32
                 " y=", my_grid_b->opIndex(5, 5)->y);
    my_grid_b->write_to_vtk_file("test_grid_b-2D.vtk");               // D:33
    eil::writeln("initial badness=", my_grid_b->measure_of_badness()); // D:34
    my_grid_b->determine_rs_grids(*my_patch, cf, r_grid, s_grid);      // D:35
    eil::writeln("final badness=", my_grid_b->measure_of_badness());   // D:36
    eil::writeln("r_grid=", r_grid);                                   // D:37
    eil::writeln("s_grid=", s_grid);                                   // D:38
    my_grid_b->write_to_vtk_file("test_grid_better-2D.vtk");           // D:39

    eil::writeln("GMOPatch is simpler to use.");                       // D:41
#if EIL_G6A_HAVE_GMOPATCH
    auto gmo_patch = std::make_shared<eil::geom::GMOPatch>(p00, p10, p11, p01, 11, 21);  // D:42
    auto gmo_grid = std::make_shared<StructuredGrid>(*gmo_patch, 11, 21, cf);            // D:43
    gmo_grid->write_to_vtk_file("test_gmo_grid-2D.vtk");                                  // D:44
#endif

    // write then read standard gzip format                             // D:46
    my_grid->write_to_gzip_file("test_grid-2D.gz");                     // D:47
    auto my_grid2 = std::make_shared<StructuredGrid>("test_grid-2D.gz", "gziptext");  // D:48
    my_grid2->write_to_vtk_file("test_grid2-2D.vtk");                   // D:49

    // write then read raw binary                                       // D:51
    my_grid->write_to_raw_binary_file("test_grid-2D.bin");              // D:52
    auto my_grid2b = std::make_shared<StructuredGrid>("test_grid-2D.bin", "rawbinary");  // D:53
    my_grid2b->write_to_vtk_file("test_grid2b-2D.vtk");                 // D:54

    eil::writeln("SlabGrid");                                           // D:56
    auto my_grid3 = my_grid->makeSlabGrid(0.2);                         // D:57
    my_grid3->write_to_vtk_file("test_grid3-slab.vtk");                 // D:58
    eil::writeln("WedgeGrid");                                          // D:59
    auto my_grid4 = my_grid->makeWedgeGrid(0.2);                        // D:60
    my_grid4->write_to_vtk_file("test_grid4-wedge.vtk");                // D:61

    eil::writeln("3D grid from the start");                             // D:63
    eil::Array<Vector3> p(8);                                           // D:64
    p[0] = Vector3(0.0, 0.1, 0.0);                                      // D:65
    p[1] = Vector3(1.0, 0.1, 0.0);                                      // D:66
    p[2] = Vector3(1.0, 1.1, 0.0);                                      // D:67
    p[3] = Vector3(0.0, 1.1, 0.0);                                      // D:68
    //
    p[4] = Vector3(0.0, 0.1, 3.0);                                      // D:70
    p[5] = Vector3(1.0, 0.1, 3.0);                                      // D:71
    p[6] = Vector3(1.0, 1.1, 3.0);                                      // D:72
    p[7] = Vector3(0.0, 1.1, 3.0);                                      // D:73
    //
    auto simple_box = std::make_shared<TFIVolume>(p.all());             // D:75
    auto my_3Dgrid = std::make_shared<StructuredGrid>(*simple_box, 11, 21, 11, cf);  // D:76
    eil::writeln("grid point 5 5 5 at p=", my_3Dgrid->opIndex(5, 5, 5)->toString());  // D:77
    my_3Dgrid->write_to_vtk_file("test_grid-3D.vtk");                   // D:78
    my_3Dgrid->sort_cells_into_bins(10, 10, 10);                        // D:79
    Vector3 my_point = 0.5 * (p[0] + p[7]);                             // D:80
    size_t cell_indx = 0;                                               // D:81
    bool found = false;                                                 // D:81
    my_3Dgrid->find_enclosing_cell(my_point, cell_indx, found);         // D:82
    eil::writeln("Search for cell enclosing my_point= ", my_point.toString());  // D:83
    if (found) {                                                        // D:84
        eil::writeln("    cell found, index= ", cell_indx);             // D:85
        eil::writeln("    cell barycentre= ",
                     my_3Dgrid->cell_barycentre(cell_indx).toString()); // D:86
    } else {                                                            // D:87
        eil::writeln("    cell not found");                             // D:88
    }
    //
    eil::writeln("2D surface from the 3D grid");                        // D:91
    auto north_grid = my_3Dgrid->get_boundary_grid(Face::north);        // D:92
    eil::writeln("grid point 5 5 at p=", north_grid->opIndex(5, 5)->toString());  // D:93

    eil::writeln("Import GridPro grid...");                             // D:95
    auto gpgrid = eil::geom::import_gridpro_grid(                       // D:96
        "../../examples/eilmer/3D/gridpro-import/blk.tmp");
    for (size_t i = 0; i < gpgrid.length(); ++i) {                      // D:97
        gpgrid[i]->write_to_vtk_file("gpgrid-" + eil::to_string(i) + ".vtk");  // D:98
    }
    eil::writeln("Done.");                                              // D:100
    return 0;
}
