#pragma once
/**
 * lua_service.d
 * Home to some commonly used idioms (collected as functions)
 * when using interfacing with Lua.
 *
 * Author: Rowan G. and Peter J.
 * Version: 2015-01-14
 */
// util/lua_service.d -> src/util/lua_service.hpp (+ lua_service.cpp).
//
// ---------------------------------------------------------------------------
// Ownership
// ---------------------------------------------------------------------------
// Nothing here allocates a D-class object: the module only hands out
// `lua_State*` (owned by the caller; init_lua_State's result is released by
// lua_close, exactly as in D) and copies values into Lua-owned userdata. The one
// place D relied on the GC is `to!string(lua_tostring(L,-1))`: in D that is a
// *slice aliasing Lua's interned string buffer* (std.conv does not copy a
// const(char)*), which stays valid only while Lua keeps the value alive -- a
// latent dangling read after every `getString`/`getArrayOfStrings`, since both
// lua_pop the value before returning it. The port copies into a std::string, so
// the returned text is owned by the caller. Same bytes, strictly longer lifetime.
//
// Objects pushed through pushObj are copied into Lua userdata and, as in D, a
// second reference is kept by the calling module in its own store (the D
// convention `GasModelStore ~= pushObj!(...)`). This module keeps no such store,
// so ownership does not change here; the ported D rule is that the luawrap file
// that calls pushObj owns the object for the life of the interpreter.
//
// ---------------------------------------------------------------------------
// D string parameters
// ---------------------------------------------------------------------------
// D takes `string key` and passes it to the C API through toStringz, which
// allocates a NUL-terminated copy. The port uses std::string and passes
// .c_str(), which is NUL-terminated by invariant, so every handoff into liblua is
// terminated. Reading back, D's `to!string(const(char)*)` on a null pointer gives
// an empty string (measured with a Phobos probe), so detail::string_or_null does
// the same instead of handing a null to the std::string constructor.
//
// ---------------------------------------------------------------------------
// Callbacks: what D's extern(C) templates become
// ---------------------------------------------------------------------------
// util/lua_service.d:528 `extern(C) int toStringObj(T, string metatableName)(lua_State* L)`
// is a Lua callback *generated from a template*, with the metatable name baked in
// as a compile-time string. C++ cannot spell that: a function template may not
// have C language linkage (g++ rejects it with "template with C linkage"), and a
// string literal is not a legal non-type template argument before C++20.
// The port therefore splits the two roles the D template plays:
//   * the body, toStringObj<T>(lua_State*, const char*), keeps C++ linkage and
//     takes the name as an argument -- D's compile-time string only ever reached
//     the C API as a char pointer, so the value it carries is unchanged;
//   * the callback entry point, which must be `extern "C" int(*)(lua_State*)`, is
//     written by the EIL_LUA_TOSTRING_OBJ macro at the registration site. It
//     expands to a real extern "C" function with no captures, so the pointer Lua
//     stores is a plain C function pointer, the same thing D's extern(C)
//     instantiation produced. Lifetime requirement introduced: none. The macro
//     bakes a *string literal* into the generated body, so the pointer it passes
//     to luaL_error has static storage duration and outlives the interpreter.
// The same split applies to every D `extern(C) int f(T, string MT)(lua_State*)`
// in the luawrap files: body function taking the name, macro-generated C shim.
//
// No D delegate, nested function or closure appears in this module. Every Lua C
// function in the D tree is already a plain extern(C) function, so no upvalue or
// context-pointer registry is needed to carry behaviour here.
//
// ---------------------------------------------------------------------------
// luaL_error leaves the frame by longjmp, not by unwinding
// ---------------------------------------------------------------------------
// getNumberFromTable/getIntegerFromTable/getBooleanFromTable (and checkObj, and
// therefore toStringObj) raise through luaL_error, which is a longjmp to Lua's
// protected frame. C++ destructors in the skipped frames do not run -- in D the
// same jump simply leaves GC objects for the collector, so D never leaks. The port
// therefore introduces one requirement and keeps it inside this module: nothing
// between a Lua-facing function and a possible luaL_error may own storage through a
// destructor. The bodies here hold only ints, doubles and references to the
// caller's strings, so they are clean. Call sites must pass messages that outlive
// the jump (a literal, or an object owned by a frame that is not skipped):
// `getNumberFromTable(L, 1, f, true, 0.0, true, format(templ, "niv"))`, the shape
// geom/luawrap/luagpath.d:473 uses, leaks the temporary in C++ where it was free in
// D. tests/test_util_lua_roundtrip.cpp keeps those messages in static objects and
// documents the pattern; the luawrap waves have to follow it. The paths are also
// only meaningful inside a protected call: raised from an unprotected one, Lua
// panics ("unprotected error in call to Lua API"), which the reference reproduces
// byte for byte.
//
// ---------------------------------------------------------------------------
// Default arguments that name the caller
// ---------------------------------------------------------------------------
// D evaluates `string file=__FILE__, size_t line=__LINE__` default arguments at
// the *call site* (measured: a call from /tmp/probe.d line 537 reports that file
// and line, and warnOnStackChange's own internal calls report util/lua_service.d
// 674). A C++ default argument would name this header instead, so the ported
// functions take file/line explicitly and the macros supply the caller's values,
// the way eil/error.hpp:62 and nm/nm_exception.hpp:34 do.

#include <cstddef>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>

#include "util/lua.hpp"

#include "eil/array.hpp"
#include "eil/aa.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"

namespace eil {
namespace util {

/**
 * Custom exception type for signalling Lua input errors.
 *
 * Recipe for custom execptions used from D Cookbook, p. 21.
 *
 * Reference:
 * Ruppe, A.D. (2014)
 * D Cookbook
 * Packt Publishing, Birmingham, UK
 */
// util/lua_service.d:688. D's ctor takes a chained `Throwable next=null`; the
// substrate ExceptionBase has no chain slot and nothing in the tree passes one
// (same omission as nm/nm_exception.hpp:13).
class LuaInputException : public eil::ExceptionBase {
  public:
    LuaInputException(const std::string& message, const char* file = "", size_t line = 0)
        : eil::ExceptionBase(message, file, line) {}
};

// `throw new Error(m)` has D's `object.Error` type, which `catch (Exception e)`
// does NOT catch. See the note on eil::ErrorBase in eil/error.hpp: the substrate
// currently derives ErrorBase from ExceptionBase, so a downstream
// `catch (eil::ExceptionBase&)` will swallow these; reported upward rather than
// worked around here.
#define EIL_THROW_UTIL_ERROR(message) throw ::eil::ErrorBase((message), __FILE__, __LINE__)
#define EIL_THROW_LUA_INPUT(message) \
    throw ::eil::util::LuaInputException((message), __FILE__, __LINE__)

namespace detail {

// std.string.toStringz / `to!string(const(char)*)` as util/lua_service.d uses it:
// D yields a null string of length 0 for a null pointer, and a copy otherwise.
inline std::string string_or_null(const char* p) {
    return p == nullptr ? std::string() : std::string(p);
}

// std.conv.to!int of a numeric value, as used by util/lua_service.d:173, 188,
// 204, 330, 359, 362, 609. Phobos behaviour measured on this machine:
//   NaN               -> ConvException        "Input was NaN"
//   beyond int range  -> ConvOverflowException "Conversion positive overflow"
//                                         or   "Conversion negative overflow"
//   otherwise         -> truncates toward zero
// static_cast<int> would wrap instead of raising, so the ranges are checked.
// eil/conv.hpp's to_int covers the *string* case only, and with different text,
// so this stays local to the Lua layer.
inline int to_int_from_double(double v) {
    if (std::isnan(v)) throw eil::ExceptionBase("Input was NaN");
    // int range, exclusive of the endpoints, checked without overflow.
    if (v >= static_cast<double>(std::numeric_limits<int>::max())) {
        throw eil::ExceptionBase("Conversion positive overflow");
    }
    if (v < static_cast<double>(std::numeric_limits<int>::min())) {
        throw eil::ExceptionBase("Conversion negative overflow");
    }
    return static_cast<int>(v);  // D's to!int truncates toward zero
}
inline int to_int_from_long(long long v) {
    if (v > static_cast<long long>(std::numeric_limits<int>::max())) {
        throw eil::ExceptionBase("Conversion positive overflow");
    }
    if (v < static_cast<long long>(std::numeric_limits<int>::min())) {
        throw eil::ExceptionBase("Conversion negative overflow");
    }
    return static_cast<int>(v);
}

// `to!(T)` where the source is a Lua number (util/lua_service.d:333, 384).
template <typename T> inline T from_lua_number(double v) {
    if constexpr (std::is_same<T, int>::value) {
        return to_int_from_double(v);
    } else {
        return static_cast<T>(v);  // double/float: value preserving
    }
}

} // namespace detail

// ---------------------------------------------------------------------------
// lua_service.d:20-56
// ---------------------------------------------------------------------------
lua_State* init_lua_State();
void doLuaFile(lua_State* L, const std::string& fname);

// util/lua_service.d:35. D's `file=__FILE__, line=__LINE__` default to the
// caller's location, so the call sites use this macro.
void warnOnStackChange(lua_State* L, int old_top, const char* file, size_t line);
#define EIL_WARN_ON_STACK_CHANGE(L, old_top) \
    ::eil::util::warnOnStackChange((L), (old_top), __FILE__, __LINE__)

bool tableEmpty(lua_State* L, int tblIdx);

// ---------------------------------------------------------------------------
// scalar getters (lua_service.d:58-260)
// ---------------------------------------------------------------------------
std::string getString(lua_State* L, int tblIdx, const std::string& key);
std::string getString(lua_State* L, const std::string& key);
std::string getStringWithDefault(lua_State* L, int tblIdx, const std::string& key,
                                const std::string& defaultValue);
double getDouble(lua_State* L, int tblIdx, const std::string& key);
double getDouble(lua_State* L, const std::string& key);
double getDoubleWithDefault(lua_State* L, int tblIdx, const std::string& key,
                            double defaultValue);
int getInt(lua_State* L, int tblIdx, const std::string& key);
int getInt(lua_State* L, const std::string& key);
int getIntWithDefault(lua_State* L, int tblIdx, const std::string& key, int defaultValue);
bool getBool(lua_State* L, int tblIdx, const std::string& key);
bool getBool(lua_State* L, const std::string& key);
bool getBoolWithDefault(lua_State* L, int tblIdx, const std::string& key, bool defaultValue);

// ---------------------------------------------------------------------------
// array getters (lua_service.d:262-430)
// ---------------------------------------------------------------------------
// D's `out string[] values` reseats the caller's array, so the port takes the
// caller's owned Array by reference rather than a view.
void getArrayOfStrings(lua_State* L, int tblIdx, const std::string& key,
                       eil::Array<std::string>& values);
void getArrayOfStrings(lua_State* L, const std::string& key, eil::Array<std::string>& values);
void getArrayOfInts(lua_State* L, int tblIdx, const std::string& key, eil::Array<int>& values);

// util/lua_service.d:312 -- the constraint `if (canCastTo!(T, double))` is kept.
template <typename T>
typename std::enable_if<eil::util::canCastTo<T, double>, void>::type
getArrayOfDoubles(lua_State* L, int tblIdx, const std::string& key, eil::Array<T>& values);

// util/lua_service.d:370. D's `out T[string] params` is an associative array the
// callee fills; pList is a `string[]` view.
template <typename T>
void getAssocArrayOfDoubles(lua_State* L, const std::string& key, eil::Slice<std::string> pList,
                            eil::AA<std::string, T>& params);

template <size_t N>
void getMatrixOfDoubles(lua_State* L, int tblIdx, double (&values)[N][N]);
template <size_t N>
void pushMatrixOfDoubles(lua_State* L, const double (&values)[N][N]);

// ---------------------------------------------------------------------------
// userdata (lua_service.d:451-535)
// ---------------------------------------------------------------------------
// D's `pushObj!(T, metatableName)` becomes `pushObj<T>(L, obj, metatableName)`:
// the metatable name is the string D would have converted with toStringz, passed
// as an argument instead of as a template parameter. T must be the ported
// representation of the D type -- a D class reference ports as `T*`, which keeps
// the `sizeof` written into the userdata at 8 bytes, as D's class reference is.
template <typename T>
T pushObj(lua_State* L, T obj, const char* metatableName);

template <typename T>
T checkObj(lua_State* L, int index, const char* metatableName);

bool isObjType(lua_State* L, int index, const std::string& tname);

// Body of util/lua_service.d:528; see the callback note at the top of the file.
template <typename T>
int toStringObj(lua_State* L, const char* metatableName);

// Registers a Lua C function equivalent to D's `toStringObj!(T, MTname)`.
#define EIL_LUA_TOSTRING_OBJ(fname, T, metatable_name)         \
    extern "C" int fname(lua_State* L) {                        \
        return ::eil::util::toStringObj<T>(L, (metatable_name)); \
    }

// ---------------------------------------------------------------------------
// tolerant field readers (lua_service.d:556-676)
// ---------------------------------------------------------------------------
// D's default sentinels measured: double.init is the standard quiet NaN
// (0x7ff8000000000000, prints "nan"), int.init is 0, bool.init is false.
double getNumberFromTable(lua_State* L, int index, const std::string& field,
                          bool errorIfNotFound = false,
                          double valIfError = std::numeric_limits<double>::quiet_NaN(),
                          bool errorIfNotValid = false, const std::string& errMsg = "");

int getIntegerFromTable(lua_State* L, int index, const std::string& field,
                        bool errorIfNotFound = false,
                        int valIfError = 0,
                        bool errorIfNotValid = false, const std::string& errMsg = "");

bool getBooleanFromTable(lua_State* L, int index, const std::string& field,
                         bool errorIfNotFound = false,
                         bool valIfError = false,
                         bool errorIfNotValid = false, const std::string& errMsg = "");

bool checkAllowedNames(lua_State* L, int tblIndx, eil::Slice<std::string> allowedNames);

// ===========================================================================
// Templates
// ===========================================================================

// util/lua_service.d:312-339
template <typename T>
typename std::enable_if<eil::util::canCastTo<T, double>, void>::type
getArrayOfDoubles(lua_State* L, int tblIdx, const std::string& key, eil::Array<T>& values) {
    int old_top = lua_gettop(L);
    if (!lua_istable(L, tblIdx)) {
        std::string errMsg = eil::format("getArrayOfDoubles was expecting a table at stack index: %d", tblIdx);
        EIL_THROW_UTIL_ERROR(errMsg);
    }
    values.set_length(0);
    lua_getfield(L, tblIdx, key.c_str());
    if (!lua_istable(L, -1)) {
        std::string errMsg = eil::format("A table of numbers was expected in field: %s", key);
        lua_pop(L, 1);
        EIL_THROW_UTIL_ERROR(errMsg);
    }
    auto n = detail::to_int_from_long(static_cast<long long>(lua_objlen(L, -1)));
    for (int i = 1; i < n + 1; ++i) {
        lua_rawgeti(L, -1, i);
        if (lua_isnumber(L, -1)) values.append(detail::from_lua_number<T>(lua_tonumber(L, -1)));
        // Silently ignore anything that isn't a value.
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    warnOnStackChange(L, old_top, __FILE__, __LINE__);
}

// util/lua_service.d:370-388
template <typename T>
void getAssocArrayOfDoubles(lua_State* L, const std::string& key, eil::Slice<std::string> pList,
                            eil::AA<std::string, T>& params) {
    int old_top = lua_gettop(L);
    if (!lua_istable(L, -1)) {
        std::string errMsg =
            eil::format("getAssocArrayOfDoubles was expecting a table at stack index: %d", -1);
        EIL_THROW_UTIL_ERROR(errMsg);
    }
    lua_getfield(L, -1, key.c_str());
    if (!lua_istable(L, -1)) {
        std::string errMsg =
            eil::format("A table with key and values (doubles) was expected in field: %s", key);
        lua_pop(L, 1);
        EIL_THROW_UTIL_ERROR(errMsg);
    }
    for (size_t k = 0; k < pList.length(); ++k) {
        const std::string p = pList[k];
        params.set(p, detail::from_lua_number<T>(getDouble(L, -1, p)));
    }
    lua_pop(L, 1);
    warnOnStackChange(L, old_top, __FILE__, __LINE__);
}

// util/lua_service.d:392-410. As in D, the table is read through the top of the
// stack (-1), so the caller must leave it there, and it is left on return.
template <size_t N>
void getMatrixOfDoubles(lua_State* L, int tblIdx, double (&values)[N][N]) {
    int old_top = lua_gettop(L);
    if (!lua_istable(L, tblIdx)) {
        std::string errMsg =
            eil::format("getArrayOfDoubles was expecting a table at stack index: %d", tblIdx);
        EIL_THROW_UTIL_ERROR(errMsg);
    }
    for (size_t i = 0; i < N; ++i) {
        lua_rawgeti(L, -1, detail::to_int_from_long(static_cast<long long>(i + 1)));
        for (size_t j = 0; j < N; ++j) {
            lua_rawgeti(L, -1, detail::to_int_from_long(static_cast<long long>(j + 1)));
            values[i][j] = (lua_isnumber(L, -1)) ? luaL_checknumber(L, -1) : 0.0;
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }
    // Table is left on stack but no other items should be added to stack.
    warnOnStackChange(L, old_top, __FILE__, __LINE__);
}

// util/lua_service.d:414-430
template <size_t N>
void pushMatrixOfDoubles(lua_State* L, const double (&values)[N][N]) {
    lua_newtable(L);  // for whole matrix
    int old_top = lua_gettop(L);
    for (size_t i = 0; i < N; ++i) {
        // Build a new row.
        lua_newtable(L);
        for (size_t j = 0; j < N; ++j) {
            lua_pushnumber(L, values[i][j]);
            lua_rawseti(L, -2, detail::to_int_from_long(static_cast<long long>(j + 1)));
        }
        // Completed row sitting at top of stack.
        lua_rawseti(L, -2, detail::to_int_from_long(static_cast<long long>(i + 1)));
    }
    // Table is left on stack but we have cleaned up otherwise.
    warnOnStackChange(L, old_top, __FILE__, __LINE__);
}

// util/lua_service.d:451-459
template <typename T>
T pushObj(lua_State* L, T obj, const char* metatableName) {
    auto ptr = static_cast<T*>(lua_newuserdata(L, sizeof(T)));
    *ptr = obj;  // Make a copy of the object into the Lua domain.
    luaL_getmetatable(L, metatableName);
    lua_setmetatable(L, -2);
    return obj;  // Back in the C++ domain, we should keep a reference.
    // It is the responsibility of the calling code to do so.
}

// util/lua_service.d:470-476
template <typename T>
T checkObj(lua_State* L, int index, const char* metatableName) {
    if (lua_isnil(L, index)) {
        luaL_error(L, "nil value as pointer to type %s", metatableName);
    }
    auto ptr = static_cast<T*>(luaL_checkudata(L, index, metatableName));
    if (ptr == nullptr) {
        luaL_error(L, "null pointer to type %s", metatableName);
    }
    return *ptr;
}

// util/lua_service.d:528-535
template <typename T>
int toStringObj(lua_State* L, const char* metatableName) {
    if (lua_isnil(L, 1)) {
        luaL_error(L, "nil value as pointer to type %s", metatableName);
    }
    auto path = checkObj<T>(L, 1, metatableName);
    // D's `path is null` only compiles for the class-reference instantiations, so
    // the test is made for pointer-like T and skipped for the rest.
    if constexpr (std::is_pointer<T>::value) {
        if (path == nullptr) {
            luaL_error(L, "null pointer to type %s", metatableName);
        }
    }
    // D spells a class-reference member access with a dot; the ported class
    // reference is a pointer, so the same access is `->` for pointer-like T and
    // stays `.` for a value type pushed directly.
    if constexpr (std::is_pointer<T>::value) {
        lua_pushstring(L, path->toString().c_str());
    } else {
        lua_pushstring(L, path.toString().c_str());
    }
    return 1;
}

} // namespace util
} // namespace eil
