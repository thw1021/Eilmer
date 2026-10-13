# cmake/wave2_geom_gpath3.cmake — verification for helix, polyline, modifiedpath,
# xspline and xsplinelsq (the second half of the already-written gpath modules).
#
# Owns only target_sources(eil_geom ...) and its own test blocks; eil_geom itself is
# declared in cmake/wave2_geom.cmake and this fragment is included from the top-level
# CMakeLists.txt after the other gpath fragments.
#
# The five gpath sources of this slice are the deliverable of the port. The five
# modules they depend on (path, line, arc, bezier, polynomial) are being registered
# by the sibling gpath fragments, but this slice's tests cannot link without them,
# so they are listed here too: CMake de-duplicates identical target_sources entries,
# so when the sibling fragments land the object is still built exactly once.

target_sources(eil_geom PRIVATE
    src/geom/gpath/helix.cpp
    src/geom/gpath/polyline.cpp
    src/geom/gpath/modifiedpath.cpp
    src/geom/gpath/xspline.cpp
    src/geom/gpath/xsplinelsq.cpp
    # Shared prerequisites (owned by cmake/wave2_geom_gpath.cmake).
    src/geom/gpath/path.cpp
    src/geom/gpath/line.cpp
    src/geom/gpath/arc.cpp
    src/geom/gpath/bezier.cpp
    src/geom/gpath/polynomial.cpp
)

# One test per D module. Each replays the D `unittest` block assertion for
# assertion *and* replays its ldc2 probe corpus line by line, reporting how many
# printed quantities are bit-identical and the worst residual in ULPs (the shape of
# tests/test_geom_elements_properties.cpp).
#
# The baseline is argv[1]; the tests/test_data directory is argv[2] for the two
# modules whose probes read coordinate files through a Path constructor
# (wave-01 lesson 3 — a test that reads a data file must be given its path).
foreach(_m IN ITEMS helix modifiedpath)
  add_executable(test_geom_gpath_${_m} tests/test_geom_gpath_${_m}.cpp)
  target_link_libraries(test_geom_gpath_${_m} PRIVATE eil_geom eil_nm eil_substrate)
  add_test(NAME geom_gpath_${_m}
           COMMAND test_geom_gpath_${_m}
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_gpath_${_m}.txt)
endforeach()

foreach(_m IN ITEMS polyline xspline xsplinelsq)
  add_executable(test_geom_gpath_${_m} tests/test_geom_gpath_${_m}.cpp)
  target_link_libraries(test_geom_gpath_${_m} PRIVATE eil_geom eil_nm eil_substrate)
  add_test(NAME geom_gpath_${_m}
           COMMAND test_geom_gpath_${_m}
                   ${CMAKE_CURRENT_SOURCE_DIR}/baselines/e587b235/probes/geom_gpath_${_m}.txt
                   ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_data)
endforeach()
