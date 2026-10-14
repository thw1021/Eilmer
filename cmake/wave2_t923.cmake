# cmake/wave2_t923.cmake — T-923: the stmatrix reader's failure paths, now actually gated.
#
# These seven paths had no corpus row before, which is how an earlier transcription came to
# invent "Failed to open \"...\"" and "Unexpected EOF" for them. D raises ErrnoException text
# built from strerror for the open failure, and core.exception.ArrayIndexError for every
# truncated or empty input (it never checks the stream). See
# probes/nm/probe_stmatrix_fails.d for the measurements.

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_nm_stmatrix_fails.cpp)
  add_executable(test_nm_stmatrix_fails tests/test_nm_stmatrix_fails.cpp)
  target_link_libraries(test_nm_stmatrix_fails PRIVATE eil_nm eil_substrate)
  add_test(NAME nm_stmatrix_fails
           COMMAND test_nm_stmatrix_fails
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/nm_stmatrix_fails.txt)
endif()
