# cmake/wave2_geom_surfvol.cmake — geom/surface/ and geom/volume/ (T-012).
#
# Included last from the top-level CMakeLists.txt. eil_geom is declared in
# cmake/wave2_geom.cmake; this fragment only appends its own sources and its own
# test blocks, so the surface/volume stages can be filled in without touching
# anyone else's file.
#
# The two abstract bases are written by the main thread rather than a stage agent,
# because every subclass overrides their virtual contract (wave-02-brief 4b) and a
# per-agent spelling of `dup()`/`operator()` would fork that contract.

target_sources(eil_geom PRIVATE
    src/geom/surface/parametricsurface.cpp
    src/geom/volume/parametricvolume.cpp
)

# Base classes registered; the subclasses add their own blocks below as they land.
foreach(_m IN ITEMS surface_parametricsurface volume_parametricvolume)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_geom_${_m}.cpp)
    add_executable(test_geom_${_m} tests/test_geom_${_m}.cpp)
    target_link_libraries(test_geom_${_m} PRIVATE eil_geom eil_nm eil_substrate)
    add_test(NAME geom_${_m}
             COMMAND test_geom_${_m}
                     ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_${_m}.txt)
  endif()
endforeach()
