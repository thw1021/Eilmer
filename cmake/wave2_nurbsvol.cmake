# cmake/wave2_nurbsvol.cmake — one T-012 stage's targets.
#
# Owns only its own target_sources(eil_geom ...) and test blocks. Each stage gets a
# separate fragment so stages ported in parallel edit disjoint files (the wave-01
# lesson about racing on one CMakeLists). Tests self-register behind if(EXISTS) so the
# tree keeps configuring while this stage's files are still landing.

# --- nurbsvolume.d (544 D lines) --------------------------------------------------
# The trivariate NURBS subclass of ParametricVolume. Self-registering so the tree
# keeps configuring while the files land (wave-02-surfvol-brief section 0).
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/geom/volume/nurbsvolume.cpp)
  target_sources(eil_geom PRIVATE
      src/geom/volume/nurbsvolume.cpp
  )
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_geom_volume_nurbsvolume.cpp)
  add_executable(test_geom_volume_nurbsvolume tests/test_geom_volume_nurbsvolume.cpp)
  target_link_libraries(test_geom_volume_nurbsvolume PRIVATE eil_geom eil_nm eil_substrate)
  add_test(NAME geom_volume_nurbsvolume
           COMMAND test_geom_volume_nurbsvolume
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_volume_nurbsvolume.txt)
endif()
