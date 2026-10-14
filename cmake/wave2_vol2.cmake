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

# --- geom/volume/tfivolume.d (T-012) ----------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/geom/volume/tfivolume.cpp)
  target_sources(eil_geom PRIVATE src/geom/volume/tfivolume.cpp)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_geom_volume_tfivolume.cpp)
  add_executable(test_geom_volume_tfivolume
                 tests/test_geom_volume_tfivolume.cpp)
  target_link_libraries(test_geom_volume_tfivolume
                        PRIVATE eil_geom eil_nm eil_substrate)
  add_test(NAME geom_volume_tfivolume
           COMMAND test_geom_volume_tfivolume
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_volume_tfivolume.txt)
endif()


# --- geom/surface/surface_demo.d and geom/volume/volume_demo.d (T-012) -------------
# The two directory-level demos of the surface/volume hierarchy. stdout is byte-compared
# against the D binary's captured text through harness/compare_text.sh. surface_demo.d
# writes one file (D:52, "aopatch.vtk"), so the demo is handed a build-tree directory as
# argv[1] and cannot leave an artefact in the source tree; volume_demo.d writes nothing.
find_program(BASH_PROGRAM bash)

if(BASH_PROGRAM AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/demo_geom_surface_surface.cpp)
  add_executable(demo_geom_surface_surface tests/demo_geom_surface_surface.cpp)
  target_link_libraries(demo_geom_surface_surface PRIVATE eil_geom eil_nm eil_substrate)
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/demo_work_vol2)
  add_test(NAME demo_equiv_geom_surface_surface
           COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/harness/compare_text.sh
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_surface_surface_demo.txt
                   $<TARGET_FILE:demo_geom_surface_surface>
                   ${CMAKE_CURRENT_BINARY_DIR}/demo_work_vol2)
endif()

if(BASH_PROGRAM AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/demo_geom_volume_volume.cpp)
  add_executable(demo_geom_volume_volume tests/demo_geom_volume_volume.cpp)
  target_link_libraries(demo_geom_volume_volume PRIVATE eil_geom eil_nm eil_substrate)
  add_test(NAME demo_equiv_geom_volume_volume
           COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/harness/compare_text.sh
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_volume_volume_demo.txt
                   $<TARGET_FILE:demo_geom_volume_volume>)
endif()
