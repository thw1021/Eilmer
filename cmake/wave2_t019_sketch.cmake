# cmake/wave2_t019_sketch.cmake — T-019 (geom/misc/sketch.d + sketch_demo.d).
#
# Owns only its own target_sources(eil_misc ...) and test blocks; tests self-register
# behind if(EXISTS) so the tree keeps configuring while this stage's files land.
# Scope: geom/misc/sketch.d (964 D lines) -> src/geom/misc/sketch.{hpp,cpp},
# probes/geom/misc/probe_sketch.d, baselines/e587b235/probes/geom_misc_sketch.txt,
# tests/test_geom_misc_sketch.cpp, and sketch_demo.d -> tests/demo_geom_misc_sketch.cpp.
# rotating_a.d / drifting_eye.d / libplot.d are NOT here: no D binary can be linked for
# them on this machine (no GNU libplot), so there is no baseline to gate against — see
# reports/backlog.md T-921.
#
# Registration rule for this stage: the add_test() lines go in ONLY when the whole
# corpus replays at 0 differ. The gate is byte-equality over every row, so a partially
# generated corpus is not a passing test, and registering one would take the suite off
# green for every other stage sharing the tree.

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/geom/misc/sketch.cpp)
  target_sources(eil_geom PRIVATE src/geom/misc/sketch.cpp)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_geom_misc_sketch.cpp)
  add_executable(test_geom_misc_sketch tests/test_geom_misc_sketch.cpp)
  target_link_libraries(test_geom_misc_sketch PRIVATE eil_geom eil_nm eil_substrate)
  # The corpus path is passed as argv (wave-1 lesson 3), and the test runs in a private
  # build-tree directory because it writes six skprobe_*.svg documents the way the D
  # probe does — never into the source tree.
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/wave2_t019_sketch_work)
  add_test(NAME geom_misc_sketch
           COMMAND test_geom_misc_sketch
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_misc_sketch.txt)
  set_tests_properties(geom_misc_sketch PROPERTIES
                       WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/wave2_t019_sketch_work)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/demo_geom_misc_sketch.cpp)
  add_executable(demo_geom_misc_sketch tests/demo_geom_misc_sketch.cpp)
  target_link_libraries(demo_geom_misc_sketch PRIVATE eil_geom eil_nm eil_substrate)

  # stdout (2 lines, 62 bytes) byte-compared against the D binary's captured text. argv[1]
  # is D's renderer argument; argv[2] is the private directory the three documents go to.
  find_program(BASH_PROGRAM bash)
  if(BASH_PROGRAM)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/wave2_t019_sketch_demo)
    add_test(NAME demo_equiv_geom_misc_sketch
             COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/harness/compare_text.sh
                     ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_misc_sketch_demo.txt
                     $<TARGET_FILE:demo_geom_misc_sketch>
                     svg ${CMAKE_CURRENT_BINARY_DIR}/wave2_t019_sketch_demo)
    set_tests_properties(demo_equiv_geom_misc_sketch PROPERTIES
                         FIXTURES_SETUP t019_sketch_demo)

    # The three .svg products are gated as files too (9739 / 1955 / 8856 bytes, md5
    # 510aea9d14a0bdefcadbf5e6b10b30a4 / 60bc566d544722cce7f42680937263bd /
    # ae87fcbcd1e47c2eb8ec9f31670751e1 — the same three the reference D binary writes).
    foreach(_a IN ITEMS test test2 test3)
      add_test(NAME demo_artifact_geom_misc_sketch_${_a}
               COMMAND ${CMAKE_COMMAND} -E compare_files
                       ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_misc_sketch_demo.${_a}.svg
                       ${CMAKE_CURRENT_BINARY_DIR}/wave2_t019_sketch_demo/${_a}.svg)
      set_tests_properties(demo_artifact_geom_misc_sketch_${_a} PROPERTIES
                           FIXTURES_REQUIRED t019_sketch_demo)
    endforeach()
  endif()
endif()
