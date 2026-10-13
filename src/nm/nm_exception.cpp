// nm/nm_exception.cpp — translation unit for module nm.nm_exception.
//
// NumericalMethodException is a header-only class (D's exception ctor body is a
// single delegating call), so there is no non-trivial code to move out of the
// header. This file exists because CMakeLists lists it and it verifies the
// header compiles on its own.

#include "nm/nm_exception.hpp"
