# cmake/wave2_geom_core.cmake — the geom/elements foundation (vector3).
#
# Kept separate from cmake/wave2_geom.cmake because the elements stage and the
# foundation equivalence checks are worked on by different owners; both fragments
# are included from the top-level CMakeLists.txt.

add_executable(test_geom_elements_vector3_equiv tests/test_geom_elements_vector3_equiv.cpp)
target_link_libraries(test_geom_elements_vector3_equiv PRIVATE eil_geom eil_nm eil_substrate)
add_test(NAME geom_elements_vector3_equiv
         COMMAND test_geom_elements_vector3_equiv
                 ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_elements_vector3.txt)
