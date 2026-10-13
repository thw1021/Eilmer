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

# --- geom/surface/beziertrianglepatch.d (T-012) ----------------------------------
# Not a ParametricSurface subclass (D:5-9), so it links against eil_geom's gpath
# half for the Bezier edge curves its second fitter takes.
target_sources(eil_geom PRIVATE
    src/geom/surface/beziertrianglepatch.cpp
)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_geom_surface_beziertrianglepatch.cpp)
  add_executable(test_geom_surface_beziertrianglepatch
                 tests/test_geom_surface_beziertrianglepatch.cpp)
  target_link_libraries(test_geom_surface_beziertrianglepatch
                        PRIVATE eil_geom eil_nm eil_substrate)
  add_test(NAME geom_surface_beziertrianglepatch
           COMMAND test_geom_surface_beziertrianglepatch
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_surface_beziertrianglepatch.txt)
  # The probe's two point-cloud fits drive nelmin, so the test gets an explicit
  # timeout rather than relying on the default.
  set_tests_properties(geom_surface_beziertrianglepatch PROPERTIES TIMEOUT 900)
endif()

# --- surface subclasses, T-012 stage A (bezierpatch, coonspatch, channelpatch,
#     cubepatch, sweptpathpatch). Each block self-registers behind an EXISTS guard so
#     a half-written module cannot break another agent's configure.
foreach(_m IN ITEMS bezierpatch coonspatch channelpatch cubepatch sweptpathpatch)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/geom/surface/${_m}.cpp)
    target_sources(eil_geom PRIVATE src/geom/surface/${_m}.cpp)
  endif()
endforeach()

foreach(_m IN ITEMS surface_bezierpatch surface_coonspatch surface_channelpatch surface_cubepatch surface_sweptpathpatch)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_geom_${_m}.cpp)
    add_executable(test_geom_${_m} tests/test_geom_${_m}.cpp)
    target_link_libraries(test_geom_${_m} PRIVATE eil_geom eil_nm eil_substrate)
    add_test(NAME geom_${_m}
             COMMAND test_geom_${_m}
                     ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_${_m}.txt)
  endif()
endforeach()

# --- volume stage A: meshvolume.d (14 lines, one empty class) -------------------
# No target_sources: meshvolume.d declares no member and no free function, so the
# port is header-only (src/geom/volume/meshvolume.hpp) and there is no TU to link.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_geom_volume_meshvolume.cpp)
  add_executable(test_geom_volume_meshvolume tests/test_geom_volume_meshvolume.cpp)
  target_link_libraries(test_geom_volume_meshvolume PRIVATE eil_geom eil_nm eil_substrate)
  add_test(NAME geom_volume_meshvolume
           COMMAND test_geom_volume_meshvolume
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_volume_meshvolume.txt)
endif()

# --- volume stage A: subrangedvolume.d (56 lines) --------------------------------
target_sources(eil_geom PRIVATE src/geom/volume/subrangedvolume.cpp)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_geom_volume_subrangedvolume.cpp)
  add_executable(test_geom_volume_subrangedvolume tests/test_geom_volume_subrangedvolume.cpp)
  target_link_libraries(test_geom_volume_subrangedvolume PRIVATE eil_geom eil_nm eil_substrate)
  add_test(NAME geom_volume_subrangedvolume
           COMMAND test_geom_volume_subrangedvolume
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_volume_subrangedvolume.txt)
endif()

# --- geom/surface stage: the SubRangedSurface adapter (T-012, 45 D lines) --------
# Replay test only: geom/surface/subrangedsurface.d has no `unittest` block (measured
# 0 occurrences) and geom/surface has no per-module demo file (`ls | grep -i demo`
# finds only the directory-level surface_demo.d, which demonstrates CoonsPatch).
target_sources(eil_geom PRIVATE
    src/geom/surface/subrangedsurface.cpp
)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_geom_surface_subrangedsurface.cpp)
  add_executable(test_geom_surface_subrangedsurface
                 tests/test_geom_surface_subrangedsurface.cpp)
  target_link_libraries(test_geom_surface_subrangedsurface
                        PRIVATE eil_geom eil_nm eil_substrate)
  add_test(NAME geom_surface_subrangedsurface
           COMMAND test_geom_surface_subrangedsurface
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_surface_subrangedsurface.txt)
endif()

# --- geom/surface stage: RuledSurface (T-012, 88 D lines) ------------------------
# Replay test only: geom/surface/ruledsurface.d has no `unittest` block (measured 0
# occurrences) and geom/surface has no per-module demo file.
target_sources(eil_geom PRIVATE
    src/geom/surface/ruledsurface.cpp
)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_geom_surface_ruledsurface.cpp)
  add_executable(test_geom_surface_ruledsurface tests/test_geom_surface_ruledsurface.cpp)
  target_link_libraries(test_geom_surface_ruledsurface PRIVATE eil_geom eil_nm eil_substrate)
  add_test(NAME geom_surface_ruledsurface
           COMMAND test_geom_surface_ruledsurface
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_surface_ruledsurface.txt)
endif()

# --- geom/surface stage: NozzleExpansionPatch (T-012, 135 D lines) ---------------
# Replay test only: the D module has no `unittest` block (measured 0 occurrences) and
# geom/surface has no per-module demo file.
target_sources(eil_geom PRIVATE
    src/geom/surface/nozzleexpansionpatch.cpp
)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_geom_surface_nozzleexpansionpatch.cpp)
  add_executable(test_geom_surface_nozzleexpansionpatch
                 tests/test_geom_surface_nozzleexpansionpatch.cpp)
  target_link_libraries(test_geom_surface_nozzleexpansionpatch
                        PRIVATE eil_geom eil_nm eil_substrate)
  add_test(NAME geom_surface_nozzleexpansionpatch
           COMMAND test_geom_surface_nozzleexpansionpatch
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_surface_nozzleexpansionpatch.txt)
endif()

# --- volume stage A: twosurfacevolume.d (153 D lines) ---------------------------
target_sources(eil_geom PRIVATE
    src/geom/volume/twosurfacevolume.cpp
)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_geom_volume_twosurfacevolume.cpp)
  add_executable(test_geom_volume_twosurfacevolume
                 tests/test_geom_volume_twosurfacevolume.cpp)
  target_link_libraries(test_geom_volume_twosurfacevolume
                        PRIVATE eil_geom eil_nm eil_substrate)
  add_test(NAME geom_volume_twosurfacevolume
           COMMAND test_geom_volume_twosurfacevolume
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_volume_twosurfacevolume.txt)
endif()

# --- volume stage A: slabvolume.d (65 D lines) ----------------------------------
# SlabVolume derives from SweptSurfaceVolume (slabvolume.d:13), which is another
# stage's module and is not in this batch, so both the source line and the test block
# are guarded: the tree still configures until that file lands.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/geom/volume/sweptsurfacevolume.hpp)
  target_sources(eil_geom PRIVATE
      src/geom/volume/slabvolume.cpp
  )
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_geom_volume_slabvolume.cpp AND
    EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/geom/volume/sweptsurfacevolume.hpp)
  add_executable(test_geom_volume_slabvolume tests/test_geom_volume_slabvolume.cpp)
  target_link_libraries(test_geom_volume_slabvolume PRIVATE eil_geom eil_nm eil_substrate)
  add_test(NAME geom_volume_slabvolume
           COMMAND test_geom_volume_slabvolume
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_volume_slabvolume.txt)
endif()
