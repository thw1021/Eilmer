# cmake/wave2_gasdyn.cmake — Wave 2 slice of gasdyn (the CEA-free part).
#
# gasdyn/gasflow.d is NOT here: its shock relations take `ref GasState`/`GasModel`
# from the gas/ package (23,281 D lines, Wave 3) and gasflow_demo.d builds a model
# with init_gas_model() on a .lua file. See reports/backlog.md.
#
# What this fragment carries is the whole CEA-free remainder of the module set:
#   gasdyn/gasflowexception.d  -> src/gasdyn/gasflowexception.hpp (header only: it
#                                 declares one exception class and no non-inline
#                                 code, so there is no .cpp pair)
#   gasdyn/idealgasflow.d      -> src/gasdyn/idealgasflow.hpp/.cpp
#   gasdyn/idealgasflow_demo.d -> tests/demo_gasdyn_idealgasflow.cpp
# idealgasflow.d is self-contained: its imports are std.conv/math/string/stdio,
# nm.bbla, nm.bracketing, nm.ridder, nm.linesearch and gasdyn.gasflowexception, all
# of which are already ported (nm.linesearch is imported by D but never called).

add_library(eil_gasdyn STATIC src/gasdyn/idealgasflow.cpp)
target_include_directories(eil_gasdyn PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(eil_gasdyn PUBLIC eil_substrate eil_nm)

# The D `unittest` blocks, plus a line-for-line replay of
# probes/gasdyn/probe_idealgasflow.d against the ldc2 capture. The corpus path is
# passed as argv[1] (wave-1 lesson 3: a test that reads a data file must be told
# where it is), so the test fails loudly rather than silently skipping the diff.
add_executable(test_gasdyn_idealgasflow tests/test_gasdyn_idealgasflow.cpp)
target_link_libraries(test_gasdyn_idealgasflow PRIVATE eil_gasdyn eil_nm eil_substrate)
add_test(NAME gasdyn_idealgasflow COMMAND test_gasdyn_idealgasflow
         ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/gasdyn_idealgasflow.txt)

# The demo, whose stdout must be byte-identical to the D binary's. Registered here
# rather than by the top-level glob, which only picks up demo_nm_* targets.
add_executable(demo_gasdyn_idealgasflow tests/demo_gasdyn_idealgasflow.cpp)
target_link_libraries(demo_gasdyn_idealgasflow PRIVATE eil_gasdyn eil_nm eil_substrate)

find_program(BASH_PROGRAM bash)
if(BASH_PROGRAM)
  add_test(NAME demo_equiv_gasdyn_idealgasflow
           COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/harness/compare_text.sh
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/gasdyn_idealgasflow_demo.txt
                   $<TARGET_FILE:demo_gasdyn_idealgasflow>)
endif()
