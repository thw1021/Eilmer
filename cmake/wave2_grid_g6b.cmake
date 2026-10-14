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
