# cmake/wave2_geom.cmake — the geometry library (gdtk src/geom, minus luawrap).
#
# Sources accumulate here by directory: elements -> gpath/misc -> surface/volume
# -> grid. One test block per D module: the D `unittest` becomes the test, and a
# D *_demo.d becomes demo_geom_<...> whose stdout is diffed against the
# ldc2-captured text under baselines/<rev>/probes/.

add_library(eil_geom STATIC
    src/geom/geometry_exception.cpp
    src/geom/elements/vector3.cpp
    src/geom/elements/nomenclature.cpp
    src/geom/elements/properties.cpp
    src/geom/elements/projection.cpp
)
target_include_directories(eil_geom PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(eil_geom PUBLIC eil_substrate eil_nm)
if(TARGET eil_gzip)
  target_link_libraries(eil_geom PUBLIC eil_gzip)
endif()

add_executable(test_geom_elements_vector3 tests/test_geom_elements_vector3.cpp)
target_link_libraries(test_geom_elements_vector3 PRIVATE eil_geom eil_nm eil_substrate)
add_test(NAME geom_elements_vector3 COMMAND test_geom_elements_vector3)

# --- geom/elements, stage 2 (the rest of the module directory) ------------------
# Each of these three tests replays its D module's `unittest` block assertion by
# assertion *and* diffs a ldc2-captured probe corpus, so the baseline file is passed
# as argv[1] (wave-1 lesson 3: a test that reads a data file must be given the path).
foreach(_m IN ITEMS nomenclature properties projection)
  add_executable(test_geom_elements_${_m} tests/test_geom_elements_${_m}.cpp)
  target_link_libraries(test_geom_elements_${_m} PRIVATE eil_geom eil_nm eil_substrate)
  add_test(NAME geom_elements_${_m}
           COMMAND test_geom_elements_${_m}
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_elements_${_m}.txt)
endforeach()

# vector3_demo.d needs project_onto_plane from projection.d, so the demo lands with
# this stage. Its stdout is compared byte-for-byte against the D binary's captured
# text, the same way the top-level CMakeLists does for demo_nm_*.
add_executable(demo_geom_elements_vector3 tests/demo_geom_elements_vector3.cpp)
target_link_libraries(demo_geom_elements_vector3 PRIVATE eil_geom eil_nm eil_substrate)
find_program(BASH_PROGRAM bash)
if(BASH_PROGRAM)
  add_test(NAME demo_equiv_geom_elements_vector3
           COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/harness/compare_text.sh
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_elements_vector3_demo.txt
                   $<TARGET_FILE:demo_geom_elements_vector3>)
endif()
