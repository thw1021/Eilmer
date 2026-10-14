# cmake/wave2_grid_g4.cmake — T-011 stage G4 (see reports/wave-02-grid-brief.md).
#
# Owns only its own target_sources(eil_geom ...) and test blocks; tests self-register
# behind if(EXISTS) so the tree keeps configuring while this stage's files land.
#
# Scope: geom/grid/sgrid.d (StructuredGrid, 1865 D lines) ->
# src/geom/grid/sgrid.{hpp,cpp}, probes/geom/grid/probe_sgrid.d,
# baselines/e587b235/probes/geom_grid_sgrid.txt (2097 rows),
# tests/test_geom_grid_sgrid.cpp.
#
# G4 and G5 are ONE work package by measurement, not by taste (grid brief section
# 10.1): usgrid.d:553 reads StructuredGrid's members and sgrid.d:1316 constructs an
# UnstructuredGrid, so sgrid.cpp includes usgrid.hpp and usgrid.cpp includes
# sgrid.hpp, with the two headers forward-declaring each other. Both .cpp files go
# into eil_geom (G5's fragment registers usgrid.cpp), and the SU2 rows of the sgrid
# corpus (sg.su2.*) can only be replayed once usgrid.cpp is in the library too.

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/geom/grid/sgrid.cpp)
  target_sources(eil_geom PRIVATE src/geom/grid/sgrid.cpp)
endif()

# Registration is all-or-nothing per the grid brief section 10.5: the corpus gate is
# row-for-row byte equality, so the add_test() lines below stay switched off until
# tests/test_geom_grid_sgrid.cpp replays all 2097 rows at 0 differ. A source file in
# src/ compiles without producing a red test; a registered red test is not allowed.
#
# Gate for the corpus replay (put in place when the row count reaches 0 differ):
#
# Registered 2026-10-14 at 2097 emitted / 2097 byte-identical / 0 differ, re-verified in a
# second build directory by the main thread before the add_test below was switched on.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_geom_grid_sgrid.cpp)
  add_executable(test_geom_grid_sgrid tests/test_geom_grid_sgrid.cpp)
  target_link_libraries(test_geom_grid_sgrid PRIVATE eil_geom eil_nm eil_substrate)
  # The corpus path is argv[1] (wave-01 lesson 3: a test that reads a data file
  # must be given the path as an argument).
  add_test(NAME geom_grid_sgrid
           COMMAND test_geom_grid_sgrid
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_grid_sgrid.txt)
endif()
