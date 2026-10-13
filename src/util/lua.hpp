#pragma once
/**
 * A Lua interface layer, wrapping the Lua 5.4 C API the way the solver expects it.
 *
 * Author: A.M.M (D original, 2023-12-01)
 */
// util/lua.d -> src/util/lua.hpp (module util.lua). Header only: every entity in
// the D file is either a compile-time trait, a declaration of a function that
// liblua already exports, or an inline wrapper around one. lua.cpp exists only
// because CMakeLists lists it, and checks that this header stands alone.
//
// ---------------------------------------------------------------------------
// Why this header includes the real lua.h instead of re-declaring the API
// ---------------------------------------------------------------------------
// util/lua.d:54-167 re-declares 60-odd `extern (C)` prototypes by hand. Those
// re-declarations are not free: `lua_rawgeti`/`lua_rawseti` were declared with an
// `int` index where Lua's third parameter is a lua_Integer (64-bit), and
// `lua_KContext` was aliased to `int` where luaconf.h:642 says ptrdiff_t (so
// D's `sizeof(lua_KContext)` is 4, C++'s is 8 -- printed by the reference probe).
// Both differences are invisible at runtime because every call in the tree passes
// a non-negative small index and a ctx of 0, which zero-extends; the port binds to
// the genuine prototypes instead, so those arguments widen to the same values and
// no call site can observe a difference. The constants D pins (LUA_TNIL and
// friends) are static_asserted against the headers below, so a Lua version change
// that moves one of them fails the build rather than the solver.
//
// Names util/lua.d defines *as wrappers* (lua_pop, lua_remove, lua_newtable,
// lua_pushcfunction, lua_pushglobaltable, lua_pcall, lua_tostring, lua_tonumber,
// lua_tointeger, lua_objlen, lua_newuserdata, luaL_checkstring, luaL_checkint,
// luaL_loadfile, luaL_dofile, luaL_dostring, luaL_getmetatable, lua_istable,
// lua_isnil, lua_isboolean, lua_isfunction) already exist as function macros in
// lua.h/lauxlib.h with byte-identical bodies, so the macro form is kept: D's
// wrapper and the macro expand to the same calls. The two exceptions are
// `lua_objlen` (no macro in 5.4) and the templated `lua_pushnumber`, both
// provided here.
//
// Consequently the ported names stay at global scope, exactly as in D, where
// `import util.lua;` puts `lua_gettop` and friends into the importing module
// unqualified. ~70 Lua-facing files (gas/*.luagas_model, geom/luawrap/*,
// eilmer/lua*.d, kinetics/lua*) call them unqualified; namespacing them would
// force a qualification change at every one of those call sites without changing
// any behaviour. The D-only *traits and helpers* are in eil::util.

#include <cstddef>
#include <cstring>
#include <string>
#include <type_traits>
#include <utility>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

// ---------------------------------------------------------------------------
// Constants util/lua.d:56-75 pins. Asserted against the headers, not repeated.
// ---------------------------------------------------------------------------
static_assert(LUAI_MAXSTACK == 1000000, "util/lua.d:56 LUAI_MAXSTACK");
static_assert(LUA_REGISTRYINDEX == -LUAI_MAXSTACK - 1000, "util/lua.d:59");
static_assert(LUA_RIDX_GLOBALS == 2, "util/lua.d:60");
static_assert(LUA_MULTRET == -1, "util/lua.d:61");
static_assert(LUA_TNIL == 0 && LUA_TBOOLEAN == 1 && LUA_TTABLE == 5 &&
                  LUA_TFUNCTION == 6, "util/lua.d:63-66");
static_assert(LUA_GCSTOP == 0 && LUA_GCRESTART == 1 && LUA_GCCOLLECT == 2 &&
                  LUA_GCCOUNT == 3 && LUA_GCCOUNTB == 4 && LUA_GCSTEP == 5 &&
                  LUA_GCSETPAUSE == 6 && LUA_GCSETSTEPMUL == 7, "util/lua.d:68-75");
// util/lua.d:81-83 aliases: lua_Number=double, lua_KContext=int,
// lua_Integer=ptrdiff_t. The first and third are checked; the second is the
// documented 4-vs-8 difference above.
static_assert(sizeof(lua_Number) == sizeof(double), "util/lua.d:81");
static_assert(std::is_signed_v<lua_Integer> && sizeof(lua_Integer) == 8,
              "util/lua.d:83 lua_Integer = ptrdiff_t");
static_assert(sizeof(lua_CFunction) == sizeof(void*), "util/lua.d:78");

namespace eil {
namespace util {

// ---------------------------------------------------------------------------
// canCastTo (util/lua.d:8-17)
// ---------------------------------------------------------------------------
// D: `is(T : U) || is(ReturnType!(T.opCast!U) == U)`. `is(T : U)` is implicit
// convertibility, which std::is_convertible covers for every case the D tree
// exercises (int/char/bool -> double true, string -> double false, and both
// directions of double/float, which D's unittest at util/lua.d:39-41 asserts
// true). The second branch is a member template `R opCast(R : T)()`; a type that
// only offers opCast is not std::is_convertible, so it is detected directly.
template <typename T, typename U, typename = void> struct has_opCast_to : std::false_type {};
template <typename T, typename U>
struct has_opCast_to<
    T, U, std::void_t<decltype(std::declval<const T&>().template opCast<U>())>>
    : std::is_same<decltype(std::declval<const T&>().template opCast<U>()), U> {};

template <typename T, typename U>
inline constexpr bool canCastTo =
    std::is_convertible_v<T, U> || has_opCast_to<T, U>::value;

// D's `cast(U) v`. For an implicitly convertible type this is static_cast; for a
// type whose only route is the D-style `opCast<U>()` member template, that is
// what D's cast calls, so the port calls it too. Used by lua_pushnumber below,
// matching util/lua.d:98 `cast(lua_Number) n`.
template <typename T> inline lua_Number d_cast_to_lua_Number(const T& v) {
    if constexpr (std::is_convertible_v<T, lua_Number>) {
        return static_cast<lua_Number>(v);
    } else {
        return v.template opCast<lua_Number>();
    }
}

} // namespace util
} // namespace eil

// ---------------------------------------------------------------------------
// Wrapper functions util/lua.d defines in terms of the C API
// ---------------------------------------------------------------------------
// util/lua.d:147 `size_t lua_objlen(lua_State* L, int idx) {return lua_rawlen(L,idx);}`
// There is no lua_objlen macro in Lua 5.4's headers, so it is defined here.
inline size_t lua_objlen(lua_State* L, int idx) { return lua_rawlen(L, idx); }

// util/lua.d:98 -- the templated overload that lets Complex and Dual numbers be
// pushed without coupling this module to them. The non-template C
// `lua_pushnumber(lua_State*, lua_Number)` is excluded from the template so the
// body's own call cannot recurse; D reaches the same function for a double
// argument because overload resolution prefers the non-template, so nothing
// observable changes.
template <typename T>
typename std::enable_if<eil::util::canCastTo<T, lua_Number> &&
                            !std::is_same<typename std::decay<T>::type, lua_Number>::value,
                        void>::type
lua_pushnumber(lua_State* L, T n) {
    lua_pushnumber(L, eil::util::d_cast_to_lua_Number(n));
}
