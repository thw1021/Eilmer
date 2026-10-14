# cmake/wave2_t926.cmake — T-926 item 1: eil::to_string dispatches a no-arg toString().
#
# Scope: substrate/include/eil/format.hpp gains has_noarg_to_string (a void_t detector for a
# zero-argument const toString()) and to_string now renders such a type via t.toString()
# instead of static_assert-ing on every non-arithmetic type. A type with BOTH a no-arg and a
# fmt toString() uses the no-arg one (matching D); a container still dumps "[a, b]" and
# dispatches each element. This is what lets port call sites write eil::to_string(obj) for the
# many classes that already hand-write a toString() (Vector3, USGCell, every geom patch, nm's
# solver classes).
#
# The reflection-only rows of the corpus (a type WITHOUT toString -> D's field dump or class
# name, enum member name, AA, null) cannot be produced in C++ and are skipped by name in the
# gate — this item's contract is dispatch, not auto-dump.
#
# Item 2 (Slice dynamic-array default arg) and item 3 (readln) of T-926 are NOT this file;
# item 3 is the same EOF-returns-empty semantics as T-923 and is deferred with Wave 4/5.
#
# Tests self-register behind if(EXISTS) so the tree keeps configuring while files land.

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_substrate_tostring_dispatch.cpp)
  add_executable(test_substrate_tostring_dispatch tests/test_substrate_tostring_dispatch.cpp)
  target_link_libraries(test_substrate_tostring_dispatch PRIVATE eil_substrate)
  add_test(NAME substrate_tostring_dispatch
           COMMAND test_substrate_tostring_dispatch
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/substrate_tostring_dispatch.txt)
endif()
