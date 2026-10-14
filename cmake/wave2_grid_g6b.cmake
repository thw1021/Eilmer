# cmake/wave2_grid_g6b.cmake — T-011 stage G6b (see reports/wave-02-grid-brief.md section 0).
#
# Split off wave2_grid_g6.cmake so the two halves of G6 can proceed in parallel without two
# writers in one file: G6a owns wave2_grid_g6.cmake (the four grid demos), this fragment owns
# only surface/gmopatch.d (171 D lines) and surface/meshpatch.d (100) -- both were moved out
# of T-012 into T-011 because gmopatch.d:87 constructs a StructuredGrid and meshpatch.d:23
# takes `ref StructuredGrid grid`, so neither could be ported before G4 landed.
#
# Scope: geom/surface/gmopatch.{d,hpp,cpp}, geom/surface/meshpatch.{d,hpp,cpp}, their probes
# and corpora under probes/geom/surface/ + baselines/e587b235/probes/, and their replay tests.
#
# Registration is all-or-nothing as everywhere in this wave: a corpus gate is only added with
# add_test() once the whole corpus replays at 0 differ, and add_executable is guarded by
# if(EXISTS) so the tree keeps configuring while this stage's files land.

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/geom/surface/gmopatch.cpp)
  target_sources(eil_geom PRIVATE src/geom/surface/gmopatch.cpp)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/geom/surface/meshpatch.cpp)
  target_sources(eil_geom PRIVATE src/geom/surface/meshpatch.cpp)
endif()

# ---------------------------------------------------------------------------
# Replay tests. add_executable is live so the tree keeps building either way;
# add_test stays commented until the WHOLE corpus replays at 0 differ (the
# all-or-nothing rule of this wave, wave-02-grid-brief 10.5). Both blocks were
# flipped on only after the counts quoted in the test output below were measured.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_geom_surface_gmopatch.cpp)
  add_executable(test_geom_surface_gmopatch tests/test_geom_surface_gmopatch.cpp)
  target_link_libraries(test_geom_surface_gmopatch PRIVATE eil_geom eil_nm eil_substrate)
  # The probe constructs a dozen GMOPatch objects, each of which runs a Nelder-Mead
  # redistribution over its own test grid, so the gate gets an explicit timeout: a hang
  # must report as a failure rather than as a truncated corpus.
  add_test(NAME geom_surface_gmopatch
           COMMAND test_geom_surface_gmopatch
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_surface_gmopatch.txt)
  set_tests_properties(geom_surface_gmopatch PROPERTIES TIMEOUT 600)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_geom_surface_meshpatch.cpp)
  add_executable(test_geom_surface_meshpatch tests/test_geom_surface_meshpatch.cpp)
  target_link_libraries(test_geom_surface_meshpatch PRIVATE eil_geom eil_nm eil_substrate)
  add_test(NAME geom_surface_meshpatch
           COMMAND test_geom_surface_meshpatch
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_surface_meshpatch.txt)
endif()

# ---------------------------------------------------------------------------
# MEASUREMENT FOR G6a (this file does not edit wave2_grid_g6.cmake): landing
# src/geom/surface/gmopatch.hpp unblocks the `__has_include` guard in
# tests/demo_geom_grid_sgrid.cpp:83-85, so D:42-44 now compile and the demo writes the
# one artefact row its own header called MISSING. Measured in a fresh build tree against
# the frozen manifest baselines/e587b235/probes/geom_grid_sgrid_demo.artefacts.txt:
#   test_gmo_grid-2D.vtk  17,420 bytes, sha256 e4cbfa71...223edddf  IDENTICAL to the
#                         manifest row (and the sgrid demo's 26 stdout lines still replay
#                         byte-identical, so the stdout gate did not move).
#   the sgrid artefact manifest is therefore 11 of 16 rows reproducing, not 10 of 16: the 5
#   gpgrid-<i>.vtk rows are the ldc2 `to!double` rounding gap (T-930), which this stage may
#   not fix and did not touch. The artefact gate stays unregistered for that reason alone.
# ---------------------------------------------------------------------------
