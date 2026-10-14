# cmake/wave2_grid_g5.cmake — T-011 stage G5 (see reports/wave-02-grid-brief.md).
#
# Owns only its own target_sources(eil_geom ...) and test blocks; tests self-register
# behind if(EXISTS) so the tree keeps configuring while this stage's files land.
#
# Scope: geom/grid/usgrid.d (USGCell_type, USGFace, USGCell, BoundaryFaceSet,
# UnstructuredGrid, 2524 D lines) -> src/geom/grid/usgrid.{hpp,cpp},
# probes/geom/grid/probe_usgrid.d,
# baselines/e587b235/probes/geom_grid_usgrid.txt (1393 rows),
# tests/test_geom_grid_usgrid.cpp.
#
# Dependency order actually observed (grid brief sections 8 and 10.1): usgrid.d needs
# G1 (Grid), G2 (paver.d: PavedGrid + POINT_LIST/FACE_LIST/CELL_LIST, taken from
# src/geom/grid/paver.hpp:194-197 rather than re-declared here), G3
# (geom.grid.paver2d: fill_interior), geom/misc/kdtree (joinGrid, D:2210-2242) and
# gzip; and it needs G4's StructuredGrid for the constructor at D:553. The two
# headers forward-declare each other; usgrid.cpp includes sgrid.hpp.
#
# Alignment with the already-green G3 gate (grid brief section 10.4): the real
# USGFace/USGCell/USGCell_type/makeFaceTag defined here are field-for-field the same
# shapes that tests/test_geom_grid_paver2d.cpp transcribed as a fixture (that file's
# own note names usgrid.d:112/:154/:218/:40-56), so geom_grid_paver2d's 480 rows are
# unaffected. Its fixture stays in place — that test file belongs to stage G3.

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/geom/grid/usgrid.cpp)
  target_sources(eil_geom PRIVATE src/geom/grid/usgrid.cpp)
endif()

# Registration is all-or-nothing per the grid brief section 10.5: the add_test()
# lines below stay switched off until tests/test_geom_grid_usgrid.cpp replays all
# 1393 rows at 0 differ. A source file in src/ compiles without producing a red test.
#
# Gate for the corpus replay (put in place when the row count reaches 0 differ):
#
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_geom_grid_usgrid.cpp)
  add_executable(test_geom_grid_usgrid tests/test_geom_grid_usgrid.cpp)
  target_link_libraries(test_geom_grid_usgrid PRIVATE eil_geom eil_nm eil_substrate)
  # add_test(NAME geom_grid_usgrid
  #          COMMAND test_geom_grid_usgrid
  #                  ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_grid_usgrid.txt)
endif()
