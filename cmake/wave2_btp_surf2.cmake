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
