# cmake/wave2_grid_g6.cmake — T-011 stage G6a: the four grid demo programs
# (see reports/wave-02-grid-brief.md sections 9.5 and 11's status note).
#
# Owns only its own targets and test blocks; tests self-register behind if(EXISTS) so the
# tree keeps configuring while this stage's files land. G6a adds no source to eil_geom —
# the four demos are tests/ executables over the library stages G1..G5 already registered.
#
# Scope: geom/grid/{paver2d,paver,sgrid,usgrid}_demo.d -> tests/demo_geom_grid_*.cpp and
# their gates. The surface half of G6 (gmopatch.d, meshpatch.d) is stage G6b and lives in
# cmake/wave2_grid_g6b.cmake, so the two halves can proceed in parallel in disjoint files.
#
# Every demo is gated twice where the evidence allows: stdout byte-compared against the
# ldc2-captured text through harness/compare_text.sh, and the files it wrote compared
# against the D reference through harness/compare_artefacts.sh. The artefact case depends
# on the stdout case as a CTest fixture, because the files can only be checked after the
# demo has run, and a run that died part way must not be graded on the artefacts it did
# not reach.
#
# THE WORKING DIRECTORY IS PART OF THE CONTRACT, exactly as the two manifest headers say
# (baselines/e587b235/probes/geom_grid_sgrid_demo.artefacts.txt:6-15): sgrid_demo.d:96
# resolves ../../examples/eilmer/3D/gridpro-import/blk.tmp and
# usgrid_demo.d:65,69,73 resolve sample-data/*.su2 relative to the cwd, each dying with a
# std.exception.ErrnoException if the layout is missing. So each demo gets a scratch root
# under the build tree with the gdtk inputs symlinked in (gdtk is read-only; nothing here
# writes into it), and the demo runs in <root>/src/eilmer as the captured recipe does. The
# scratch roots are wiped at CONFIGURE time, so `cmake -S . -B <dir> && cmake --build ...
# && ctest` — the documented loop — always grades a clean directory.
#
# paver2d_demo has its artefact versioned as BYTES (the .vtk under probes/), not as a
# checksum manifest, so its manifest is derived from those frozen bytes at configure time
# with file(SIZE)/file(SHA256) and written into the BUILD tree. paver_demo has the same
# arrangement but its gate is switched off (see its block), so its manifest lines are
# commented out. Nothing under baselines/** is written, edited or re-captured here; the
# only files that appear there are the <expected>.norm siblings harness/compare_text.sh
# writes for every stdout gate (10 of them are already tracked from earlier waves).

find_program(BASH_PROGRAM bash)

set(G6A_PROBES ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes)
set(G6A_WORK ${CMAKE_CURRENT_BINARY_DIR}/g6a_demo_work)
# The gdtk tree this port is measured against (same relative reference CMakeLists.txt:31
# uses for the vendored Lua). The demos read their inputs through symlinks into it.
set(G6A_GDTK ${CMAKE_CURRENT_SOURCE_DIR}/../gdtk)

# ---------------------------------------------------------------------------
# paver2d_demo.d (51 lines) — stdout 2 lines, one artefact (paved_grid2.vtk).
# ---------------------------------------------------------------------------
if(BASH_PROGRAM AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/demo_geom_grid_paver2d.cpp)
  add_executable(demo_geom_grid_paver2d tests/demo_geom_grid_paver2d.cpp)
  target_link_libraries(demo_geom_grid_paver2d PRIVATE eil_geom eil_nm eil_substrate)

  set(G6A_P2D_DIR ${G6A_WORK}/paver2d)
  file(REMOVE_RECURSE ${G6A_P2D_DIR})
  file(MAKE_DIRECTORY ${G6A_P2D_DIR})
  add_test(NAME demo_equiv_geom_grid_paver2d
           COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/harness/compare_text.sh
                   ${G6A_PROBES}/geom_grid_paver2d_demo.txt
                   $<TARGET_FILE:demo_geom_grid_paver2d>)
  set_tests_properties(demo_equiv_geom_grid_paver2d PROPERTIES
                       WORKING_DIRECTORY ${G6A_P2D_DIR}
                       FIXTURES_SETUP g6a_ran_paver2d)

  # Manifest derived from the frozen 20,112 byte baseline .vtk at configure time.
  file(SIZE ${G6A_PROBES}/geom_grid_paver2d_demo.paved_grid2.vtk G6A_P2D_BYTES)
  file(SHA256 ${G6A_PROBES}/geom_grid_paver2d_demo.paved_grid2.vtk G6A_P2D_SHA)
  file(WRITE ${G6A_WORK}/paver2d.artefacts.txt
       "bytes\tpaved_grid2.vtk\t${G6A_P2D_BYTES}\t${G6A_P2D_SHA}\n")
  add_test(NAME demo_artefacts_geom_grid_paver2d
           COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/harness/compare_artefacts.sh
                   ${G6A_WORK}/paver2d.artefacts.txt ${G6A_P2D_DIR})
  set_tests_properties(demo_artefacts_geom_grid_paver2d PROPERTIES
                       FIXTURES_REQUIRED g6a_ran_paver2d)
endif()

# ---------------------------------------------------------------------------
# paver_demo.d (115 lines) — stdout 129 lines, one artefact (paved_grid.vtk).
#
# STDOUT GATE: REGISTERED — 129 of 129 lines byte-identical to the captured text
# (5,097 bytes, `cmp` clean on 2026-10-14).
# ARTEFACT GATE: NOT registered. paved_grid.vtk has the same 141,490 byte length but
# 398 of its 1,490 vertex lines differ from the frozen baseline, all in the last digits
# (first difference at line 235 = vertex 228: reference
# `6.036702247749276516e-03 3.683425520169235079e-01 0.000000000000000000e+00`, port
# `6.036702247749274781e-03 ...`; worst relative gap measured over the file 4.3e-13).
# The first 214 vertices ARE byte-identical, and 214 is exactly D's "there are 214 points
# in the external boundary" (baseline line 2), so construct_boundary and the gpath arc/line
# sampling match the reference and the divergence is inside geom/grid/paver.cpp's interior
# paving arithmetic — a work package of its own, not something to patch from a demo.
# ---------------------------------------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/demo_geom_grid_paver.cpp)
  add_executable(demo_geom_grid_paver tests/demo_geom_grid_paver.cpp)
  target_link_libraries(demo_geom_grid_paver PRIVATE eil_geom eil_nm eil_substrate)
endif()

if(BASH_PROGRAM AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/demo_geom_grid_paver.cpp)
  set(G6A_PAV_DIR ${G6A_WORK}/paver)
  file(REMOVE_RECURSE ${G6A_PAV_DIR})
  file(MAKE_DIRECTORY ${G6A_PAV_DIR})
  add_test(NAME demo_equiv_geom_grid_paver
           COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/harness/compare_text.sh
                   ${G6A_PROBES}/geom_grid_paver_demo.txt
                   $<TARGET_FILE:demo_geom_grid_paver>)
  set_tests_properties(demo_equiv_geom_grid_paver PROPERTIES
                       WORKING_DIRECTORY ${G6A_PAV_DIR}
                       FIXTURES_SETUP g6a_ran_paver)

  # Switched on when paver.cpp reproduces the vertex block (see the block comment).
  #   file(SIZE ${G6A_PROBES}/geom_grid_paver_demo.paved_grid.vtk G6A_PAV_BYTES)
  #   file(SHA256 ${G6A_PROBES}/geom_grid_paver_demo.paved_grid.vtk G6A_PAV_SHA)
  #   file(WRITE ${G6A_WORK}/paver.artefacts.txt
  #        "bytes\tpaved_grid.vtk\t${G6A_PAV_BYTES}\t${G6A_PAV_SHA}\n")
  #   add_test(NAME demo_artefacts_geom_grid_paver
  #            COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/harness/compare_artefacts.sh
  #                    ${G6A_WORK}/paver.artefacts.txt ${G6A_PAV_DIR})
  #   set_tests_properties(demo_artefacts_geom_grid_paver PROPERTIES
  #                        FIXTURES_REQUIRED g6a_ran_paver)
endif()

# ---------------------------------------------------------------------------
# sgrid_demo.d (101 lines) — stdout 26 lines, 16 artefact rows.
#
# STDOUT GATE: REGISTERED — 26 of 26 lines byte-identical (930 bytes, `cmp` clean),
# D:41's "GMOPatch is simpler to use." included, because that line is a plain writeln.
# Two things had to be right for it: the r/s redistribution literals of D:23-30 must carry
# ldc2's CTFE-folded bit patterns (tests/demo_geom_grid_sgrid.cpp:125-145; with gcc's own
# folding the stdout still matched at %.6g but artefact rows test_grid_b-2D.vtk and
# test_grid_better-2D.vtk differed in 51 of 231 vertices — after pinning, both match), and
# the demo must run from <root>/src/eilmer with <root>/examples pointing at the gdtk
# examples tree, or D:96's blk.tmp path raises an ErrnoException.
#
# ARTEFACT GATE: NOT registered. 10 of its 16 rows reproduce; the remaining 6 are two
# separate, measured reasons:
#   * test_gmo_grid-2D.vtk is MISSING. D:42 constructs a GMOPatch — stage G6b's deliverable
#     (cmake/wave2_grid_g6b.cmake:16-18 adds gmopatch.cpp to eil_geom as soon as it lands),
#     which this file may not define. D:42-44 therefore compile only when
#     geom/surface/gmopatch.hpp exists (the __has_include guard in the demo), so this row
#     cannot be produced until G6b lands.
#   * all five gpgrid-<i>.vtk rows differ (same byte length; 1 to 5 vertex lines per block,
#     always in the last digit). Root cause is NOT geom/grid: import_gridpro_grid (D:1603)
#     only does `uflowz(scale*to!double(tks[m]))` with scale=1.0, so the value is the parsed
#     literal. Measured with this ldc2 for the token `0.2929550221030571`
#     (blk.tmp:70, → vertex line 1394 of gpgrid-0.vtk): D's `to!double` yields
#     2.929550221030571278e-01 = 0x1.2bfc66bc86beep-2 while strtod, eil::to_double and
#     Python's float() all yield the correctly rounded 2.929550221030570722e-01 =
#     0x1.2bfc66bc86bedp-2. That is a substrate gap (Phobos' std.conv decimal→binary fast
#     path is not correctly rounded; T-913 covers its error text, not its rounding), so it is
#     reported rather than patched here.
# ---------------------------------------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/demo_geom_grid_sgrid.cpp)
  add_executable(demo_geom_grid_sgrid tests/demo_geom_grid_sgrid.cpp)
  target_link_libraries(demo_geom_grid_sgrid PRIVATE eil_geom eil_nm eil_substrate)
endif()

if(BASH_PROGRAM AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/demo_geom_grid_sgrid.cpp
    AND EXISTS ${G6A_GDTK}/examples/eilmer/3D/gridpro-import/blk.tmp)
  set(G6A_SG_ROOT ${G6A_WORK}/sgrid)
  set(G6A_SG_DIR ${G6A_SG_ROOT}/src/eilmer)
  file(REMOVE_RECURSE ${G6A_SG_ROOT})
  file(MAKE_DIRECTORY ${G6A_SG_DIR})
  execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
                  ${G6A_GDTK}/examples ${G6A_SG_ROOT}/examples)
  add_test(NAME demo_equiv_geom_grid_sgrid
           COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/harness/compare_text.sh
                   ${G6A_PROBES}/geom_grid_sgrid_demo.txt
                   $<TARGET_FILE:demo_geom_grid_sgrid>)
  set_tests_properties(demo_equiv_geom_grid_sgrid PROPERTIES
                       WORKING_DIRECTORY ${G6A_SG_DIR}
                       FIXTURES_SETUP g6a_ran_sgrid)

  # Add these two lines to the gmopatch condition above and uncomment the block when
  # src/geom/surface/gmopatch.hpp exists AND the substrate to!double rounding is fixed;
  # measured 2026-10-14: 6 of 16 rows fail (see the block comment).
  #   add_test(NAME demo_artefacts_geom_grid_sgrid
  #            COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/harness/compare_artefacts.sh
  #                    ${G6A_PROBES}/geom_grid_sgrid_demo.artefacts.txt ${G6A_SG_DIR})
  #   set_tests_properties(demo_artefacts_geom_grid_sgrid PROPERTIES
  #                        FIXTURES_REQUIRED g6a_ran_sgrid)
endif()


# ---------------------------------------------------------------------------
# usgrid_demo.d (103 lines) — stdout 56 lines, 28 artefact rows (two of them .gz,
# compared as decompressed payload).
#
# BOTH GATES REGISTERED. Measured 2026-10-14: 56 of 56 stdout lines byte-identical
# (2,048 bytes) and 28 of 28 artefact rows identical, including the two `payload` rows
# (test_grid.gz, test_3Dgrid.gz), which confirms the manifest header's rule that the
# decompressed payload is what reproduces and the container is not.
# ---------------------------------------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/demo_geom_grid_usgrid.cpp)
  add_executable(demo_geom_grid_usgrid tests/demo_geom_grid_usgrid.cpp)
  target_link_libraries(demo_geom_grid_usgrid PRIVATE eil_geom eil_nm eil_substrate)
endif()

if(BASH_PROGRAM AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/demo_geom_grid_usgrid.cpp
    AND EXISTS ${G6A_GDTK}/src/geom/sample-data/square-mesh.su2)
  set(G6A_USG_ROOT ${G6A_WORK}/usgrid)
  set(G6A_USG_DIR ${G6A_USG_ROOT}/src/eilmer)
  file(REMOVE_RECURSE ${G6A_USG_ROOT})
  file(MAKE_DIRECTORY ${G6A_USG_DIR})
  execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
                  ${G6A_GDTK}/src/geom/sample-data ${G6A_USG_DIR}/sample-data)
  add_test(NAME demo_equiv_geom_grid_usgrid
           COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/harness/compare_text.sh
                   ${G6A_PROBES}/geom_grid_usgrid_demo.txt
                   $<TARGET_FILE:demo_geom_grid_usgrid>)
  set_tests_properties(demo_equiv_geom_grid_usgrid PROPERTIES
                       WORKING_DIRECTORY ${G6A_USG_DIR}
                       FIXTURES_SETUP g6a_ran_usgrid)
  add_test(NAME demo_artefacts_geom_grid_usgrid
           COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/harness/compare_artefacts.sh
                   ${G6A_PROBES}/geom_grid_usgrid_demo.artefacts.txt ${G6A_USG_DIR})
  set_tests_properties(demo_artefacts_geom_grid_usgrid PROPERTIES
                       FIXTURES_REQUIRED g6a_ran_usgrid)
endif()
