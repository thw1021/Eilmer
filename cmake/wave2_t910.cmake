# cmake/wave2_t910.cmake — the T-910 root-cause test for the geom_gpath_modifiedpath
# tolerance band, plus the substrate eil::atan2 it forced.
#
# Included last from the top-level CMakeLists.txt. Owns only its own test block.

add_executable(test_geom_gpath_mp_rot_chain tests/test_geom_gpath_mp_rot_chain.cpp)
target_link_libraries(test_geom_gpath_mp_rot_chain PRIVATE eil_geom eil_nm eil_substrate)
add_test(NAME geom_gpath_mp_rot_chain
         COMMAND test_geom_gpath_mp_rot_chain
                 ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_gpath_mp_rot_chain.txt)

# The to!double error taxonomy, replayed against Phobos row for row. Substrate-owned:
# these strings reach users through Lua job-script parsing and the normalized-log gate.
add_executable(test_substrate_conv_failtext tests/test_substrate_conv_failtext.cpp)
target_link_libraries(test_substrate_conv_failtext PRIVATE eil_substrate)
add_test(NAME substrate_conv_failtext
         COMMAND test_substrate_conv_failtext
                 ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/substrate_conv_failtext.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_data/conv_inputs.hex)
