// tests/test_util_lua.cpp -- port of the unittest in util/lua.d:19-52.
//
// That unittest is the only test the D file has, and it is all about the canCastTo
// trait: D's `isFloatingPoint` cannot see the solver's own scalar types (Complex,
// Dual), so lua_pushnumber is templated on `canCastTo!(T, lua_Number)` instead
// (lua.d:98). Every static assert in the D block becomes a static assert here, so a
// trait that stops matching D fails the build, not a run.

#include <cstdio>
#include <string>

#include "util/lua.hpp"

namespace {

int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    } else {
        std::printf("ok: %s\n", what);
    }
}

// D: struct MyType(T) { T inner; R opCast(R : T)() const { return inner; } }
template <typename T> struct MyType {
    T inner;
    template <typename R> R opCast() const { return static_cast<R>(inner); }
};

}  // namespace

// A Lua callback with the C linkage the Lua API requires: lua_pushcfunction must
// accept its address and Lua must be able to call it.
extern "C" int sevenFromC(lua_State* S) {
    lua_pushinteger(S, 7);
    return 1;
}

int main() {
    using eil::util::canCastTo;

    // lua.d:34-37 -- the opCast branch.
    static_assert(canCastTo<MyType<double>, double>, "lua.d:34");
    static_assert(canCastTo<MyType<float>, float>, "lua.d:35");
    static_assert(canCastTo<MyType<double>, float>, "lua.d:36");
    static_assert(canCastTo<MyType<float>, double>, "lua.d:37");

    // lua.d:39-41 -- D's `is(T : U)` accepts both widening and narrowing.
    static_assert(canCastTo<double, double>, "lua.d:39");
    static_assert(canCastTo<float, double>, "lua.d:40");
    static_assert(canCastTo<double, float>, "lua.d:41");

    // lua.d:48-51
    static_assert(canCastTo<int, double>, "lua.d:48");
    static_assert(canCastTo<char, double>, "lua.d:49");
    static_assert(canCastTo<bool, double>, "lua.d:50");
    static_assert(!canCastTo<std::string, double>, "lua.d:51");

    // lua.d:29-32 -- the cast itself produces the right type and value.
    MyType<double> value{1.0};
    double casted = eil::util::d_cast_to_lua_Number(value);  // D: cast(double)(value)
    check(casted == 1.0, "opCast route yields the stored value");

    int i1 = 1;
    double d1 = static_cast<double>(i1);  // lua.d:44
    check(d1 == 1.0, "int to double cast equals 1.0");

    // lua.d:56-75: the constants the module pins. These are static_asserts in
    // lua.hpp; checked again here so a Lua version bump fails this test too.
    check(LUAI_MAXSTACK == 1000000, "LUAI_MAXSTACK (lua.d:56)");
    check(LUA_REGISTRYINDEX == -1001000, "LUA_REGISTRYINDEX (lua.d:59)");
    check(LUA_RIDX_GLOBALS == 2, "LUA_RIDX_GLOBALS (lua.d:60)");
    check(LUA_MULTRET == -1, "LUA_MULTRET (lua.d:61)");
    check(LUA_TNIL == 0 && LUA_TBOOLEAN == 1 && LUA_TTABLE == 5 && LUA_TFUNCTION == 6,
          "type codes (lua.d:63-66)");
    check(LUA_GCSTOP == 0 && LUA_GCCOLLECT == 2 && LUA_GCSETSTEPMUL == 7,
          "gc codes (lua.d:68-75)");

    // The wrapper functions D defines (lua.d:90,92,105,112,136,137,147) have to keep
    // working against the real library, so exercise them on a live state.
    lua_State* L = luaL_newstate();
    check(L != nullptr, "luaL_newstate (lua.d:157)");
    luaL_openlibs(L);
    check(luaL_dostring(L, "x = 1") == 0, "luaL_dostring (lua.d:166)");
    check(lua_gettop(L) == 0, "lua_gettop starts empty (lua.d:86)");
    lua_pushglobaltable(L);  // lua.d:105
    check(lua_istable(L, -1), "lua_pushglobaltable leaves the globals table");
    lua_pop(L, 1);  // lua.d:90
    check(lua_gettop(L) == 0, "lua_pop empties again");
    lua_newtable(L);  // lua.d:136
    check(lua_type(L, -1) == LUA_TTABLE, "lua_newtable");
    lua_pushnumber(L, 2.5);   // non-template overload
    lua_rawseti(L, -2, 1);
    lua_pushnumber(L, 2);     // templated overload, lua.d:98
    lua_rawseti(L, -2, 2);
    lua_pushnumber(L, 1.5f);  // templated overload through float
    lua_rawseti(L, -2, 3);
    MyType<double> m{4.25};
    lua_pushnumber(L, m);  // templated overload through opCast
    lua_rawseti(L, -2, 4);
    check(lua_objlen(L, -1) == 4, "lua_objlen counts the four pushed entries (lua.d:147)");
    lua_rawgeti(L, -1, 4);
    check(lua_tonumber(L, -1) == 4.25, "opCast value reached Lua as a number");
    lua_pop(L, 1);
    lua_rawgeti(L, -1, 3);
    check(lua_tonumber(L, -1) == 1.5, "float argument reached Lua as 1.5");
    lua_pop(L, 1);
    lua_pop(L, 1);  // drop the table
    lua_pushinteger(L, 10);
    lua_pushinteger(L, 20);
    lua_remove(L, -2);  // lua.d:92
    check(lua_gettop(L) == 1 && lua_tointeger(L, -1) == 20, "lua_remove drops one slot");
    lua_pop(L, 1);
    lua_pushcfunction(L, &sevenFromC);  // lua.d:137
    check(lua_isfunction(L, -1), "lua_pushcfunction leaves a function (lua.d:137)");
    if (lua_pcall(L, 0, 1, 0) == 0) {  // lua.d:119
        check(lua_tointeger(L, -1) == 7, "the pushed C function returns 7 through lua_pcall");
    } else {
        check(false, "lua_pcall of the pushed C function");
    }
    lua_pop(L, 1);
    lua_pushstring(L, "abc");
    check(lua_isstring(L, -1) && std::string(lua_tostring(L, -1)) == "abc",
          "lua_pushstring/lua_tostring (lua.d:100,145)");
    lua_pop(L, 1);
    lua_newuserdata(L, 16);  // lua.d:112
    check(lua_isuserdata(L, -1), "lua_newuserdata accepts a table");
    lua_pop(L, 1);
    lua_close(L);

    std::printf("%s: util.lua (canCastTo, constants, wrappers) (%d failures)\n",
                failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
