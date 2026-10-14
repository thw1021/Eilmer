# cmake/wave2_t929.cmake — T-929: eil::Array fills a grown/new slot with T.init.
#
# Scope: substrate/include/eil/array.hpp gains detail::grow_init (NaN for floating point,
# T() otherwise), set_length fills the tail through it, and both set_length and
# set_length_signed report D's own OutOfMemoryError site (rt/lifetime.d:1454) when the
# allocator refuses a length. That let the port-local funnels sgrid.cpp d_set_length and
# usgrid.cpp set_length_d, and the test-side d_init_doubles helper, be deleted.
#
# The corpus is needed rather than a comment because the previous behaviour was a silent
# zero-fill that no gate exercised on the unwritten-slot path: double.init is a quiet NaN,
# measured over 30 rows in probes/substrate/probe_array_init.d (new/length/grow/regrow for
# double, plus int, bool, a struct with a double field, and a class handle).
#
# Tests self-register behind if(EXISTS) so the tree keeps configuring while files land.

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_substrate_array_init.cpp)
  add_executable(test_substrate_array_init tests/test_substrate_array_init.cpp)
  target_link_libraries(test_substrate_array_init PRIVATE eil_substrate)
  add_test(NAME substrate_array_init
           COMMAND test_substrate_array_init
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/substrate_array_init.txt)
endif()
