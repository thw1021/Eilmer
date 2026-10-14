# cmake/wave2_t920.cmake — T-920: the cos/sin pair must stay two libm calls.
#
# Scope: substrate/include/eil/math.hpp gains eil::sin / eil::cos, and every ported
# site that takes both of one argument is routed through them
# (geom/elements/vector3.cpp rotate_about_zaxis + rotate2d, geom/elements/projection.cpp:78,
# geom/grid/grid.cpp:533-534, geom/gpath/arc.cpp:119, geom/gpath/helix.cpp:70,
# geom/volume/wedgevolume.cpp, gasdyn/idealgasflow.cpp).
#
# Why a corpus is needed rather than a comment: BOTH compilers fuse the pair from -O1 up
# (g++ into glibc sincos, LDC via LLVM's sincos pass), and the fused form is 1 ULP off the
# two separate calls on about 1 component in 1466. Classified over the 415 angles of
# tests/test_data/sincos_angle.txt: ldc2 -O0 reproduces 401 rows as separate-only and
# ldc2 -O3 reproduces the same 401 as sincos-only — so the reference's own answer moves
# with its -O, and only the -O0 capture is a contract. The 400 `div` angles are exactly
# the ones that can tell the two forms apart.
#
# Tests self-register behind if(EXISTS) so the tree keeps configuring while this
# stage's files land.

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_substrate_sin_cos_pair.cpp)
  add_executable(test_substrate_sin_cos_pair tests/test_substrate_sin_cos_pair.cpp)
  target_link_libraries(test_substrate_sin_cos_pair PRIVATE eil_substrate)
  add_test(NAME substrate_sin_cos_pair
           COMMAND test_substrate_sin_cos_pair
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/substrate_sin_cos_pair.txt
                   ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_data/sincos_angle.txt)
endif()
