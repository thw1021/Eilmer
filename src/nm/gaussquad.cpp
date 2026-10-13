// nm/gaussquad.d -> src/nm/gaussquad.cpp
//
// The two rules are templates on the user function and the scalar type, so their
// definitions live in gaussquad.hpp (as D keeps them in the module body). The
// module-level rule tables are `inline` variables in that header, so this TU has
// no definition of its own; it exists because CMakeLists lists it and it checks
// that the header compiles alone.

#include "nm/gaussquad.hpp"
