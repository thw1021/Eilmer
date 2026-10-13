cmake_minimum_required(VERSION 3.20)
project(Eilmer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Floating point: the port is judged against D output at 1e-13, so nothing here
# may reorder or re-associate arithmetic.
# g++ has no -ffp-conform (that is clang). The flag that actually matters here is
# -ffp-contract=off: g++ defaults to 'fast', so at -O2 it rewrites a*b+c into an
# FMA, which changes the last bits of a result relative to the two-instruction
# sequence ldc2 emits for the same D source at -O0. Measured divergence between
# Phobos and glibc transcendentals is already <=1 ULP, so contraction on top of
# that would push the residuals further from the reference.
add_compile_options(-fno-fast-math -fno-unsafe-math-optimizations -ffp-contract=off)
add_compile_options(-Wall -Wextra -Wno-unused-parameter)

# Slices carrying a generation counter fail loudly when their owning Array
# reallocated underneath them. Off by default (it changes what operator[] costs);
# -DEIL_SLICE_CHECK=ON used to be a cache-only no-op, so the gate never ran.
option(EIL_SLICE_CHECK "Check every Slice dereference against its owner's generation" OFF)
if(EIL_SLICE_CHECK)
  add_compile_definitions(EIL_SLICE_CHECK)
endif()

include(cmake/EilmerVariant.cmake)

# Vendored Lua 5.4.3 from the gdtk tree (headers + liblua.a), reused rather than
# re-ported: it is C, and the solver's Lua contract depends on its exact version.
set(EIL_LUA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../gdtk/extern/lua-5.4.3/install"
    CACHE PATH "Prebuilt Lua 5.4 install tree")
add_library(eil_lua IMPORTED STATIC)
set_target_properties(eil_lua PROPERTIES IMPORTED_LOCATION "${EIL_LUA_DIR}/lib/liblua.a")
target_include_directories(eil_lua INTERFACE "${EIL_LUA_DIR}/include")

# ---------------------------------------------------------------------------
# Substrate: the D-compatibility layer. Header only, plus the ABI guard TU.
# ---------------------------------------------------------------------------
add_library(eil_substrate STATIC substrate/src/abi.cpp)
target_include_directories(eil_substrate PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/substrate/include)
target_link_libraries(eil_substrate PUBLIC eil_variants)

# ---------------------------------------------------------------------------
# Wave 1: leaf library modules, mirroring the gdtk src/ directory layout.
# Lists grow one file at a time; every file listed here must compile.
# ---------------------------------------------------------------------------
set(EIL_NM_SOURCES
    src/nm/nm_exception.cpp
    src/nm/limiters.cpp
    src/nm/bracketing.cpp
    src/nm/brent.cpp
    src/nm/ridder.cpp
    src/nm/secant.cpp
    src/nm/linesearch.cpp
    src/nm/newton.cpp
    src/nm/newtoncotes.cpp
    src/nm/gaussquad.cpp
    src/nm/rungekutta.cpp
    src/nm/schedule.cpp
    src/nm/rsla.cpp
    src/nm/stmatrix.cpp
    src/nm/bbla.cpp
    src/nm/smla.cpp
)
add_library(eil_nm STATIC ${EIL_NM_SOURCES})
target_include_directories(eil_nm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(eil_nm PUBLIC eil_substrate)

set(EIL_UTIL_SOURCES
    src/util/msg_service.cpp
    src/util/lua.cpp
    src/util/lua_service.cpp
    src/util/time_utils.cpp
)
# src/util/json_helper.cpp is deliberately absent: it wraps std.json, and no JSON
# library is installed here. Its float round-trip and error text need their own
# corpus probe first (see reports/wave-01.md, pending items).
add_library(eil_util STATIC ${EIL_UTIL_SOURCES})
target_include_directories(eil_util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(eil_util PUBLIC eil_substrate eil_lua)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_executable(test_format_corpus tests/test_format_corpus.cpp)
target_link_libraries(test_format_corpus PRIVATE eil_substrate)
add_test(NAME format_corpus
         COMMAND test_format_corpus ${CMAKE_CURRENT_SOURCE_DIR}/tools/fmt_corpus.txt)

add_executable(test_abi tests/test_abi.cpp)
target_link_libraries(test_abi PRIVATE eil_substrate)
add_test(NAME abi_guard COMMAND test_abi)

add_executable(test_array tests/test_array.cpp)
target_link_libraries(test_array PRIVATE eil_substrate)
add_test(NAME array_semantics COMMAND test_array)

add_executable(test_aa tests/test_aa.cpp)
target_link_libraries(test_aa PRIVATE eil_substrate)
add_test(NAME associative_arrays COMMAND test_aa)

# Substrate rule for D's `^^` operator: replayed against the ldc2-captured probe.
add_executable(test_substrate_pow tests/test_substrate_pow.cpp)
target_link_libraries(test_substrate_pow PRIVATE eil_substrate)
add_test(NAME substrate_pow COMMAND test_substrate_pow
         ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/substrate_pow.txt)

# Substrate rules for std.range.iota element values and bound strictness.
add_executable(test_substrate_iota tests/test_substrate_iota.cpp)
target_link_libraries(test_substrate_iota PRIVATE eil_substrate)
add_test(NAME substrate_iota COMMAND test_substrate_iota
         ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/substrate_iota.txt)

# --- Wave 1: nm --------------------------------------------------------------------------------
foreach(_m IN ITEMS bracketing brent ridder secant linesearch newton bbla rsla schedule
                    newtoncotes gaussquad rungekutta)
  add_executable(test_nm_${_m} tests/test_nm_${_m}.cpp)
  target_link_libraries(test_nm_${_m} PRIVATE eil_nm eil_substrate)
  add_test(NAME nm_${_m} COMMAND test_nm_${_m})
endforeach()
# smla and stmatrix replay the D unittests against nm/test_data/b1_ss.mtx, so they
# take the matrix file as argv[1] and fail without it.
foreach(_m IN ITEMS smla stmatrix)
  add_executable(test_nm_${_m} tests/test_nm_${_m}.cpp)
  target_link_libraries(test_nm_${_m} PRIVATE eil_nm eil_substrate)
  add_test(NAME nm_${_m} COMMAND test_nm_${_m} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_data/b1_ss.mtx)
endforeach()

foreach(_d IN ITEMS ridder linesearch bbla newtoncotes gaussquad rungekutta)
  add_executable(demo_nm_${_d} tests/demo_nm_${_d}.cpp)
  target_link_libraries(demo_nm_${_d} PRIVATE eil_nm eil_substrate)
endforeach()

# --- Wave 1: util ------------------------------------------------------------------------------
foreach(_u IN ITEMS lua lua_service lua_roundtrip msg_service time_utils)
  add_executable(test_util_${_u} tests/test_util_${_u}.cpp)
  target_link_libraries(test_util_${_u} PRIVATE eil_util eil_substrate)
  add_test(NAME util_${_u} COMMAND test_util_${_u})
endforeach()

# --- Wave 2: geom ------------------------------------------------------------------------------
# Registered per module in cmake/wave2_geom.cmake.

# --- Demo equivalence against the D reference --------------------------------------------------
# Each demo's stdout is compared to the byte-identical D binary's stdout, captured by
# harness/capture_d_demos.sh into baselines/<rev>/demos/.
find_program(BASH_PROGRAM bash)
if(BASH_PROGRAM AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/baselines)
  file(GLOB _demo_baselines ${CMAKE_CURRENT_SOURCE_DIR}/baselines/*/demos/*.txt)
  foreach(_b ${_demo_baselines})
    get_filename_component(_name ${_b} NAME_WE)
    if(TARGET demo_nm_${_name})
      add_test(NAME demo_equiv_${_name}
               COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/harness/compare_text.sh
                       ${_b} $<TARGET_FILE:demo_nm_${_name}>)
    endif()
  endforeach()
endif()

# --- Wave 2 stages -----------------------------------------------------------------------------
# Each Wave 2 stage declares its own targets in a fragment under cmake/ so that
# stages ported in parallel edit disjoint files instead of racing on this one.
# Included last: their add_test() calls must land after enable_testing().
include(cmake/wave2_nm.cmake)
include(cmake/wave2_geom.cmake)
include(cmake/wave2_geom_core.cmake)
include(cmake/wave2_geom_gpath.cmake)
include(cmake/wave2_geom_gpath2.cmake)
include(cmake/wave2_geom_gpath3.cmake)
include(cmake/wave2_geom_misc.cmake)
include(cmake/wave2_gasdyn.cmake)
