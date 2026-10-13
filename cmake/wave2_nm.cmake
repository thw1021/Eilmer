# cmake/wave2_nm.cmake — Wave 2 prerequisites from the nm leaf layer + gzip.
#
# These three nm modules were listed as "migrate with the wave that consumes
# them" (backlog T-905), but geom/gpath/{xspline,xsplinelsq,gpath_utils}.d,
# geom/surface/beziertrianglepatch.d and geom/grid/sgrid.d import them, so Wave 2
# has to carry them. extern/gzip/gzip.d is here for the same reason: sgrid.d and
# usgrid.d define read_from_gzip_file()/write_to_gzip_file().
#
# Layout convention: append `target_sources(eil_nm PRIVATE src/nm/x.cpp)` blocks
# and one test-registration block per D module.

# nm/spline.d, nm/splinelsq.d and nm/nelmin.d all have non-header-only content:
# spline.cpp and splinelsq.cpp hold the class definitions, and nelmin.cpp is the
# instantiation TU for the template header (same arrangement as nm/bbla.cpp).
target_sources(eil_nm PRIVATE
    src/nm/spline.cpp
    src/nm/splinelsq.cpp
    src/nm/nelmin.cpp
)

# extern/gzip/gzip.d, mirroring the gdtk path. zlib.h is at /usr/include/zlib.h
# (zlib 1.2.11, the same series Phobos links), so the library links against `z`
# directly; the include directory is the src root so consumers write
# #include "extern/gzip/gzip.hpp".
add_library(eil_gzip STATIC src/extern/gzip/gzip.cpp)
target_include_directories(eil_gzip PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(eil_gzip PUBLIC eil_substrate z)

# --- tests -----------------------------------------------------------------------------------
# Each D `unittest` block is ported into the matching test as executable assertions,
# and each numeric cluster is replayed line for line against the ldc2 capture under
# baselines/e587b235/probes/. Paths to every data file are passed as argv: wave 1
# had false failures from tests registered without them.

add_executable(test_nm_spline tests/test_nm_spline.cpp)
target_link_libraries(test_nm_spline PRIVATE eil_nm eil_substrate)
add_test(NAME nm_spline COMMAND test_nm_spline
         ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/nm_spline.txt)

add_executable(test_nm_splinelsq tests/test_nm_splinelsq.cpp)
target_link_libraries(test_nm_splinelsq PRIVATE eil_nm eil_substrate)
add_test(NAME nm_splinelsq COMMAND test_nm_splinelsq
         ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/nm_splinelsq.txt)

add_executable(test_nm_nelmin tests/test_nm_nelmin.cpp)
target_link_libraries(test_nm_nelmin PRIVATE eil_nm eil_substrate)
add_test(NAME nm_nelmin COMMAND test_nm_nelmin
         ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/nm_nelmin.txt
         ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/nm_nelmin_unittest.txt)

# The tie table behind the sort nelmin uses: 6889 permutations captured from D.
add_executable(test_nm_nelmin_sort tests/test_nm_nelmin_sort.cpp)
target_link_libraries(test_nm_nelmin_sort PRIVATE eil_nm eil_substrate)
add_test(NAME nm_nelmin_sort COMMAND test_nm_nelmin_sort
         ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/nm_nelmin_sort.txt)

# nm/nelmin_demo.d: stdout must be byte-identical to the D binary's.
add_executable(demo_nm_nelmin tests/demo_nm_nelmin.cpp)
target_link_libraries(demo_nm_nelmin PRIVATE eil_nm eil_substrate)
find_program(BASH_PROGRAM bash)
add_test(NAME nm_nelmin_demo
         COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/harness/compare_text.sh
                 ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/nm_nelmin_demo.txt
                 $<TARGET_FILE:demo_nm_nelmin>)

# extern/gzip/gzip.d has no unittest block; this test pins its observable
# behaviour instead (see the header of the test). It needs the payload text, the
# two D-written containers and a writable scratch directory, all passed as argv.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/gzip_scratch)
add_executable(test_extern_gzip_gzip tests/test_extern_gzip_gzip.cpp)
target_link_libraries(test_extern_gzip_gzip PRIVATE eil_gzip eil_substrate)
add_test(NAME extern_gzip_gzip
         COMMAND test_extern_gzip_gzip
                 ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/extern_gzip_gzip.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_data/gzip_payload.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_data/gzip_payload.dgz.gz
                 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_data/gzip_payload.zraw
                 ${CMAKE_CURRENT_BINARY_DIR}/gzip_scratch)
set_tests_properties(extern_gzip_gzip PROPERTIES
                     WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/gzip_scratch)
