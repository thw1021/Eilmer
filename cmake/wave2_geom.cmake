# cmake/wave2_geom.cmake — the geometry library (gdtk src/geom, minus luawrap).
#
# Sources accumulate here by directory: elements -> gpath/misc -> surface/volume
# -> grid. One test block per D module: the D `unittest` becomes the test, and a
# D *_demo.d becomes demo_geom_<...> whose stdout is diffed against the
# ldc2-captured text under baselines/<rev>/probes/.

add_library(eil_geom STATIC
    src/geom/elements/vector3.cpp
)
target_include_directories(eil_geom PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(eil_geom PUBLIC eil_substrate eil_nm)
if(TARGET eil_gzip)
  target_link_libraries(eil_geom PUBLIC eil_gzip)
endif()

add_executable(test_geom_elements_vector3 tests/test_geom_elements_vector3.cpp)
target_link_libraries(test_geom_elements_vector3 PRIVATE eil_geom eil_nm eil_substrate)
add_test(NAME geom_elements_vector3 COMMAND test_geom_elements_vector3)
