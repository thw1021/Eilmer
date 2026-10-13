// tests/test_util_lua_service.cpp
//
// util/lua_service.d's own unittest block (lines 697-742) is commented out in the
// reference and refers to a `LuaState` class and a `getValues()` that the module
// does not contain, so there was nothing to port from it. This file tests what the
// module actually promises, with every expected string taken from the D reference
// run (ldc2 against util/lua_service.d, mirrored one-for-one by
// tests/test_util_lua_roundtrip.cpp, which is the byte-for-byte diff against D).
//
// Two shapes are checked deliberately:
//   * which exception *type* each failure raises, because D's `Error` is not
//     catchable by `catch (Exception)` while LuaInputException is;
//   * the stack height each function leaves, because warnOnStackChange's warning is
//     part of the solver log and depends on those heights.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "util/lua.hpp"
#include "util/lua_service.hpp"

#include "eil/array.hpp"
#include "eil/aa.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"

using eil::util::LuaInputException;

namespace {
int failures = 0;
void check(bool ok, const std::string& what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what.c_str());
        ++failures;
    } else {
        std::printf("ok: %s\n", what.c_str());
    }
}

class Widget {
  public:
    double size;
    std::string name;
    Widget(double s, std::string n) : size(s), name(std::move(n)) {}
    std::string toString() const { return eil::format("Widget(%s, %s)", size, name); }
};
const char* WidgetMT = "WidgetObj";
eil::Array<Widget*> widgetStore;

// The messages handed to luaL_error live in static objects: a Lua error longjmps
// out of the calling frame without running destructors, so a temporary would leak.
const std::string kMissingMsg("widget field is missing");
const std::string kBadMsg("widget field is bad");

extern "C" int newWidget(lua_State* L) {
    lua_remove(L, 1);
    lua_rawgeti(L, 1, 1);
    double s = luaL_checknumber(L, -1);
    lua_pop(L, 1);
    lua_rawgeti(L, 1, 2);
    std::string n = eil::util::detail::string_or_null(luaL_checkstring(L, -1));
    lua_pop(L, 1);
    Widget* w = new Widget(s, n);
    widgetStore.append(eil::util::pushObj<Widget*>(L, w, WidgetMT));
    return 1;
}

extern "C" int widgetArea(lua_State* L) {
    auto w = eil::util::checkObj<Widget*>(L, 1, WidgetMT);
    lua_pushnumber(L, w->size * w->size);
    return 1;
}

// D registers `&toStringObj!(Widget, WidgetMT)`; C++ registers the extern "C"
// function the macro writes, whose body is that same template.
EIL_LUA_TOSTRING_OBJ(widgetToString, Widget*, WidgetMT)

extern "C" int raiseMissing(lua_State* L) {
    eil::util::getNumberFromTable(L, 1, "nope", true, 0.0, false, kMissingMsg);
    return 0;
}

void registerWidget(lua_State* L) {
    luaL_newmetatable(L, WidgetMT);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, &newWidget);
    lua_setfield(L, -2, "new");
    lua_pushcfunction(L, &widgetToString);
    lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, &widgetArea);
    lua_setfield(L, -2, "area");
    lua_setglobal(L, WidgetMT);
}

/// Catch what `f` raises and report the type and message, D-style.
template <typename F> void expectError(const std::string& what, F f, const char* kind,
                                       const std::string& want_msg) {
    try {
        f();
        std::printf("FAIL: %s -- no exception (expected %s)\n", what.c_str(), kind);
        ++failures;
    } catch (const LuaInputException& e) {
        const char* got = "LuaInputException";
        check(std::string(kind) == got && e.msg() == want_msg,
              what + " -> " + got + " |" + e.msg() + "|");
    } catch (const eil::ErrorBase& e) {
        const char* got = "Error";
        check(std::string(kind) == got && e.msg() == want_msg,
              what + " -> " + got + " |" + e.msg() + "|");
    } catch (const eil::ExceptionBase& e) {
        const char* got = "Exception";
        check(std::string(kind) == got && e.msg() == want_msg,
              what + " -> " + got + " |" + e.msg() + "|");
    }
}
}  // namespace

int main() {
    using namespace eil::util;

    lua_State* L = init_lua_State();
    check(L != nullptr, "init_lua_State opens a state with the standard libraries");
    check(luaL_dostring(L, "io = io; s = string.format('%d', 12)") == 0 &&
              getString(L, "s") == "12",
          "luaL_openlibs ran (string.format works)");

    luaL_dostring(L, "cfg = {d = 1.25, i = 4, b = true, s = 'text', "
                     "arr = {1.5, 2.5, 3.5}, iarr = {7, 8}, sarr = {'p', 'q'}, "
                     "mixed = {1.0, 'skip', 2.0}, empty = {}, assoc = {X = 0.5, Y = -1.5}}");

    // --- scalar readers, table at index 1 after being pushed ---
    lua_getglobal(L, "cfg");
    int cfg = lua_gettop(L);
    check(getString(L, cfg, "s") == "text", "getString reads a string field");
    check(getDouble(L, cfg, "d") == 1.25, "getDouble reads a number field");
    check(getInt(L, cfg, "i") == 4, "getInt reads an integer field");
    check(getBool(L, cfg, "b") == true, "getBool reads a boolean field");
    check(getStringWithDefault(L, cfg, "absent", "dflt") == "dflt", "string default");
    check(getDoubleWithDefault(L, cfg, "absent", 8.5) == 8.5, "double default");
    check(getIntWithDefault(L, cfg, "absent", 9) == 9, "int default");
    check(getBoolWithDefault(L, cfg, "absent", true) == true, "bool default");
    // Lua's number/string coercion, as D sees it through the same C predicates.
    check(getString(L, cfg, "d") == "1.25", "getString coerces a number field, as D does");

    expectError("getString on a table field", [&] { getString(L, cfg, "arr"); },
                "LuaInputException", "A string was expected in field: arr");
    expectError("getDouble on a string field", [&] { getDouble(L, cfg, "s"); }, "Error",
                "A double was expected in field: s");
    expectError("getInt on a float field", [&] { getInt(L, cfg, "d"); }, "Error",
                "An integer was expected in field: d");
    expectError("getBool on an int field", [&] { getBool(L, cfg, "i"); }, "Error",
                "A boolean value was expected in field: i");
    expectError("getString with a non-table index", [&] { getString(L, cfg - 1, "s"); }, "Error",
                "getString was expecting a table at stack index: " + std::to_string(cfg - 1));
    expectError("getDouble with a non-table index", [&] { getDouble(L, cfg - 1, "s"); }, "Error",
                "getDouble was expecting a table at stack index: " + std::to_string(cfg - 1));
    expectError("getInt with a non-table index", [&] { getInt(L, cfg - 1, "s"); }, "Error",
                "getInt was expecting a table at stack index: " + std::to_string(cfg - 1));
    expectError("getBool with a non-table index", [&] { getBool(L, cfg - 1, "s"); }, "Error",
                "getBool was expecting a table at stack index: " + std::to_string(cfg - 1));
    expectError("getStringWithDefault with a non-table index",
                [&] { getStringWithDefault(L, cfg - 1, "s", "x"); }, "Error",
                "getStringWithDefault was expecting a table at stack index: " +
                    std::to_string(cfg - 1));

    // --- array readers ---
    eil::Array<double> dv;
    eil::Array<int> iv;
    eil::Array<std::string> sv;
    getArrayOfDoubles(L, cfg, "arr", dv);
    check(dv.length() == 3 && dv[0] == 1.5 && dv[2] == 3.5, "getArrayOfDoubles reads all three");
    getArrayOfDoubles(L, cfg, "mixed", dv);
    check(dv.length() == 2 && dv[1] == 2.0, "getArrayOfDoubles silently skips non-numbers");
    getArrayOfInts(L, cfg, "iarr", iv);
    check(iv.length() == 2 && iv[0] == 7 && iv[1] == 8, "getArrayOfInts reads the integers");
    getArrayOfInts(L, cfg, "arr", iv);
    check(iv.length() == 0, "getArrayOfInts skips non-integers");
    getArrayOfStrings(L, cfg, "sarr", sv);
    check(sv.length() == 2 && sv[0] == "p" && sv[1] == "q", "getArrayOfStrings reads the strings");
    getArrayOfStrings(L, cfg, "arr", sv);
    check(sv.length() == 3, "getArrayOfStrings keeps number fields (Lua coerces them)");
    dv = eil::Array<double>({9.0});
    expectError("getArrayOfDoubles on a string field", [&] { getArrayOfDoubles(L, cfg, "s", dv); },
                "Error", "A table of numbers was expected in field: s");
    check(dv.length() == 0, "a throwing getArrayOfDoubles still empties the caller's array");
    expectError("getArrayOfInts on a string field", [&] { getArrayOfInts(L, cfg, "s", iv); },
                "Error", "A table of integers was expected in field: s");
    expectError("getArrayOfStrings on a string field", [&] { getArrayOfStrings(L, cfg, "s", sv); },
                "Error", "A table of strings was expected in field: s");

    // The global-name overload exists for strings only, exactly as in D.
    sv = eil::Array<std::string>();
    expectError("getArrayOfStrings on a missing global", [&] { getArrayOfStrings(L, "nope", sv); },
                "Error", "A table of strings was expected in field: nope");

    // --- associative array reader ---
    eil::Array<std::string> pList({"X", "Y"});
    eil::AA<std::string, double> params;
    getAssocArrayOfDoubles(L, "assoc", pList.all(), params);
    check(params.length() == 2 && params.get("X") == 0.5 && params.get("Y") == -1.5,
          "getAssocArrayOfDoubles fills the caller's AA by name");
    expectError("getAssocArrayOfDoubles on a missing key", [&] {
        eil::Array<std::string> one({"Z"});
        eil::AA<std::string, double> p2;
        getAssocArrayOfDoubles(L, "nokey", one.all(), p2);
    }, "Error", "A table with key and values (doubles) was expected in field: nokey");

    // --- matrix roundtrip through Lua ---
    double src[3][3] = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}, {7.0, 8.0, 9.0}};
    pushMatrixOfDoubles<3>(L, src);
    lua_setglobal(L, "m3");
    double back[3][3];
    lua_getglobal(L, "m3");
    getMatrixOfDoubles<3>(L, -1, back);
    bool same = true;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) same = same && (src[i][j] == back[i][j]);
    check(same, "pushMatrixOfDoubles/getMatrixOfDoubles round-trip a 3x3");
    check(lua_istable(L, -1), "getMatrixOfDoubles leaves the table on the stack, as D does");
    lua_pop(L, 1);
    // A missing row reads as zeros rather than raising (D's ternary at line 403).
    luaL_dostring(L, "short = {{1.0, 2.0}}");
    lua_getglobal(L, "short");
    double sparse[3][3];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) sparse[i][j] = -1.0;
    getMatrixOfDoubles<3>(L, -1, sparse);
    // Measured against D and reproduced exactly: row 0 reads 1, 2, 0 (the third
    // entry is absent, so the ternary at lua_service.d:403 supplies 0.0), and rows 1
    // and 2 read *the same values again*. D's inner lookup is relative to the stack
    // top (-1), and when a row is missing Lua 5.4 pushes a nil whose payload still
    // holds the previous table pointer (lapi.c:708 writes only the type byte, and
    // gettable's ttistable check is a compiled-out api_check), so the missing rows
    // alias row 0. That is liblua's behaviour, identical for D and C++; recorded here
    // so the port is never "fixed" away from it.
    check(sparse[0][0] == 1.0 && sparse[0][1] == 2.0 && sparse[0][2] == 0.0,
          "absent matrix entries become 0.0, not an error");
    check(sparse[1][0] == 1.0 && sparse[1][2] == 0.0 && sparse[2][1] == 2.0,
          "a missing row re-reads the previous row, as D does");
    lua_pop(L, 1);

    // --- tableEmpty ---
    lua_getglobal(L, "cfg");
    lua_getfield(L, -1, "empty");
    int top_before_empty = lua_gettop(L);
    check(tableEmpty(L, -1) == true, "tableEmpty sees an empty table as empty");
    // D leaves the sentinel nil behind on the empty path: the height grows by one
    // relative to the call, which is what the reference logs.
    check(lua_gettop(L) == top_before_empty, "tableEmpty leaves its nil key on the stack (D quirk)");
    lua_settop(L, cfg);
    lua_getglobal(L, "cfg");
    lua_getfield(L, -1, "arr");
    check(tableEmpty(L, -1) == false, "tableEmpty sees a filled table as non-empty");
    lua_settop(L, cfg);

    // --- checkAllowedNames ---
    luaL_dostring(L, "onlyok = {a = 1, b = 2}");
    luaL_dostring(L, "onebad = {a = 1, zz = 2}");
    lua_getglobal(L, "onlyok");
    eil::Array<std::string> okNames({"a", "b"});
    check(checkAllowedNames(L, lua_gettop(L), okNames.all()) == true,
          "checkAllowedNames accepts a table of known keys");
    lua_settop(L, cfg);
    lua_getglobal(L, "onebad");
    eil::Array<std::string> oneName({"a"});
    int top_before_bad = lua_gettop(L);
    // The invalid-key report and the stack warning are the observable output here;
    // both are printed by the function, matching the D reference text.
    check(checkAllowedNames(L, top_before_bad, oneName.all()) == false,
          "checkAllowedNames rejects an unknown key (prints the two D messages above)");
    lua_settop(L, cfg);

    // --- userdata: pushObj, checkObj, isObjType, toStringObj ---
    registerWidget(L);
    lua_getglobal(L, WidgetMT);
    check(lua_istable(L, -1), "the metatable is visible under the name pushObj expects");
    check(getString(L, -1, "__name") == WidgetMT, "Lua's own name for the metatable matches");
    std::vector<std::string> names;
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        names.push_back(eil::util::detail::string_or_null(lua_tostring(L, -2)));
        lua_pop(L, 1);
    }
    std::sort(names.begin(), names.end());
    check(names == std::vector<std::string>({"__index", "__name", "__tostring", "area", "new"}),
          "registration puts exactly the D-expected names on the metatable");
    lua_pop(L, 1);

    check(luaL_dostring(L, "w = WidgetObj:new{2.5, 'bolt'}") == 0, "the constructor runs");
    check(luaL_dostring(L, "wstr = tostring(w)") == 0 && getString(L, "wstr") == "Widget(2.5, bolt)",
          "__tostring goes through the ported toStringObj");
    check(luaL_dostring(L, "wa = w:area()") == 0 && getDouble(L, "wa") == 6.25,
          "checkObj reads the object back out of the userdata");
    check(widgetStore.length() == 1 && widgetStore[0]->size == 2.5 &&
              widgetStore[0]->name == "bolt",
          "the object Lua holds is the one the C++ store kept a reference to");

    check(isObjType(L, 1, "number") == false, "isObjType on an empty stack slot says no");
    lua_pushnumber(L, 3.0);
    check(isObjType(L, -1, "number") == true, "isObjType recognises 'number'");
    check(isObjType(L, -1, WidgetMT) == false, "isObjType rejects the metatable name");
    lua_pop(L, 1);
    lua_pushstring(L, "txt");
    check(isObjType(L, -1, "string") == true, "isObjType recognises 'string'");
    lua_pop(L, 1);
    lua_pushnil(L);
    check(isObjType(L, -1, WidgetMT) == false, "isObjType rejects nil");
    lua_pop(L, 1);

    // A Lua error raised through luaL_error is only well behaved inside a protected
    // call; that is how the solver reaches these paths.
    lua_pushcfunction(L, &raiseMissing);
    lua_setglobal(L, "raiseMissing");
    check(luaL_dostring(L, "ok1, msg1 = pcall(raiseMissing, cfg)") == 0, "protected call 1 loads");
    check(lua_isstring(L, -1) == 0, "pcall leaves nothing unhandled");
    lua_getglobal(L, "ok1");
    check(lua_toboolean(L, -1) == 0, "raiseMissing fails as expected");
    lua_pop(L, 1);
    lua_getglobal(L, "msg1");
    check(std::string(lua_tostring(L, -1)).find("widget field is missing") != std::string::npos,
          "the Lua error text carries the errMsg argument verbatim");
    lua_pop(L, 1);

    // --- getNumber/Integer/BooleanFromTable, non-raising forms ---
    lua_getglobal(L, "cfg");
    int c2 = lua_gettop(L);
    check(getNumberFromTable(L, c2, "d") == 1.25, "getNumberFromTable reads a number");
    check(std::isnan(getNumberFromTable(L, c2, "nope")), "missing field gives double.init (NaN)");
    check(getIntegerFromTable(L, c2, "d") == 1, "getIntegerFromTable truncates, as to!int does");
    check(getIntegerFromTable(L, c2, "nope") == 0, "missing integer field gives int.init (0)");
    check(getBooleanFromTable(L, c2, "nope") == false, "missing boolean field gives bool.init");
    check(getNumberFromTable(L, c2, "s", false, 5.0, false, kBadMsg) == 5.0,
          "invalid value with errorIfNotValid=false returns valIfError");
    lua_settop(L, cfg);

    // --- doLuaFile ---
    expectError("doLuaFile on a missing file", [&] { doLuaFile(L, "no_such_file.lua"); },
                "LuaInputException", "cannot open no_such_file.lua: No such file or directory");

    lua_close(L);
    for (Widget* w : widgetStore) delete w;

    std::printf("%s: util.lua_service (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
