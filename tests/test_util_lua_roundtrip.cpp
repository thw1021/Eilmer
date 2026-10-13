// tests/test_util_lua_roundtrip.cpp -- functional test of the Lua host layer:
// create a lua_State through the port, execute a script, register a C-bound
// function the way lua_service does, call it from Lua and read the result back.
//
// util/lua.d has no unittest of its own beyond the canCastTo trait (that is
// tests/test_util_lua.cpp), and util/lua_service.d's unittest block
// (lua_service.d:697-742) is commented out in the reference and refers to a LuaState
// class and a getValues() that do not exist in the module, so there was no test to
// port. What the module's contract actually is, is what the solver's 20 Lua-facing
// files do with it, so this file reproduces those uses and is diffed byte-for-byte
// against the D reference (/tmp/probe.d, built against util/lua.d +
// util/lua_service.d). Every printed line here has the same text as the
// corresponding line there; a difference is a FAIL and also shows in the diff.
//
// Lines whose text names the source location cannot match by construction --
// warnOnStackChange and msg_service print the calling file and line, which is the
// behaviour under test (D resolves __FILE__/__LINE__ defaults at the call site) --
// so they are compared after normalising location to <LOC>.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "util/lua.hpp"
#include "util/lua_service.hpp"
#include "util/msg_service.hpp"
#include "util/time_utils.hpp"

#include "eil/array.hpp"
#include "eil/aa.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"

using eil::util::LuaInputException;

namespace {

int failures = 0;

// Reported only when something differs, so a clean run's output matches D's.
void expect(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

// Printable forms matching the reference probe's joinNums/joinStrs.
template <typename T> std::string joinNums(const eil::Array<T>& a) {
    std::string s = "[";
    for (size_t i = 0; i < a.length(); ++i) {
        if (i > 0) s += " ";
        s += eil::to_string(a[i]);
    }
    return s + "]";
}
std::string joinStrs(const eil::Array<std::string>& a) {
    std::string s = "[";
    for (size_t i = 0; i < a.length(); ++i) {
        if (i > 0) s += " ";
        s += "\"" + a[i] + "\"";
    }
    return s + "]";
}
std::string joinStrsVec(const std::vector<std::string>& a) {
    std::string s = "[";
    for (size_t i = 0; i < a.size(); ++i) {
        if (i > 0) s += " ";
        s += "\"" + a[i] + "\"";
    }
    return s + "]";
}

// ----------------------------------------------------------------- test object
// A D class reference ports as a pointer, so pushObj/checkObj write the same 8
// bytes into the userdata that D's `obj.sizeof` reports for a class.
class Probe {
  public:
    double v;
    std::string tag;
    Probe(double v_, std::string tag_) : v(v_), tag(std::move(tag_)) {}
    std::string toString() const {
        return eil::format("Probe(v=%s, tag=%s)", v, tag);
    }
};

const char* ProbeMT = "ProbeObj";
eil::Array<Probe*> probeStore;  // D: static const(Probe)[] probeStore;

extern "C" int newProbe(lua_State* L) {
    lua_remove(L, 1);  // remove the self argument
    lua_rawgeti(L, 1, 1);
    double v = luaL_checknumber(L, -1);
    lua_pop(L, 1);
    lua_rawgeti(L, 1, 2);
    std::string tag = eil::util::detail::string_or_null(luaL_checkstring(L, -1));
    lua_pop(L, 1);
    Probe* p = new Probe(v, tag);
    probeStore.append(eil::util::pushObj<Probe*>(L, p, ProbeMT));
    return 1;
}

extern "C" int probeDoubled(lua_State* L) {
    auto p = eil::util::checkObj<Probe*>(L, 1, ProbeMT);
    lua_pushnumber(L, 2.0 * p->v);
    return 1;
}

extern "C" int probeTypeName(lua_State* L) {
    // isObjType against several names, the way the geom luawrap files do.
    if (eil::util::isObjType(L, 1, "number")) { lua_pushstring(L, "number"); return 1; }
    if (eil::util::isObjType(L, 1, "string")) { lua_pushstring(L, "string"); return 1; }
    if (eil::util::isObjType(L, 1, ProbeMT))  { lua_pushstring(L, "ProbeObj"); return 1; }
    if (eil::util::isObjType(L, 1, "Other"))  { lua_pushstring(L, "Other"); return 1; }
    lua_pushstring(L, "none");
    return 1;
}

extern "C" int badUdataCall(lua_State* L) {
    // Deliberately used on a nil argument, to capture luaL_error's text.
    auto p = eil::util::checkObj<Probe*>(L, 1, ProbeMT);
    lua_pushnumber(L, p->v);
    return 1;
}

extern "C" int stackGrower(lua_State* L) {
    lua_pushnumber(L, 1.0);  // leaves one extra value on the stack
    return 0;
}

// The __tostring callback: D writes `&toStringObj!(Probe, ProbeMT)`, which C++
// cannot spell (a function template may not have C linkage), so the macro emits the
// extern "C" entry point that calls the same body.
EIL_LUA_TOSTRING_OBJ(probeToStringObj, Probe*, ProbeMT)

// A Lua error raised by luaL_error longjmps out of this frame, skipping the
// destructors of anything with one. The messages therefore live in static objects,
// so the longjmp cannot leak them.
const std::string msgStrBad("field str is bad");
const std::string msgNumMissing("missing field num");
const std::string msgIntBad("field str is not an int");
const std::string msgBoolBad("field num is not bool");
const std::string msgUnused("unused");

// The getNumber/Integer/BooleanFromTable family is only meaningful inside a
// protected Lua call, because its error paths go through luaL_error (longjmp).
extern "C" int cfgGood(lua_State* L) {
    lua_pushnumber(L, eil::util::getNumberFromTable(L, 1, "num"));
    lua_pushnumber(L, eil::util::getNumberFromTable(L, 1, "missing"));  // double.init
    lua_pushnumber(L, eil::util::getNumberFromTable(L, 1, "str", false, 7.0, false, msgUnused));
    lua_pushnumber(L, eil::util::getNumberFromTable(L, 1, "strnum", false, 7.0, true, msgUnused));
    lua_pushinteger(L, eil::util::getIntegerFromTable(L, 1, "int"));
    lua_pushinteger(L, eil::util::getIntegerFromTable(L, 1, "num"));
    lua_pushinteger(L, eil::util::getIntegerFromTable(L, 1, "missing"));  // int.init
    lua_pushinteger(L, eil::util::getIntegerFromTable(L, 1, "strnum", false, 33, false, msgUnused));
    lua_pushboolean(L, eil::util::getBooleanFromTable(L, 1, "flag"));
    lua_pushboolean(L, eil::util::getBooleanFromTable(L, 1, "missing"));  // bool.init
    lua_pushboolean(L, eil::util::getBooleanFromTable(L, 1, "truthy"));
    // Not-a-number field, errorIfNotValid left false: D's integer and boolean
    // variants return without popping, so they leave the stack one higher.
    lua_pushinteger(L, eil::util::getIntegerFromTable(L, 1, "str", false, 0, false, msgUnused));
    lua_pushinteger(L, lua_gettop(L));  // stack height seen inside the C function
    lua_pushboolean(L, eil::util::getBooleanFromTable(L, 1, "str", false, false, false, msgUnused));
    lua_pushinteger(L, lua_gettop(L));  // and again after the boolean variant
    lua_pushnumber(L, eil::util::getNumberFromTable(L, 1, "int", false, 0.0, false, msgUnused));
    lua_pushinteger(L, lua_gettop(L));  // the double variant does pop
    return 16;
}

extern "C" int cfgErrNumberNotValid(lua_State* L) {
    auto v = eil::util::getNumberFromTable(L, 1, "str", false, 1.0, true, msgStrBad);
    lua_pushnumber(L, v);
    return 1;
}
extern "C" int cfgErrNumberNotFound(lua_State* L) {
    auto v = eil::util::getNumberFromTable(L, 1, "missing", true, 2.0, false, msgNumMissing);
    lua_pushnumber(L, v);
    return 1;
}
extern "C" int cfgErrIntNotValid(lua_State* L) {
    auto v = eil::util::getIntegerFromTable(L, 1, "str", false, 3, true, msgIntBad);
    lua_pushinteger(L, v);
    return 1;
}
extern "C" int cfgErrBoolNotValid(lua_State* L) {
    auto v = eil::util::getBooleanFromTable(L, 1, "num", false, true, true, msgBoolBad);
    lua_pushboolean(L, v);
    return 1;
}

void registerProbe(lua_State* L) {
    luaL_newmetatable(L, ProbeMT);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, &newProbe);
    lua_setfield(L, -2, "new");
    lua_pushcfunction(L, &probeToStringObj);
    lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, &probeDoubled);
    lua_setfield(L, -2, "doubled");
    lua_pushcfunction(L, &probeTypeName);
    lua_setfield(L, -2, "typeName");
    lua_pushcfunction(L, &badUdataCall);
    lua_setfield(L, -2, "badNil");
    lua_setglobal(L, ProbeMT);
}

// ------------------------------------------------------------- helper sections
void section(lua_State*, const std::string& name) {
    std::printf("--- %s ---\n", name.c_str());
}

void runString(lua_State* L, const std::string& src) {
    if (luaL_dostring(L, src.c_str()) != 0) {
        eil::writefln("LUA-ERROR |%s|", eil::util::detail::string_or_null(lua_tostring(L, -1)));
        lua_pop(L, 1);
    }
}

/// Run a Lua expression through pcall, printing ok and the message.
void runProtected(lua_State* L, const std::string& src) {
    lua_settop(L, 0);
    std::string code = "__ok, __msg = pcall(function() return " + src + " end)";
    if (luaL_dostring(L, code.c_str()) != 0) {
        eil::writefln("LOAD-ERROR |%s|", eil::util::detail::string_or_null(lua_tostring(L, -1)));
        lua_pop(L, 1);
        return;
    }
    lua_getglobal(L, "__ok");
    eil::writeln("pcall ok ", lua_toboolean(L, -1) != 0);
    lua_pop(L, 1);
    lua_getglobal(L, "__msg");
    if (lua_isstring(L, -1)) {
        eil::writefln("pcall msg |%s|", eil::util::detail::string_or_null(lua_tostring(L, -1)));
    } else {
        eil::writefln("pcall msg type %d", lua_type(L, -1));
    }
    lua_pop(L, 1);
    lua_settop(L, 0);
}

// D's tryGet catches LuaInputException, then Exception, then Error. C++ needs the
// most-derived first, and eil::ErrorBase currently derives from eil::ExceptionBase
// (see the note in lua_service.hpp), so Error is tested before Exception.
template <typename F> void tryGet(const std::string& label, F f) {
    try {
        f();
        eil::writeln(label, " no-throw");
    } catch (const LuaInputException& e) {
        eil::writeln(label, " LuaInputException |", e.msg(), "|");
    } catch (const eil::ErrorBase& e) {
        eil::writeln(label, " Error |", e.msg(), "|");
    } catch (const eil::ExceptionBase& e) {
        eil::writeln(label, " Exception |", e.msg(), "|");
    }
}

void write_child_files() {
    { std::ofstream f("/tmp/probe_child.lua"); f << "error('child failure')\n"; }
    { std::ofstream f("/tmp/probe_syntax.lua"); f << "x = 1 +\n"; }
    { std::ofstream f("/tmp/probe_ok.lua"); f << "loaded_ok = 1.25\n"; }
}

}  // namespace

int main() {
    using namespace eil::util;

    lua_State* L = init_lua_State();

    section(L, "constants");
    eil::writefln("LUAI_MAXSTACK %d", (int)LUAI_MAXSTACK);
    eil::writefln("LUA_REGISTRYINDEX %d", LUA_REGISTRYINDEX);
    eil::writefln("LUA_RIDX_GLOBALS %d", LUA_RIDX_GLOBALS);
    eil::writefln("LUA_MULTRET %d", LUA_MULTRET);
    eil::writefln("types %d %d %d %d", LUA_TNIL, LUA_TBOOLEAN, LUA_TTABLE, LUA_TFUNCTION);
    eil::writefln("gc %d %d %d %d %d %d %d %d", LUA_GCSTOP, LUA_GCRESTART, LUA_GCCOLLECT,
                  LUA_GCCOUNT, LUA_GCCOUNTB, LUA_GCSTEP, LUA_GCSETPAUSE, LUA_GCSETSTEPMUL);
    // D's util/lua.d:82 aliases lua_KContext to int, so it prints 4 there and 8
    // here (luaconf.h:642 makes it ptrdiff_t). Documented difference; no D call
    // passes a non-zero ctx, so lua_pcall behaves the same in both.
    eil::writefln("sizeof lua_Number %d lua_Integer %d lua_KContext %d",
                  (int)sizeof(lua_Number), (int)sizeof(lua_Integer), (int)sizeof(lua_KContext));
    expect(sizeof(lua_Number) == 8, "lua_Number is 8 bytes");
    expect(sizeof(lua_Integer) == 8, "lua_Integer is 8 bytes");

    section(L, "script and scalar globals");
    runString(L, "gx = 2.5; gs = 'hello world'; gi = 7; gb = true; gstrnum = '12.75'");
    eil::writefln("top after script %d", lua_gettop(L));
    eil::writefln("getDouble(gx) %.17g", getDouble(L, "gx"));
    eil::writefln("getString(gs) |%s|", getString(L, "gs"));
    eil::writefln("getInt(gi) %d", getInt(L, "gi"));
    eil::writefln("getBool(gb) %s", getBool(L, "gb"));
    eil::writefln("getDouble(gstrnum) %.17g", getDouble(L, "gstrnum"));
    expect(getDouble(L, "gx") == 2.5, "getDouble on a number global");
    expect(getString(L, "gs") == "hello world", "getString on a string global");
    expect(getInt(L, "gi") == 7, "getInt on an integer global");
    expect(getBool(L, "gb") == true, "getBool on a boolean global");
    // Lua coerces strings to numbers in lua_isnumber/lua_tonumber, and numbers to
    // strings in lua_isstring/lua_tostring.
    tryGet("getString(gi):", [&] { auto v = getString(L, "gi"); eil::writefln("  value |%s|", v); });
    tryGet("getDouble(gs):", [&] { auto v = getDouble(L, "gs"); eil::writefln("  value %.17g", v); });
    tryGet("getInt(gx):", [&] { auto v = getInt(L, "gx"); eil::writefln("  value %d", v); });
    tryGet("getBool(gi):", [&] { auto v = getBool(L, "gi"); eil::writefln("  value %s", v); });
    tryGet("getString(missing global):", [&] { getString(L, "nope"); });
    tryGet("getDouble(missing global):", [&] { getDouble(L, "nope"); });
    tryGet("getInt(missing global):", [&] { getInt(L, "nope"); });
    tryGet("getBool(missing global):", [&] { getBool(L, "nope"); });
    eil::writefln("top after global getters %d", lua_gettop(L));

    section(L, "defaults");
    lua_pushglobaltable(L);
    eil::writefln("getStringWithDefault(gs) |%s|", getStringWithDefault(L, -1, "gs", "fallback"));
    eil::writefln("getStringWithDefault(missing) |%s|",
                  getStringWithDefault(L, -1, "nope", "fallback"));
    eil::writefln("getDoubleWithDefault(missing) %.17g",
                  getDoubleWithDefault(L, -1, "nope", -1.0));
    eil::writefln("getIntWithDefault(missing) %d", getIntWithDefault(L, -1, "nope", 42));
    eil::writefln("getBoolWithDefault(missing) %s", getBoolWithDefault(L, -1, "nope", true));
    tryGet("getStringWithDefault(non-string):", [&] {
        auto v = getStringWithDefault(L, -1, "gx", "dflt");
        eil::writefln("  value |%s|", v);
    });
    lua_pop(L, 1);
    eil::writefln("top after defaults %d", lua_gettop(L));
    // An index that holds something other than a table. Passing -1000 would make
    // Lua read L->top - 1000, which lapi.c:64 only guards with api_check (compiled
    // out), so the tests use a real index. These paths raise object.Error.
    lua_pushnumber(L, 3.5);
    tryGet("getStringWithDefault(non-table):", [&] {
        auto v = getStringWithDefault(L, -1, "gs", "fallback");
        eil::writefln("  value |%s|", v);
    });
    tryGet("getDoubleWithDefault(non-table):", [&] {
        auto v = getDoubleWithDefault(L, -1, "gs", 1.0);
        eil::writefln("  value %.17g", v);
    });
    tryGet("getIntWithDefault(non-table):", [&] {
        auto v = getIntWithDefault(L, -1, "gs", 1);
        eil::writefln("  value %d", v);
    });
    tryGet("getBoolWithDefault(non-table):", [&] {
        auto v = getBoolWithDefault(L, -1, "gs", true);
        eil::writefln("  value %s", v);
    });
    tryGet("getDouble(non-table):", [&] {
        auto v = getDouble(L, -1, "gs");
        eil::writefln("  value %.17g", v);
    });
    tryGet("getInt(non-table):", [&] {
        auto v = getInt(L, -1, "gs");
        eil::writefln("  value %d", v);
    });
    tryGet("getBool(non-table):", [&] {
        auto v = getBool(L, -1, "gs");
        eil::writefln("  value %s", v);
    });
    tryGet("getString(non-table):", [&] { auto v = getString(L, -1, "gs"); });
    tryGet("getArrayOfDoubles(non-table):", [&] { eil::Array<double> vv; getArrayOfDoubles(L, -1, "t", vv); });
    tryGet("getArrayOfInts(non-table):", [&] { eil::Array<int> vv; getArrayOfInts(L, -1, "t", vv); });
    tryGet("getArrayOfStrings(non-table):", [&] { eil::Array<std::string> vv; getArrayOfStrings(L, -1, "t", vv); });
    tryGet("getMatrixOfDoubles(non-table):", [&] {
        double mm[4][4]; getMatrixOfDoubles<4>(L, -1, mm);
    });
    lua_pop(L, 1);
    eil::writefln("top after non-table probes %d", lua_gettop(L));

    section(L, "table getters");
    runString(L, std::string("tb = {a = 1.5, b = 2.5, i = 3, s = 'str', t = {10.0, 20.0, 30.0}, ")
                   + "ints = {1, 2, 3}, strs = {'aa', 'bb'}, mixed = {1.0, 'x', 2.0}, "
                   + "empty = {}, assoc = {P = 0.25, Q = -0.5}}");
    lua_getglobal(L, "tb");
    eil::writefln("istable top %s", lua_istable(L, -1) ? "true" : "false");
    eil::writefln("getString(tb.b) |%s|", getString(L, -1, "b"));
    eil::writefln("getDouble(tb.a) %.17g", getDouble(L, -1, "a"));
    eil::writefln("getInt(tb.i) %d", getInt(L, -1, "i"));
    tryGet("getBool(tb.i):", [&] { auto v = getBool(L, -1, "i"); eil::writefln("  value %s", v); });
    tryGet("getString(tb.t):", [&] { auto v = getString(L, -1, "t"); eil::writefln("  value |%s|", v); });
    eil::writefln("getStringWithDefault(tb.z) |%s|", getStringWithDefault(L, -1, "z", "dflt"));
    eil::writefln("getDoubleWithDefault(tb.z) %.17g", getDoubleWithDefault(L, -1, "z", 9.0));
    eil::writefln("getIntWithDefault(tb.z) %d", getIntWithDefault(L, -1, "z", 9));
    eil::writefln("getBoolWithDefault(tb.z) %s", getBoolWithDefault(L, -1, "z", true));
    tryGet("getString(non-table idx):", [&] { getString(L, -2, "b"); });
    tryGet("getDouble(tb.missing):", [&] { getDouble(L, -1, "zz"); });
    tryGet("getInt(tb.missing):", [&] { getInt(L, -1, "zz"); });
    tryGet("getBool(tb.missing):", [&] { getBool(L, -1, "zz"); });
    eil::writefln("stack top after getters %d", lua_gettop(L));

    eil::Array<double> dv;
    eil::Array<int> iv;
    eil::Array<std::string> sv;
    getArrayOfDoubles(L, -1, "t", dv);
    eil::writefln("getArrayOfDoubles t %s n=%d", joinNums(dv), dv.length());
    getArrayOfDoubles(L, -1, "mixed", dv);
    eil::writefln("getArrayOfDoubles mixed %s n=%d", joinNums(dv), dv.length());
    dv = eil::Array<double>({1.0, 2.0});  // must be emptied by the callee even when it throws
    tryGet("getArrayOfDoubles(missing key):", [&] { getArrayOfDoubles(L, -1, "nope", dv); });
    eil::writefln("  dv length after throw %d", dv.length());
    tryGet("getArrayOfInts(missing key):", [&] { getArrayOfInts(L, -1, "nope", iv); });
    eil::writefln("  iv length after throw %d", iv.length());
    sv = eil::Array<std::string>({"z"});
    tryGet("getArrayOfStrings(missing key):", [&] { getArrayOfStrings(L, -1, "nope", sv); });
    eil::writefln("  sv length after throw %d", sv.length());
    getArrayOfInts(L, -1, "ints", iv);
    eil::writefln("getArrayOfInts ints %s n=%d", joinNums(iv), iv.length());
    getArrayOfInts(L, -1, "t", iv);
    eil::writefln("getArrayOfInts t(non-int) n=%d", iv.length());
    getArrayOfStrings(L, -1, "strs", sv);
    eil::writefln("getArrayOfStrings strs %s n=%d", joinStrs(sv), sv.length());
    getArrayOfStrings(L, -1, "t", sv);
    eil::writefln("getArrayOfStrings t(non-string) n=%d", sv.length());
    tryGet("getArrayOfDoubles(tb.s):", [&] { getArrayOfDoubles(L, -1, "s", dv); });
    tryGet("getArrayOfInts(tb.s):", [&] { getArrayOfInts(L, -1, "s", iv); });
    tryGet("getArrayOfStrings(tb.s):", [&] { getArrayOfStrings(L, -1, "s", sv); });
    eil::writefln("stack top after arrays %d", lua_gettop(L));
    lua_pop(L, 1);

    section(L, "assoc array and global arrays");
    eil::Array<std::string> pList({"P", "Q"});
    eil::AA<std::string, double> params;
    lua_getglobal(L, "tb");
    getAssocArrayOfDoubles(L, "assoc", pList.all(), params);
    eil::writefln("getAssoc P=%.17g Q=%.17g n=%d", params.get("P"), params.get("Q"), params.length());
    tryGet("getAssoc(missing):", [&] {
        eil::Array<std::string> bad({"NOPE"});
        eil::AA<std::string, double> p2;
        getAssocArrayOfDoubles(L, "NOPE", bad.all(), p2);
    });
    tryGet("getAssoc(not-a-table):", [&] {
        eil::Array<std::string> bad({"a"});
        eil::AA<std::string, double> p2;
        getAssocArrayOfDoubles(L, "s", bad.all(), p2);
    });
    lua_pop(L, 1);
    runString(L, "gnums = {1.5, 2.5}; gints = {4, 5}; gstrs = {'aa', 'bb', 'cc'}");
    lua_pushglobaltable(L);
    int gt = lua_gettop(L);
    getArrayOfDoubles(L, gt, "gnums", dv);
    eil::writefln("getArrayOfDoubles gnums via globals %s n=%d", joinNums(dv), dv.length());
    getArrayOfInts(L, gt, "gints", iv);
    eil::writefln("getArrayOfInts gints via globals %s n=%d", joinNums(iv), iv.length());
    getArrayOfDoubles(L, gt, "tb", dv);
    eil::writefln("getArrayOfDoubles tb(array of tables) n=%d", dv.length());
    tryGet("getArrayOfDoubles(missing in globals):", [&] { getArrayOfDoubles(L, gt, "nope", dv); });
    tryGet("getArrayOfInts(missing in globals):", [&] { getArrayOfInts(L, gt, "nope", iv); });
    getArrayOfStrings(L, "gstrs", sv);
    eil::writefln("getArrayOfStrings gstrs %s n=%d", joinStrs(sv), sv.length());
    sv = eil::Array<std::string>({"q"});
    tryGet("getArrayOfStrings(missing global):", [&] { getArrayOfStrings(L, "nope", sv); });
    eil::writefln("  sv length after throw %d", sv.length());
    lua_pop(L, 1);
    eil::writefln("top after assoc/global arrays %d", lua_gettop(L));

    section(L, "matrix roundtrip");
    double rm[4][4];
    runString(L, std::string("mat4 = {{0.0, 0.25, 0.5, 1.0}, {0.0, 1.0/3, 0.5, 1.0}, ")
                   + "{0.0, 0.25, 1.0/3, 1.0}, {0.0, 0.25, 0.5, 2.0}}");
    lua_getglobal(L, "mat4");
    getMatrixOfDoubles<4>(L, -1, rm);
    eil::writefln("row0 %.17g %.17g %.17g %.17g", rm[0][0], rm[0][1], rm[0][2], rm[0][3]);
    eil::writefln("row1 %.17g %.17g %.17g %.17g", rm[1][0], rm[1][1], rm[1][2], rm[1][3]);
    eil::writefln("row3 %.17g %.17g %.17g %.17g", rm[3][0], rm[3][1], rm[3][2], rm[3][3]);
    eil::writefln("stack top after get matrix %d", lua_gettop(L));
    pushMatrixOfDoubles<4>(L, rm);
    eil::writefln("stack top after push matrix %d", lua_gettop(L));
    lua_setglobal(L, "matback");
    lua_getglobal(L, "matback");
    lua_rawgeti(L, -1, 2);
    lua_rawgeti(L, -1, 3);
    eil::writefln("matback[2][3] %.17g", lua_tonumber(L, -1));
    lua_rawgeti(L, -2, 1);
    eil::writefln("matback[1] rows %d", (int)lua_objlen(L, -1));
    lua_pop(L, 2);
    lua_pop(L, 1);
    eil::writefln("top after matrix %d", lua_gettop(L));

    section(L, "tableEmpty / checkAllowedNames");
    lua_getglobal(L, "tb");
    lua_getfield(L, -1, "empty");
    eil::writefln("empty tableEmpty %s", tableEmpty(L, -1) ? "true" : "false");
    eil::writefln("  top after empty check %d", lua_gettop(L));
    lua_settop(L, 1);
    lua_pop(L, 1);
    lua_getglobal(L, "tb");
    lua_getfield(L, -1, "t");
    eil::writefln("non-empty tableEmpty %s", tableEmpty(L, -1) ? "true" : "false");
    eil::writefln("  top after non-empty check %d", lua_gettop(L));
    lua_settop(L, 0);
    lua_getglobal(L, "tb");
    // NOTE: checkAllowedNames does not adjust a negative index for the key it
    // pushes (tableEmpty does), so the table index must be absolute. Measured:
    // passing -1 crashes inside lua_next. Solver call sites pass positive index 1.
    eil::Array<std::string> allNames({"a", "b", "i", "s", "t", "ints", "strs", "mixed",
                                      "empty", "assoc"});
    eil::writefln("checkAllowedNames(all) %s",
                  checkAllowedNames(L, lua_gettop(L), allNames.all()) ? "true" : "false");
    eil::writefln("  top after all-names %d", lua_gettop(L));
    lua_settop(L, 0);
    // A table with exactly one disallowed key, so the key that gets reported does
    // not depend on Lua's traversal order (which follows short-string addresses).
    runString(L, "badtb = {a = 1, zz = 2}");
    lua_getglobal(L, "badtb");
    eil::Array<std::string> oneName({"a"});
    eil::writefln("checkAllowedNames(bad) %s",
                  checkAllowedNames(L, lua_gettop(L), oneName.all()) ? "true" : "false");
    eil::writefln("  top after bad-names %d", lua_gettop(L));
    lua_settop(L, 0);
    eil::writefln("top after names %d", lua_gettop(L));

    section(L, "userdata registration and dispatch");
    registerProbe(L);
    lua_getglobal(L, "ProbeObj");
    eil::writefln("ProbeObj is a table %s", lua_istable(L, -1) ? "true" : "false");
    eil::writefln("metatable name |%s|", getString(L, -1, "__name"));
    std::vector<std::string> mtNames;
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        mtNames.push_back(eil::util::detail::string_or_null(lua_tostring(L, -2)));
        lua_pop(L, 1);
    }
    std::sort(mtNames.begin(), mtNames.end());
    eil::writefln("metatable keys %s n=%d", joinStrsVec(mtNames), (int)mtNames.size());
    lua_pop(L, 1);
    eil::writefln("top before new %d", lua_gettop(L));
    runString(L, "p = ProbeObj:new{3.5, 'tagA'}");
    eil::writefln("stack after new %d", lua_gettop(L));
    runString(L, "pstr = tostring(p)");
    eil::writefln("tostring(p) |%s|", getString(L, "pstr"));
    runString(L, "pdoubled = p:doubled()");
    eil::writefln("p:doubled() %.17g", getDouble(L, "pdoubled"));
    runString(L, "ptype = p:typeName()");
    eil::writefln("p:typeName() |%s|", getString(L, "ptype"));
    runString(L, "numType = ProbeObj.typeName(1.25)");
    eil::writefln("typeName(1.25) |%s|", getString(L, "numType"));
    runString(L, "strType = ProbeObj.typeName('ab')");
    eil::writefln("typeName('ab') |%s|", getString(L, "strType"));
    runString(L, "nilType = ProbeObj.typeName(nil)");
    eil::writefln("typeName(nil) |%s|", getString(L, "nilType"));
    runString(L, "otherType = ProbeObj.typeName({})");
    eil::writefln("typeName({}) |%s|", getString(L, "otherType"));
    runProtected(L, "ProbeObj.badNil(nil)");
    runProtected(L, "p.v");
    runProtected(L, "ProbeObj:new{1.0, 'x'}");
    eil::writefln("probeStore length %d", (int)probeStore.length());
    eil::writefln("top after userdata %d", lua_gettop(L));

    section(L, "stack discipline and warnOnStackChange");
    int top_before = lua_gettop(L);
    lua_pushcfunction(L, &stackGrower);
    lua_setglobal(L, "stackGrower");
    runString(L, "stackGrower()");
    eil::writefln("top after grower call %d (was %d)", lua_gettop(L), top_before);
    EIL_WARN_ON_STACK_CHANGE(L, top_before + 1);  // deliberately mismatched
    EIL_WARN_ON_STACK_CHANGE(L, lua_gettop(L));   // matched: prints nothing
    EIL_WARN_ON_STACK_CHANGE(L, 999);             // mismatched the other way
    lua_settop(L, 0);

    section(L, "getNumberFromTable family, from inside Lua");
    runString(L, std::string("cfg = {num = 4.5, int = 6, flag = false, str = 'x', strnum = '3.5', ")
                   + "truthy = true}");
    lua_pushcfunction(L, &cfgGood);
    lua_setglobal(L, "cfgGood");
    runString(L, "res = {cfgGood(cfg)}");
    lua_getglobal(L, "res");
    eil::writefln("res is a table %s n=%d", lua_istable(L, -1) ? "true" : "false",
                  (int)lua_objlen(L, -1));
    for (int k = 1; k <= 16; ++k) {
        lua_rawgeti(L, -1, k);
        int ty = lua_type(L, -1);
        double x = lua_tonumber(L, -1);
        if (lua_isnumber(L, -1)) {
            eil::writefln("res[%d] number type=%d int=%s isNaN=%s finite=%s text=%s raw=%.17g", k,
                          ty, lua_isinteger(L, -1) != 0, std::isnan(x), std::isfinite(x),
                          eil::to_string(x), x);
        } else if (ty == LUA_TBOOLEAN) {
            eil::writefln("res[%d] bool %s", k, lua_toboolean(L, -1) != 0);
        } else {
            eil::writefln("res[%d] type %d", k, ty);
        }
        lua_pop(L, 1);
    }
    eil::writefln("stack top after res walk %d", lua_gettop(L));
    lua_pop(L, 1);
    lua_pushcfunction(L, &cfgErrNumberNotValid);
    lua_setglobal(L, "cfgErrNumberNotValid");
    lua_pushcfunction(L, &cfgErrNumberNotFound);
    lua_setglobal(L, "cfgErrNumberNotFound");
    lua_pushcfunction(L, &cfgErrIntNotValid);
    lua_setglobal(L, "cfgErrIntNotValid");
    lua_pushcfunction(L, &cfgErrBoolNotValid);
    lua_setglobal(L, "cfgErrBoolNotValid");
    runProtected(L, "cfgErrNumberNotValid(cfg)");
    runProtected(L, "cfgErrNumberNotFound(cfg)");
    runProtected(L, "cfgErrIntNotValid(cfg)");
    runProtected(L, "cfgErrBoolNotValid(cfg)");

    section(L, "doLuaFile");
    write_child_files();
    tryGet("missing file:", [&] { doLuaFile(L, "no_such_file.lua"); });
    tryGet("runtime error file:", [&] { doLuaFile(L, "/tmp/probe_child.lua"); });
    tryGet("syntax error file:", [&] { doLuaFile(L, "/tmp/probe_syntax.lua"); });
    doLuaFile(L, "/tmp/probe_ok.lua");
    eil::writefln("loaded value %.17g", getDouble(L, "loaded_ok"));
    eil::writefln("top after doLuaFile %d", lua_gettop(L));

    section(L, "msg_service");
    // Explicit location arguments so the C++ port can print the same bytes.
    eil::writefln("pre |%s|", brokenPreCondition("varA", 111, "some/file.d"));
    eil::writefln("post |%s|", brokenPostCondition("varB", 222, "some/file.d"));
    eil::writefln("unit |%s|", failedUnitTest(333, "some/file.d"));
    // Defaults: D substitutes the *caller's* file and line, so the macros do too.
    eil::writefln("pre-default >>>%s<<<", EIL_BROKEN_PRE_CONDITION("varC"));
    eil::writefln("post-default >>>%s<<<", EIL_BROKEN_POST_CONDITION("varD"));
    eil::writefln("unit-default >>>%s<<<", EIL_FAILED_UNIT_TEST());

    section(L, "time_utils");
    eil::writefln("time 10 %d", timeStringToSeconds("10"));
    eil::writefln("time 1:2:3 %d", timeStringToSeconds("1:2:3"));
    eil::writefln("time 0:0:0 %d", timeStringToSeconds("0:0:0"));
    eil::writefln("time 23:59:59 %d", timeStringToSeconds("23:59:59"));
    std::vector<std::string> bad({"1:30", "abc", "1:2:3:4", "", " 7", "7 ", "1.5", "-5", "+7",
                                  "0x10", "2147483648", "-", "07", "1e3", "2:3:4", "0",
                                  "x:2:3", "1:y:3", "1:2:z", "5:", "1:2:", "2147483647"});
    for (const std::string& b : bad) {
        try {
            eil::writefln("time(|%s|) %d", b, timeStringToSeconds(b));
        } catch (const eil::ErrorBase& e) {
            eil::writefln("time(|%s|) Error |%s|", b, e.msg());
        } catch (const eil::ExceptionBase& e) {
            eil::writefln("time(|%s|) Exception |%s|", b, e.msg());
        }
    }

    section(L, "lua API details");
    lua_newtable(L);
    lua_pushstring(L, "k1"); lua_rawseti(L, -2, 1);
    eil::writefln("rawlen %d objlen %d", (int)lua_rawlen(L, -1), (int)lua_objlen(L, -1));
    lua_pushvalue(L, -1);
    eil::writefln("rawequal %s", lua_rawequal(L, -1, -2) ? "true" : "false");
    lua_pop(L, 1);
    lua_pushinteger(L, static_cast<lua_Integer>(-3));
    eil::writefln("isinteger %d isnumber %d isstring %d tointeger %d tonumber %.17g",
                  (int)(lua_isinteger(L, -1) != 0), (int)(lua_isnumber(L, -1) != 0),
                  (int)(lua_isstring(L, -1) != 0), (int)lua_tointeger(L, -1), lua_tonumber(L, -1));
    lua_pop(L, 1);
    lua_pushnumber(L, 1.0 / 3.0);
    eil::writefln("isinteger of 1/3 %d isnumber %d isstring %d tointeger %d",
                  (int)(lua_isinteger(L, -1) != 0), (int)(lua_isnumber(L, -1) != 0),
                  (int)(lua_isstring(L, -1) != 0), (int)lua_tointeger(L, -1));
    lua_pop(L, 1);
    eil::writefln("gccount %d", lua_gc(L, LUA_GCCOUNT, 0));
    lua_pushglobaltable(L);
    eil::writefln("global table is table %s", lua_istable(L, -1) ? "true" : "false");
    lua_pop(L, 1);
    lua_newuserdata(L, 8);
    // The lua_is* names that lua.h spells as macros yield bool in C++ and int in
    // C, and eil::format renders a bool under %d as "true"/"false" where Phobos
    // prints 1/0 (measured). Cast, so the printed digits are D's.
    eil::writefln("isuserdata %d type %d isnil %d isboolean %d isfunction %d",
                  (int)(lua_isuserdata(L, -1) != 0), lua_type(L, -1),
                  (int)(lua_isnil(L, -1) != 0), (int)(lua_isboolean(L, -1) != 0),
                  (int)(lua_isfunction(L, -1) != 0));
    lua_pop(L, 1);
    lua_pushnil(L);
    eil::writefln("nil: type %d isnil %d isstring %d tostring-null %s", lua_type(L, -1),
                  (int)(lua_isnil(L, -1) != 0), (int)(lua_isstring(L, -1) != 0),
                  lua_tostring(L, -1) == nullptr ? "true" : "false");
    lua_pop(L, 1);
    lua_pushboolean(L, 1);
    eil::writefln("boolean toboolean %d", (int)(lua_toboolean(L, -1) != 0));
    lua_pop(L, 1);
    lua_pushstring(L, "abc");
    size_t slen;
    const char* sbuf = lua_tolstring(L, -1, &slen);
    eil::writefln("tolstring |%s| len %d tostring |%s|", eil::util::detail::string_or_null(sbuf), slen,
                  eil::util::detail::string_or_null(lua_tostring(L, -1)));
    lua_pop(L, 1);
    runString(L, "function two() return 11, 22 end");
    lua_getglobal(L, "two");
    if (lua_pcall(L, 0, LUA_MULTRET, 0) != 0) {
        eil::writefln("pcall error |%s|", eil::util::detail::string_or_null(lua_tostring(L, -1)));
        lua_pop(L, 1);
    } else {
        eil::writefln("pcall top %d first %d second %d", lua_gettop(L),
                      (int)lua_tointeger(L, -2), (int)lua_tointeger(L, -1));
        lua_pop(L, 2);
    }
    eil::writefln("final top %d", lua_gettop(L));
    lua_close(L);
    for (Probe* p : probeStore) delete p;  // D leaves these to the GC; the port owns them
    probeStore = eil::Array<Probe*>();

    std::printf("%s: lua roundtrip (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    std::fflush(stdout);
    return failures ? 1 : 0;
}
