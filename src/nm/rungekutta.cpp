// nm/rungekutta.d -> src/nm/rungekutta.cpp
//
// Both entry points are templates (on the scalar type and, for the stepper, on
// the user's derivative function), so their definitions live in rungekutta.hpp as
// D keeps them in the module body. This TU exists because CMakeLists lists it, and
// it verifies the header compiles alone.

#include "nm/rungekutta.hpp"
