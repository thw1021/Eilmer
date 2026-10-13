# cmake/wave2_geom_misc.cmake — geom/misc leaves (univariatefunctions, kdtree, svg,
# nurbs_utils). Owns only target_sources(eil_geom ...) and its own test blocks.
#
# The eil_geom target itself is declared in cmake/wave2_geom.cmake (another owner), as
# are the nm/gpath/gasdyn fragments; nothing here touches them.
#
# Test naming follows the wave-02 brief: one test per D module, geom_misc_<module>,
# which (a) replays the D `unittest` blocks assertion for assertion and (b) replays the
# ldc2-captured probe output of probes/geom/misc/probe_<module>.d against
# baselines/e587b235/probes/geom_misc_<module>.txt line by line, reporting how many
# printed quantities are bit-identical and the worst residual in ULPs.
#
# Baselines are passed as argv (wave-1 lesson 3): a test that reads a data file must be
# given its path, or it fails for the wrong reason.

target_sources(eil_geom PRIVATE
    src/geom/misc/univariatefunctions.cpp
    src/geom/misc/kdtree.cpp
    src/geom/misc/svg.cpp
    src/geom/misc/nurbs_utils.cpp
)

# The modules whose only input is their own baseline file.
foreach(_m IN ITEMS svg nurbs_utils)
  add_executable(test_geom_misc_${_m} tests/test_geom_misc_${_m}.cpp)
  target_link_libraries(test_geom_misc_${_m} PRIVATE eil_geom eil_nm eil_substrate)
  add_test(NAME geom_misc_${_m}
           COMMAND test_geom_misc_${_m}
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_misc_${_m}.txt)
endforeach()

# univariatefunctions (833 D lines, 9 UnivariateFunction subclasses counted by the
# wave-02 brief: the 8 classes defined here plus geom/luawrap/luaunifunction.d:207
# LuaFnUnivariateFunction, which is Wave 7). Its test replays the 857-line probe
# corpus and D's unittest block. See the two header comments in
# tests/test_geom_misc_univariatefunctions.cpp for why that corpus comes from
# probes/geom/misc/univariatefunctions_copy.d rather than the module itself.
add_executable(test_geom_misc_univariatefunctions
               tests/test_geom_misc_univariatefunctions.cpp)
target_link_libraries(test_geom_misc_univariatefunctions PRIVATE eil_geom eil_nm eil_substrate)
add_test(NAME geom_misc_univariatefunctions
         COMMAND test_geom_misc_univariatefunctions
                 ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_misc_univariatefunctions.txt)

# kdtree takes a second file: the 100 query points Phobos' MT19937 draws from seed
# 19920829 (kdtree.d:176-182). Phobos' RNG is not ported, so the points are captured
# from D into tests/test_data/kdtree_queries.txt and replayed; see the header of
# probes/geom/misc/probe_kdtree.d for the command that regenerates both files.
add_executable(test_geom_misc_kdtree tests/test_geom_misc_kdtree.cpp)
target_link_libraries(test_geom_misc_kdtree PRIVATE eil_geom eil_nm eil_substrate)
add_test(NAME geom_misc_kdtree
         COMMAND test_geom_misc_kdtree
                 ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_misc_kdtree.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_data/kdtree_queries.txt)

# --- the two D demos of this slice -------------------------------------------------
# stdout byte-compared against the D binary. univariatefunctions_demo.d cannot be
# built from gdtk (nm.brent is @nogc and throws, which blocks any module calling it),
# so its baseline comes from probes/geom/misc/univariatefunctions_demo_copy.d, whose
# only difference from the D original is the import line — see gen_d_copies.py.
# svg_demo.d builds directly from gdtk.
# Both demos also *write* SVG files (svg.d draws into a File), so they run in the build
# directory and their stdout is what is compared; the document text itself is gated by
# the geom_misc_svg probe replay.
add_executable(demo_geom_misc_svg tests/demo_geom_misc_svg.cpp)
target_link_libraries(demo_geom_misc_svg PRIVATE eil_geom eil_nm eil_substrate)

add_executable(demo_geom_misc_univariatefunctions tests/demo_geom_misc_univariatefunctions.cpp)
target_link_libraries(demo_geom_misc_univariatefunctions PRIVATE eil_geom eil_nm eil_substrate)

find_program(BASH_PROGRAM bash)
if(BASH_PROGRAM)
  foreach(_d IN ITEMS svg univariatefunctions)
    if(NOT TARGET demo_geom_misc_${_d})
      continue()
    endif()
    add_test(NAME demo_equiv_geom_misc_${_d}
             COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/harness/compare_text.sh
                     ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_misc_${_d}_demo.txt
                     $<TARGET_FILE:demo_geom_misc_${_d}>)
  endforeach()
endif()
