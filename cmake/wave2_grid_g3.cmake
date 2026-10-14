# cmake/wave2_grid_g3.cmake — T-011 stage G3 (see reports/wave-02-grid-brief.md).
#
# Owns only its own target_sources(eil_geom ...) and test blocks; tests self-register
# behind if(EXISTS) so the tree keeps configuring while this stage's files land.
#
# Scope: geom/grid/paver2d.d (enum NodeType + fill_interior) ->
# src/geom/grid/paver2d.{hpp,cpp}, probes/geom/grid/probe_paver2d.d,
# baselines/e587b235/probes/geom_grid_paver2d.txt,
# tests/test_geom_grid_paver2d.cpp.
#
# The two demos in this slice (geom/grid/paver2d_demo.d, geom/grid/paver_demo.d) are
# NOT registered here, because neither can run before geom/grid/usgrid.d (stage G5,
# UnstructuredGrid::this at usgrid.d:458 and :508) exists: measured by compiling and
# running the D sources themselves, whose whole stdout is
#   paver2d_demo  ->  98 bytes  (the two fill_interior writeln lines)
#   paver_demo    ->  5097 bytes, plus a 141490 byte paved_grid.vtk
# Both baselines are captured and versioned:
#   baselines/e587b235/probes/geom_grid_paver2d_demo.txt        (stdout, 98 bytes)
#   baselines/e587b235/probes/geom_grid_paver2d_demo.paved_grid2.vtk (20112 bytes)
#   baselines/e587b235/probes/geom_grid_paver_demo.txt          (stdout, 5097 bytes)
#   baselines/e587b235/probes/geom_grid_paver_demo.paved_grid.vtk (141490 bytes)
# and are reproducible (two runs of each D binary compared with cmp, byte-identical).
# The C++ demos land with G5; nothing is registered for a source that does not exist.

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/geom/grid/paver2d.cpp)
  target_sources(eil_geom PRIVATE src/geom/grid/paver2d.cpp)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_geom_grid_paver2d.cpp)
  add_executable(test_geom_grid_paver2d tests/test_geom_grid_paver2d.cpp)
  target_link_libraries(test_geom_grid_paver2d PRIVATE eil_geom eil_nm eil_substrate)
  # The test replays the ldc2-captured probe corpus, so the baseline path is passed as
  # argv[1] (wave-1 lesson 3: a test that reads a data file must be given the path).
  add_test(NAME geom_grid_paver2d
           COMMAND test_geom_grid_paver2d
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_grid_paver2d.txt)
endif()
