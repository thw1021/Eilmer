# cmake/wave2_grid_g2.cmake — T-011 stage G2 (see reports/wave-02-grid-brief.md).
#
# Owns only its own target_sources(eil_geom ...) and test blocks; tests self-register
# behind if(EXISTS) so the tree keeps configuring while this stage's files land.
#
# Scope: geom/grid/paver.d (the 2D quad paver, 1737 D lines) ->
# src/geom/grid/paver.{hpp,cpp}, probes/geom/grid/probe_paver.d,
# baselines/e587b235/probes/geom_grid_paver.txt, tests/test_geom_grid_paver.cpp.
# paver2d/sgrid/usgrid and the demos belong to G3..G6 in their own fragments.
# Note that geom/grid/grid.{hpp,cpp} (G1) is a dependency of nothing here: paver.d's
# `class PavedGrid` (paver.d:1598) has no base class, so eil_geom only needs the
# substrate and geom/geometry_exception.

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/geom/grid/paver.cpp)
  target_sources(eil_geom PRIVATE src/geom/grid/paver.cpp)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_geom_grid_paver.cpp)
  add_executable(test_geom_grid_paver tests/test_geom_grid_paver.cpp)
  target_link_libraries(test_geom_grid_paver PRIVATE eil_geom eil_nm eil_substrate)
  # The test replays the ldc2-captured probe corpus, so the baseline path is passed as
  # argv[1] (wave-1 lesson 3: a test that reads a data file must be given the path).
  add_test(NAME geom_grid_paver
           COMMAND test_geom_grid_paver
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_grid_paver.txt)
endif()
