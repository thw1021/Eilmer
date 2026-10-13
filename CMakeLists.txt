cmake_minimum_required(VERSION 3.20)
project(Eilmer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Floating point: the port is judged against D output at 1e-13, so nothing here
# may reorder or re-associate arithmetic. Default g++ already conforms, and these
# make an accidental -ffast-math in a parent toolchain impossible.
add_compile_options(-fno-fast-math -fno-unsafe-math-optimizations -ffp-conform=off)
add_compile_options(-Wall -Wextra -Wno-unused-parameter)

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
    src/util/json_helper.cpp
)
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

add_executable(test_array tests/test_array.cpp)
target_link_libraries(test_array PRIVATE eil_substrate)
add_test(NAME array_semantics COMMAND test_array)

add_executable(test_aa tests/test_aa.cpp)
target_link_libraries(test_aa PRIVATE eil_substrate)
add_test(NAME associative_arrays COMMAND test_aa)
