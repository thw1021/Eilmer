# cmake/wave2_btp_surf2.cmake — one T-012 stage's targets.
#
# Owns only its own target_sources(eil_geom ...) and test blocks. Each stage gets a
# separate fragment so stages ported in parallel edit disjoint files (the wave-01
# lesson about racing on one CMakeLists). Tests self-register behind if(EXISTS) so the
# tree keeps configuring while this stage's files are still landing.

# --- geom/surface/spherepatch.d (T-012 stage btp_surf2, 171 D lines) --------------
# SpherePatch is a ParametricSurface subclass, so it follows the wave-02-brief 4b
# contract: dup() -> shared_ptr<ParametricSurface>, opCall -> operator()(r,s) const,
# toString(), and classString() declared but NOT marked override (D declares no
# classString anywhere in geom/surface: measured 0 matches).
target_sources(eil_geom PRIVATE
    src/geom/surface/spherepatch.cpp
)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_geom_surface_spherepatch.cpp)
  add_executable(test_geom_surface_spherepatch tests/test_geom_surface_spherepatch.cpp)
  target_link_libraries(test_geom_surface_spherepatch PRIVATE eil_geom eil_nm eil_substrate)
  add_test(NAME geom_surface_spherepatch
           COMMAND test_geom_surface_spherepatch
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_surface_spherepatch.txt)
endif()

# --- geom/surface/aopatch.d (T-012 stage btp_surf2, 240 D lines) ------------------
# Composes the already-ported CoonsPatch as its near-boundary fall-back. Its copy
# constructor (D:90-102) leaves _nx/_ny at int.init == 0, which is measured rather than
# "fixed" — see the header note.
target_sources(eil_geom PRIVATE
    src/geom/surface/aopatch.cpp
)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_geom_surface_aopatch.cpp)
  add_executable(test_geom_surface_aopatch tests/test_geom_surface_aopatch.cpp)
  target_link_libraries(test_geom_surface_aopatch PRIVATE eil_geom eil_nm eil_substrate)
  add_test(NAME geom_surface_aopatch
           COMMAND test_geom_surface_aopatch
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_surface_aopatch.txt)
endif()

# --- geom/surface/controlpointpatch.d (T-012 stage btp_surf2, 340 D lines) --------
# Composes the already-ported CoonsPatch, ChannelPatch and this stage's AOPatch as its
# four guide-patch branches; the boundary Paths are optional (D:46-49).
target_sources(eil_geom PRIVATE
    src/geom/surface/controlpointpatch.cpp
)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_geom_surface_controlpointpatch.cpp)
  add_executable(test_geom_surface_controlpointpatch
                 tests/test_geom_surface_controlpointpatch.cpp)
  target_link_libraries(test_geom_surface_controlpointpatch
                        PRIVATE eil_geom eil_nm eil_substrate)
  add_test(NAME geom_surface_controlpointpatch
           COMMAND test_geom_surface_controlpointpatch
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_surface_controlpointpatch.txt)
endif()
