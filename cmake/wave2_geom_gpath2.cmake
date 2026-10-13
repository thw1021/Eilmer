# cmake/wave2_geom_gpath2.cmake — the gpath modules the first pass did not reach:
# nurbs.d, svgpath.d, gpath_utils.d and the two gpath demos.
#
# (gpath_demo.d is registered by cmake/wave2_geom_gpath.cmake as
# demo_geom_gpath_gpathd; the only demo in this slice is gpath_utils_demo.d.)
#
# Owns only target_sources(eil_geom ...) and its own test/demo blocks; eil_geom is
# declared in cmake/wave2_geom.cmake, and the five stage-1 modules plus the
# helix/polyline/modifiedpath/xspline/xsplinelsq modules this slice depends on are
# registered by the sibling gpath fragments. Only the three sources named below are
# added here.
#
# Test naming follows the wave-02 brief: one test per D module, geom_gpath_<module>,
# which (a) replays the D `unittest` block assertion for assertion (gpath_utils.d has
# no unittest block; its test says so and leans on the probe corpus plus the demo) and
# (b) replays the ldc2-captured probe output of probes/geom/gpath/probe_<module>.d
# against baselines/e587b235/probes/geom_gpath_<module>.txt line by line, reporting
# how many printed quantities are bit-identical and the worst residual in ULPs.
#
# Baselines and data files are passed as argv (wave-01 lesson 3).

target_sources(eil_geom PRIVATE
    src/geom/gpath/nurbs.cpp
    src/geom/gpath/svgpath.cpp
    src/geom/gpath/gpath_utils.cpp
)

# Self-registering blocks: the stage agent wrote the C++ and captured the corpora but
# ran out of turns before writing the tests, so each target appears once its source
# exists. That keeps the tree configurable for parallel stages meanwhile.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/geom/misc/nurbs_utils.hpp)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_geom_gpath_nurbs.cpp)
add_executable(test_geom_gpath_nurbs tests/test_geom_gpath_nurbs.cpp)
  target_link_libraries(test_geom_gpath_nurbs PRIVATE eil_geom eil_nm eil_substrate)
  add_test(NAME geom_gpath_nurbs
           COMMAND test_geom_gpath_nurbs
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_gpath_nurbs.txt)
endif()
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_geom_gpath_svgpath.cpp)
add_executable(test_geom_gpath_svgpath tests/test_geom_gpath_svgpath.cpp)
target_link_libraries(test_geom_gpath_svgpath PRIVATE eil_geom eil_nm eil_substrate)
add_test(NAME geom_gpath_svgpath
         COMMAND test_geom_gpath_svgpath
                 ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_gpath_svgpath.txt)
endif()

# gpath_utils' probe reads four coordinate files: the cowl sample (which the demo also
# reads) plus the three tests/test_data files its readPointsFromFile rows use. The
# directory is argv[2] so the test can run from any build directory.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_geom_gpath_gpath_utils.cpp)
add_executable(test_geom_gpath_gpath_utils tests/test_geom_gpath_gpath_utils.cpp)
target_link_libraries(test_geom_gpath_gpath_utils PRIVATE eil_geom eil_nm eil_substrate)
add_test(NAME geom_gpath_gpath_utils
         COMMAND test_geom_gpath_gpath_utils
                 ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_gpath_gpath_utils.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_data)
endif()

# --- geom/gpath/gpath_utils_demo.d ------------------------------------------------
# stdout byte-compared against the D binary's captured text through
# harness/compare_text.sh. The D demo opens "sample-data/cowl-spline-pts.dat"
# relative to its cwd (the makefile runs it from src/geom); the port is handed the
# same bytes as argv[1] from tests/test_data so it can run from anywhere — see the
# demo's header comment.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_data/cowl-spline-pts.dat)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/demo_geom_gpath_gpath_utils.cpp)
add_executable(demo_geom_gpath_gpath_utils tests/demo_geom_gpath_gpath_utils.cpp)
  target_link_libraries(demo_geom_gpath_gpath_utils PRIVATE eil_geom eil_nm eil_substrate)
  find_program(BASH_PROGRAM bash)
  if(BASH_PROGRAM)
    add_test(NAME demo_equiv_geom_gpath_gpath_utils
             COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/harness/compare_text.sh
                     ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_gpath_gpath_utils_demo.txt
                     $<TARGET_FILE:demo_geom_gpath_gpath_utils>
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_data/cowl-spline-pts.dat)
  endif()
endif()
endif()