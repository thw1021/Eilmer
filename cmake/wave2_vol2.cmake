# cmake/wave2_vol2.cmake — one T-012 stage's targets.
#
# Owns only its own target_sources(eil_geom ...) and test blocks. Each stage gets a
# separate fragment so stages ported in parallel edit disjoint files (the wave-01
# lesson about racing on one CMakeLists). Tests self-register behind if(EXISTS) so the
# tree keeps configuring while this stage's files are still landing.
#
# Owner: the geom/volume stage covering sweptsurfacevolume.d, wedgevolume.d,
# tfivolume.d and volume_demo.d. nurbsvolume.d is registered in
# cmake/wave2_nurbsvol.cmake (another agent), not here.

# --- geom/volume/sweptsurfacevolume.d (T-012) -------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/geom/volume/sweptsurfacevolume.cpp)
  target_sources(eil_geom PRIVATE src/geom/volume/sweptsurfacevolume.cpp)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_geom_volume_sweptsurfacevolume.cpp)
  add_executable(test_geom_volume_sweptsurfacevolume
                 tests/test_geom_volume_sweptsurfacevolume.cpp)
  target_link_libraries(test_geom_volume_sweptsurfacevolume
                        PRIVATE eil_geom eil_nm eil_substrate)
  add_test(NAME geom_volume_sweptsurfacevolume
           COMMAND test_geom_volume_sweptsurfacevolume
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_volume_sweptsurfacevolume.txt)
endif()

# --- geom/volume/wedgevolume.d (T-012) --------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/geom/volume/wedgevolume.cpp)
  target_sources(eil_geom PRIVATE src/geom/volume/wedgevolume.cpp)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_geom_volume_wedgevolume.cpp)
  add_executable(test_geom_volume_wedgevolume
                 tests/test_geom_volume_wedgevolume.cpp)
  target_link_libraries(test_geom_volume_wedgevolume
                        PRIVATE eil_geom eil_nm eil_substrate)
  add_test(NAME geom_volume_wedgevolume
           COMMAND test_geom_volume_wedgevolume
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_volume_wedgevolume.txt)
endif()

