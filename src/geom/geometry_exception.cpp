// geom/geometry_exception.d -> src/geom/geometry_exception.cpp
//
// GeometryException is a header-only class (D's ctor body is a single delegating
// call to Exception's, geometry_exception.d:11-15), so there is no non-trivial code
// to move out of the header. This file exists because cmake/wave2_geom.cmake lists
// it and it verifies the header compiles standalone (wave-1 lesson 4).

#include "geom/geometry_exception.hpp"
