// util/lua.d -> src/util/lua.cpp
//
// Nothing in util/lua.d has storage or a non-inline body: it is a trait, a set of
// `extern (C)` declarations satisfied by liblua.a, and inline wrappers. This TU
// exists because CMakeLists lists src/util/lua.cpp, and it does two jobs:
//   (1) proves util/lua.hpp compiles on its own, and
//   (2) instantiates the templated parts against the kinds the D tree uses, so a
//       regression shows up here rather than in whichever luawrap file gets there
//       first.

#include "util/lua.hpp"

namespace eil {
namespace util {
namespace lua_check {

// The overload set the solver relies on: a plain double, an int, and a D-style
// type that only offers `opCast`.
struct OnlyOpCast {
    double inner = 0.0;
    template <typename R> R opCast() const { return static_cast<R>(inner); }
};

void push_all(lua_State* L, double d, int i, float f, const OnlyOpCast& o) {
    lua_pushnumber(L, d);   // non-template C function
    lua_pushnumber(L, i);   // util/lua.d:98 template
    lua_pushnumber(L, f);   // util/lua.d:98 template
    lua_pushnumber(L, o);   // util/lua.d:98 template through opCast
    lua_pushinteger(L, static_cast<lua_Integer>(i));
    lua_pushboolean(L, 1);
    lua_pushstring(L, "x");
    lua_pop(L, 1);
    lua_remove(L, -1);
    lua_newtable(L);
    lua_objlen(L, -1);
    lua_pushglobaltable(L);
    lua_pop(L, 2);
}

// D's trait used the way util/lua_service.d:316 uses it.
static_assert(eil::util::canCastTo<double, double>, "lua.d:39");
static_assert(eil::util::canCastTo<float, double>, "lua.d:40");
static_assert(eil::util::canCastTo<double, float>, "lua.d:41");
static_assert(eil::util::canCastTo<int, double>, "lua.d:48");
static_assert(eil::util::canCastTo<char, double>, "lua.d:49");
static_assert(eil::util::canCastTo<bool, double>, "lua.d:50");
static_assert(!eil::util::canCastTo<std::string, double>, "lua.d:51");
static_assert(eil::util::canCastTo<OnlyOpCast, double>, "lua.d:34");

} // namespace lua_check
} // namespace util
} // namespace eil
