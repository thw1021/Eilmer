# cmake/wave2_t928.cmake — T-928: string->integer range check + std.conv.ConvOverflowException.
#
# Scope: substrate/include/eil/error.hpp gains ConvOverflowException (the overflow child of
# ConversionException), and parse_int_strict in substrate/include/eil/conv.hpp becomes a
# per-target-type template that raises it with "Overflow in integral conversion" as soon as
# the accumulated magnitude leaves T's range, accumulating in make_unsigned_t<T> so ulong's
# own maximum converts instead of overflowing a signed accumulator. That let the port-local
# usgrid.hpp ConvOverflowException + usgrid.cpp d_to_int, and the third scanner
# util/time_utils.cpp to_int_d, be deleted in favour of the substrate scan.
#
# The corpus already existed (probes/substrate/probe_conv_int_typename.d, captured for
# T-927) but had NO gate, which is why the missing range check survived: the overflow rows
# (int.max+1, uint.max+1, ulong.max+1, size_t.huge) and the two asymmetric signed endpoints
# were measured and then never wired. This registers the replay.
#
# Tests self-register behind if(EXISTS) so the tree keeps configuring while files land.

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_substrate_conv_int_typename.cpp)
  add_executable(test_substrate_conv_int_typename tests/test_substrate_conv_int_typename.cpp)
  target_link_libraries(test_substrate_conv_int_typename PRIVATE eil_substrate)
  add_test(NAME substrate_conv_int_typename
           COMMAND test_substrate_conv_int_typename
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/substrate_conv_int_typename.txt)
endif()
