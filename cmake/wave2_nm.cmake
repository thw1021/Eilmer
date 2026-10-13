# cmake/wave2_nm.cmake — Wave 2 prerequisites from the nm leaf layer + gzip.
#
# These three nm modules were listed as "migrate with the wave that consumes
# them" (backlog T-905), but geom/gpath/{xspline,xsplinelsq,gpath_utils}.d,
# geom/surface/beziertrianglepatch.d and geom/grid/sgrid.d import them, so Wave 2
# has to carry them. extern/gzip/gzip.d is here for the same reason: sgrid.d and
# usgrid.d define read_from_gzip_file()/write_to_gzip_file().
#
# Layout convention: append `target_sources(eil_nm PRIVATE src/nm/x.cpp)` blocks
# and one test-registration block per D module.
