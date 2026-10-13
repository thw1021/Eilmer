# cmake/wave2_t910.cmake — the T-910 root-cause test for the geom_gpath_modifiedpath
# tolerance band, plus the substrate eil::atan2 it forced.
#
# Included last from the top-level CMakeLists.txt. Owns only its own test block.

add_executable(test_geom_gpath_mp_rot_chain tests/test_geom_gpath_mp_rot_chain.cpp)
target_link_libraries(test_geom_gpath_mp_rot_chain PRIVATE eil_geom eil_nm eil_substrate)
add_test(NAME geom_gpath_mp_rot_chain
         COMMAND test_geom_gpath_mp_rot_chain
                 ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_gpath_mp_rot_chain.txt)
