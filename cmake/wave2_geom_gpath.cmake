# cmake/wave2_geom_gpath.cmake — geom/gpath (one-parameter geometry elements).
#
# Stage 1 of the gpath verification: the five modules geom/gpath/path.d, line.d,
# arc.d, bezier.d and polynomial.d (823 D lines), their ported `unittest` blocks and
# their ldc2 probe replays, plus the package demo geom/gpath/gpath_demo.d.
#
# Owns only target_sources(eil_geom ...) and its own test/demo blocks; eil_geom
# itself is declared in cmake/wave2_geom.cmake (another owner) so stages can add
# sources without editing each other's files. The helix/polyline/modifiedpath/
# xspline/xsplinelsq modules of this directory belong to cmake/wave2_geom_gpath3.cmake
# and are not listed here.
#
# Test naming follows the wave-02 brief: one test per D module, geom_gpath_<module>,
# which (a) replays the D `unittest` block assertion for assertion (path.d and
# polynomial.d have none in D; both say so in their header comment) and (b) replays
# the ldc2-captured probe output of probes/geom/gpath/probe_<module>.d against
# baselines/e587b235/probes/geom_gpath_<module>.txt line by line, reporting how many
# printed quantities are bit-identical and the worst residual in ULPs.
#
# Baselines are passed as argv (wave-01 lesson 3): a test that reads a data file must
# be given its path, or it fails for the wrong reason.

target_sources(eil_geom PRIVATE
    src/geom/gpath/path.cpp
    src/geom/gpath/line.cpp
    src/geom/gpath/arc.cpp
    src/geom/gpath/bezier.cpp
    src/geom/gpath/polynomial.cpp
)

foreach(_m IN ITEMS path line arc bezier polynomial)
  add_executable(test_geom_gpath_${_m} tests/test_geom_gpath_${_m}.cpp)
  target_link_libraries(test_geom_gpath_${_m} PRIVATE eil_geom eil_nm eil_substrate)
  add_test(NAME geom_gpath_${_m}
           COMMAND test_geom_gpath_${_m}
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_gpath_${_m}.txt)
endforeach()

# The atan2/atan corpus that gates eil::geom::phobos_atan2 (see arc.hpp), plus the
# step-by-step dump of Arc.evaluate_position_and_length that localised it.
add_executable(test_geom_gpath_arcsteps tests/test_geom_gpath_arcsteps.cpp)
target_link_libraries(test_geom_gpath_arcsteps PRIVATE eil_geom eil_nm eil_substrate)
add_test(NAME geom_gpath_arcsteps
         COMMAND test_geom_gpath_arcsteps
                 ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_gpath_arcsteps.txt)

# --- geom/gpath/gpath_demo.d ------------------------------------------------------
# stdout byte-compared against the D binary's captured text
# (baselines/e587b235/probes/geom_gpath_gpath_demo.txt) through
# harness/compare_text.sh.
#
# The name carries a _gpathd suffix rather than the bare module name because the
# wave-02 brief also lists "the two gpath demos" under
# cmake/wave2_geom_gpath2.cmake; two add_executable() calls for the same target
# would break configuration for every stage, and this way the demo that exercises
# the five stage-1 modules is guaranteed to exist.
#
# The demo also constructs Helix, Polyline, ArcLengthParameterizedPath,
# SubRangedPath and ReversedPath, which are stage-2/3 modules of this directory.
# Their translation units are attached to this executable only — never to
# eil_geom, which their own fragment owns — so the static library cannot hand the
# linker a second definition of a symbol this binary already provides.
add_executable(demo_geom_gpath_gpathd tests/demo_geom_gpath_gpathd.cpp)
target_sources(demo_geom_gpath_gpathd PRIVATE
    src/geom/gpath/helix.cpp
    src/geom/gpath/polyline.cpp
    src/geom/gpath/modifiedpath.cpp
)
target_link_libraries(demo_geom_gpath_gpathd PRIVATE eil_geom eil_nm eil_substrate)

find_program(BASH_PROGRAM bash)
if(BASH_PROGRAM)
  add_test(NAME demo_equiv_geom_gpath_gpathd
           COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/harness/compare_text.sh
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_gpath_gpath_demo.txt
                   $<TARGET_FILE:demo_geom_gpath_gpathd>)
endif()
