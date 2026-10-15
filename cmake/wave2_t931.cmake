# cmake/wave2_t931.cmake — T-931: two port rules corrected by measurement, and the stage
# replay that found them (reports/backlog.md T-011 / T-931).
#
#   substrate_pow        extended by 4,444 sweep rows: D's `x ^^ 2' is one multiply on
#                        2,222/2,222 operands, while glibc's pow(x,2.0) (what a port
#                        eil::pow(x,2) call becomes) misses 3 of them. 89 `^^2' sites in
#                        the port were routed through pow on the strength of a 7-operand
#                        sample where the two happened to agree.
#   substrate_acos_one   new: D's asin/acos are atan2 formed in 80-bit real, and glibc's
#                        std::acos/std::asin miss 120 of 8,492 arguments — 118 of them
#                        within 2^-30 of +-1, the band geom/grid/paver.d:579 puts an
#                        argument in twice.
#   geom_grid_paver_stage the paving of paver_demo.d, fingerprinted after every step that
#                        can move a node. The 1,961-row geom_grid_paver corpus is byte
#                        identical and still missed this class, because its x/y digests
#                        cannot see an interior_angle that moved by 1 ULP.
#
# Tests self-register behind if(EXISTS) so the tree keeps configuring while files land.

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_substrate_acos_one.cpp)
  add_executable(test_substrate_acos_one tests/test_substrate_acos_one.cpp)
  target_link_libraries(test_substrate_acos_one PRIVATE eil_substrate)
  add_test(NAME substrate_acos_one
           COMMAND test_substrate_acos_one
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/substrate_acos_one.txt)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_substrate_readln.cpp)
  add_executable(test_substrate_readln tests/test_substrate_readln.cpp)
  target_link_libraries(test_substrate_readln PRIVATE eil_substrate)
  add_test(NAME substrate_readln
           COMMAND test_substrate_readln
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/substrate_readln.txt)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_geom_grid_paver_stage.cpp)
  add_executable(test_geom_grid_paver_stage tests/test_geom_grid_paver_stage.cpp)
  target_link_libraries(test_geom_grid_paver_stage PRIVATE eil_geom eil_nm eil_substrate)
  add_test(NAME geom_grid_paver_stage
           COMMAND test_geom_grid_paver_stage
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_grid_paver_stage.txt)
  # ~14 s of paving on both sides; the suite's other grid tests are in the same band.
  set_tests_properties(geom_grid_paver_stage PROPERTIES TIMEOUT 600)
endif()
