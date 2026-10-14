# cmake/wave2_grid_g1.cmake — T-011 stage G1 (see reports/wave-02-grid-brief.md).
#
# Owns only its own target_sources(eil_geom ...) and test blocks; tests self-register
# behind if(EXISTS) so the tree keeps configuring while this stage's files land.
#
# Scope: geom/grid/grid.d (the abstract Grid base) -> src/geom/grid/grid.{hpp,cpp},
# probes/geom/grid/probe_grid.d,
# baselines/e587b235/probes/geom_grid_grid.txt, tests/test_geom_grid_grid.cpp.
# Nothing else in geom/grid is handled here — G2..G6 own paver/paver2d/sgrid/usgrid
# and the demos in their own fragments.

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/geom/grid/grid.cpp)
  target_sources(eil_geom PRIVATE src/geom/grid/grid.cpp)
endif()

# The wave-02-grid-brief build self-check names `--target grid`; that is this stage's
# aggregate (the base class is a component of eil_geom, not a library of its own).
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/geom/grid/grid.cpp)
  add_custom_target(grid DEPENDS eil_geom)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_geom_grid_grid.cpp)
  add_executable(test_geom_grid_grid tests/test_geom_grid_grid.cpp)
  target_link_libraries(test_geom_grid_grid PRIVATE eil_geom eil_nm eil_substrate)
  # The test replays the ldc2-captured probe corpus, so the baseline path is passed as
  # argv[1] (wave-1 lesson 3: a test that reads a data file must be given the path).
  add_test(NAME geom_grid_grid
           COMMAND test_geom_grid_grid
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_grid_grid.txt)
endif()

# T-919: the both-NaN operand-choice contract. geom_grid_grid row 178 was red only in
# Release because an SSE add of two NaNs returns whichever NaN sits in the destination
# register, and that is register allocation, not arithmetic. probes/geom/grid/
# probe_grid_nanadd.d records ldc2 -O0's choice for every operation (638 rows of raw
# 64-bit patterns), so the pin in substrate/include/eil/math.hpp is now guarded at every
# -O level instead of being re-derived by whoever hits the flip next.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_geom_grid_nanadd.cpp)
  add_executable(test_geom_grid_nanadd tests/test_geom_grid_nanadd.cpp)
  target_link_libraries(test_geom_grid_nanadd PRIVATE eil_geom eil_substrate)
  add_test(NAME geom_grid_nanadd
           COMMAND test_geom_grid_nanadd
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_grid_nanadd.txt)
endif()
