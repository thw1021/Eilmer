// nm/rsla.d -> src/nm/rsla.cpp
//
// rsla.d is a template module: every entry point is templated on the static
// dimensions (N, NDIM, NDIM2) and the scalar type T, so the definitions live in
// rsla.hpp. This TU exists because CMakeLists lists it, and it verifies the
// header compiles on its own.

#include "nm/rsla.hpp"
