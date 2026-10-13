// util/lua_service.d -> src/util/lua_service.cpp
//
// The non-template half of the module. Bodies follow the D file line by line;
// the D line each one comes from is noted above it.

#include "util/lua_service.hpp"

namespace eil {
namespace util {

// util/lua_service.d:20-25
lua_State* init_lua_State() {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    return L;
}

// util/lua_service.d:27-33
void doLuaFile(lua_State* L, const std::string& fname) {
    if (luaL_dofile(L, fname.c_str()) != 0) {
        std::string errMsg = detail::string_or_null(lua_tostring(L, -1));
        EIL_THROW_LUA_INPUT(errMsg);
    }
}

// util/lua_service.d:35-42
void warnOnStackChange(lua_State* L, int old_top, const char* file, size_t line) {
    int new_top = lua_gettop(L);
    if (new_top - old_top != 0) {
        eil::writefln("WARNING unexpected stack size change old_top=%d new_top=%d at line %d in file %s",
                      old_top, new_top, line, file);
    }
}

// util/lua_service.d:44-56
bool tableEmpty(lua_State* L, int tblIdx) {
    lua_pushnil(L);  // first key
    // If we are looking from the top of the stack,
    // the table is now one further down.
    if (tblIdx < 0) { tblIdx -= 1; }
    if (lua_next(L, tblIdx) != 0) {
        lua_pop(L, 2);  // discard key and value
        return false;
    } else {
        return true;
    }
}

// util/lua_service.d:58-75
std::string getString(lua_State* L, int tblIdx, const std::string& key) {
    int old_top = lua_gettop(L);
    if (!lua_istable(L, tblIdx)) {
        std::string errMsg =
            eil::format("getString was expecting a table at stack index: %d", tblIdx);
        EIL_THROW_UTIL_ERROR(errMsg);
    }
    lua_getfield(L, tblIdx, key.c_str());
    if (!lua_isstring(L, -1)) {
        std::string errMsg = eil::format("A string was expected in field: %s", key);
        lua_pop(L, 1);
        EIL_THROW_LUA_INPUT(errMsg);
    }
    std::string val = detail::string_or_null(lua_tostring(L, -1));
    lua_pop(L, 1);
    warnOnStackChange(L, old_top, __FILE__, __LINE__);
    return val;
}

// util/lua_service.d:77-90
std::string getString(lua_State* L, const std::string& key) {
    int old_top = lua_gettop(L);
    lua_getglobal(L, key.c_str());
    if (!lua_isstring(L, -1)) {
        std::string errMsg = eil::format("A string was expected in field: %s", key);
        lua_pop(L, 1);
        EIL_THROW_LUA_INPUT(errMsg);
    }
    std::string val = detail::string_or_null(lua_tostring(L, -1));
    lua_pop(L, 1);
    warnOnStackChange(L, old_top, __FILE__, __LINE__);
    return val;
}

// util/lua_service.d:92-107
std::string getStringWithDefault(lua_State* L, int tblIdx, const std::string& key,
                                const std::string& defaultValue) {
    int old_top = lua_gettop(L);
    std::string val = defaultValue;
    if (!lua_istable(L, tblIdx)) {
        std::string errMsg =
            eil::format("getStringWithDefault was expecting a table at stack index: %d", tblIdx);
        EIL_THROW_UTIL_ERROR(errMsg);
    }
    lua_getfield(L, tblIdx, key.c_str());
    if (lua_isstring(L, -1)) { val = detail::string_or_null(lua_tostring(L, -1)); }
    lua_pop(L, 1);
    warnOnStackChange(L, old_top, __FILE__, __LINE__);
    return val;
}

// util/lua_service.d:109-126
double getDouble(lua_State* L, int tblIdx, const std::string& key) {
    int old_top = lua_gettop(L);
    if (!lua_istable(L, tblIdx)) {
        std::string errMsg =
            eil::format("getDouble was expecting a table at stack index: %d", tblIdx);
        EIL_THROW_UTIL_ERROR(errMsg);
    }
    lua_getfield(L, tblIdx, key.c_str());
    if (!lua_isnumber(L, -1)) {
        std::string errMsg = eil::format("A double was expected in field: %s", key);
        lua_pop(L, 1);
        EIL_THROW_UTIL_ERROR(errMsg);
    }
    double val = lua_tonumber(L, -1);
    lua_pop(L, 1);
    warnOnStackChange(L, old_top, __FILE__, __LINE__);
    return val;
}

// util/lua_service.d:128-141
double getDouble(lua_State* L, const std::string& key) {
    int old_top = lua_gettop(L);
    lua_getglobal(L, key.c_str());
    if (!lua_isnumber(L, -1)) {
        std::string errMsg = eil::format("A double was expected in field: %s", key);
        lua_pop(L, 1);
        EIL_THROW_UTIL_ERROR(errMsg);
    }
    double val = lua_tonumber(L, -1);
    lua_pop(L, 1);
    warnOnStackChange(L, old_top, __FILE__, __LINE__);
    return val;
}

// util/lua_service.d:143-158
double getDoubleWithDefault(lua_State* L, int tblIdx, const std::string& key,
                            double defaultValue) {
    int old_top = lua_gettop(L);
    if (!lua_istable(L, tblIdx)) {
        std::string errMsg =
            eil::format("getDoubleWithDefault was expecting a table at stack index: %d", tblIdx);
        EIL_THROW_UTIL_ERROR(errMsg);
    }
    double val = defaultValue;
    lua_getfield(L, tblIdx, key.c_str());
    if (lua_isnumber(L, -1)) { val = lua_tonumber(L, -1); }
    lua_pop(L, 1);
    warnOnStackChange(L, old_top, __FILE__, __LINE__);
    return val;
}

// util/lua_service.d:160-177
int getInt(lua_State* L, int tblIdx, const std::string& key) {
    int old_top = lua_gettop(L);
    if (!lua_istable(L, tblIdx)) {
        std::string errMsg =
            eil::format("getInt was expecting a table at stack index: %d", tblIdx);
        EIL_THROW_UTIL_ERROR(errMsg);
    }
    lua_getfield(L, tblIdx, key.c_str());
    if (!lua_isinteger(L, -1)) {
        std::string errMsg = eil::format("An integer was expected in field: %s", key);
        lua_pop(L, 1);
        EIL_THROW_UTIL_ERROR(errMsg);
    }
    int val = detail::to_int_from_long(lua_tointeger(L, -1));
    lua_pop(L, 1);
    warnOnStackChange(L, old_top, __FILE__, __LINE__);
    return val;
}

// util/lua_service.d:179-192
int getInt(lua_State* L, const std::string& key) {
    int old_top = lua_gettop(L);
    lua_getglobal(L, key.c_str());
    if (!lua_isinteger(L, -1)) {
        std::string errMsg = eil::format("An integer was expected in field: %s", key);
        lua_pop(L, 1);
        EIL_THROW_UTIL_ERROR(errMsg);
    }
    int val = detail::to_int_from_long(lua_tointeger(L, -1));
    lua_pop(L, 1);
    warnOnStackChange(L, old_top, __FILE__, __LINE__);
    return val;
}

// util/lua_service.d:194-209
int getIntWithDefault(lua_State* L, int tblIdx, const std::string& key, int defaultValue) {
    int old_top = lua_gettop(L);
    if (!lua_istable(L, tblIdx)) {
        std::string errMsg =
            eil::format("getIntWithDefault was expecting a table at stack index: %d", tblIdx);
        EIL_THROW_UTIL_ERROR(errMsg);
    }
    int val = defaultValue;
    lua_getfield(L, tblIdx, key.c_str());
    if (lua_isinteger(L, -1)) { val = detail::to_int_from_long(lua_tointeger(L, -1)); }
    lua_pop(L, 1);
    warnOnStackChange(L, old_top, __FILE__, __LINE__);
    return val;
}

// util/lua_service.d:211-228
bool getBool(lua_State* L, int tblIdx, const std::string& key) {
    int old_top = lua_gettop(L);
    if (!lua_istable(L, tblIdx)) {
        std::string errMsg =
            eil::format("getBool was expecting a table at stack index: %d", tblIdx);
        EIL_THROW_UTIL_ERROR(errMsg);
    }
    lua_getfield(L, tblIdx, key.c_str());
    if (!lua_isboolean(L, -1)) {
        std::string errMsg = eil::format("A boolean value was expected in field: %s", key);
        lua_pop(L, 1);
        EIL_THROW_UTIL_ERROR(errMsg);
    }
    bool val = lua_toboolean(L, -1) != 0;  // D: to!bool(lua_toboolean(...))
    lua_pop(L, 1);
    warnOnStackChange(L, old_top, __FILE__, __LINE__);
    return val;
}

// util/lua_service.d:230-243
bool getBool(lua_State* L, const std::string& key) {
    int old_top = lua_gettop(L);
    lua_getglobal(L, key.c_str());
    if (!lua_isboolean(L, -1)) {
        std::string errMsg = eil::format("A boolean value was expected in field: %s", key);
        lua_pop(L, 1);
        EIL_THROW_UTIL_ERROR(errMsg);
    }
    bool val = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    warnOnStackChange(L, old_top, __FILE__, __LINE__);
    return val;
}

// util/lua_service.d:245-260
bool getBoolWithDefault(lua_State* L, int tblIdx, const std::string& key, bool defaultValue) {
    int old_top = lua_gettop(L);
    if (!lua_istable(L, tblIdx)) {
        std::string errMsg =
            eil::format("getBoolWithDefault was expecting a table at stack index: %d", tblIdx);
        EIL_THROW_UTIL_ERROR(errMsg);
    }
    bool val = defaultValue;
    lua_getfield(L, tblIdx, key.c_str());
    if (lua_isboolean(L, -1)) { val = lua_toboolean(L, -1) != 0; }
    lua_pop(L, 1);
    warnOnStackChange(L, old_top, __FILE__, __LINE__);
    return val;
}

// util/lua_service.d:262-285
void getArrayOfStrings(lua_State* L, int tblIdx, const std::string& key,
                       eil::Array<std::string>& values) {
    int old_top = lua_gettop(L);
    if (!lua_istable(L, tblIdx)) {
        std::string errMsg =
            eil::format("getArrayOfStrings was expecting a table at stack index: %d", tblIdx);
        EIL_THROW_UTIL_ERROR(errMsg);
    }
    values.set_length(0);
    lua_getfield(L, tblIdx, key.c_str());
    if (!lua_istable(L, -1)) {
        std::string errMsg = eil::format("A table of strings was expected in field: %s", key);
        lua_pop(L, 1);
        EIL_THROW_UTIL_ERROR(errMsg);
    }
    auto n = detail::to_int_from_long(static_cast<long long>(lua_objlen(L, -1)));
    for (int i = 1; i < n + 1; ++i) {
        lua_rawgeti(L, -1, i);
        if (lua_isstring(L, -1)) values.append(detail::string_or_null(lua_tostring(L, -1)));
        // Silently ignore anything that isn't a string value.
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    warnOnStackChange(L, old_top, __FILE__, __LINE__);
}

// util/lua_service.d:287-306
void getArrayOfStrings(lua_State* L, const std::string& key, eil::Array<std::string>& values) {
    int old_top = lua_gettop(L);
    values.set_length(0);
    lua_getglobal(L, key.c_str());
    if (!lua_istable(L, -1)) {
        std::string errMsg = eil::format("A table of strings was expected in field: %s", key);
        lua_pop(L, 1);
        EIL_THROW_UTIL_ERROR(errMsg);
    }
    auto n = detail::to_int_from_long(static_cast<long long>(lua_objlen(L, -1)));
    for (int i = 1; i < n + 1; ++i) {
        lua_rawgeti(L, -1, i);
        if (lua_isstring(L, -1)) values.append(detail::string_or_null(lua_tostring(L, -1)));
        // Silently ignore anything that isn't a string value.
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    warnOnStackChange(L, old_top, __FILE__, __LINE__);
}

// util/lua_service.d:345-368
void getArrayOfInts(lua_State* L, int tblIdx, const std::string& key, eil::Array<int>& values) {
    int old_top = lua_gettop(L);
    if (!lua_istable(L, tblIdx)) {
        std::string errMsg =
            eil::format("getArrayOfInts was expecting a table at stack index: %d", tblIdx);
        EIL_THROW_UTIL_ERROR(errMsg);
    }
    values.set_length(0);
    lua_getfield(L, tblIdx, key.c_str());
    if (!lua_istable(L, -1)) {
        std::string errMsg = eil::format("A table of integers was expected in field: %s", key);
        lua_pop(L, 1);
        EIL_THROW_UTIL_ERROR(errMsg);
    }
    auto n = detail::to_int_from_long(static_cast<long long>(lua_objlen(L, -1)));
    for (int i = 1; i < n + 1; ++i) {
        lua_rawgeti(L, -1, i);
        if (lua_isinteger(L, -1)) values.append(detail::to_int_from_long(lua_tointeger(L, -1)));
        // Silently ignore anything that isn't a value.
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    warnOnStackChange(L, old_top, __FILE__, __LINE__);
}

// util/lua_service.d:492-522
bool isObjType(lua_State* L, int index, const std::string& tname) {
    bool result;
    if (tname == "number") {
        if (lua_isnumber(L, index)) {
            return true;
        } else {
            return false;
        }
    }
    if (tname == "string") {
        if (lua_isstring(L, index)) {
            return true;
        } else {
            return false;
        }
    }
    void* p = lua_touserdata(L, index);
    if (p) {  // value is a userdata?
        if (lua_getmetatable(L, index)) {  // does it have a metatable?
            luaL_getmetatable(L, tname.c_str());  // get correct metatable
            if (lua_rawequal(L, -1, -2))          // the same?
                result = true;
            else
                result = false;
            lua_pop(L, 2);  // remove both metatables
            return result;
        }
    }
    return false;  // value is not a userdata with a metatable
}

// util/lua_service.d:556-587
double getNumberFromTable(lua_State* L, int index, const std::string& field,
                          bool errorIfNotFound, double valIfError, bool errorIfNotValid,
                          const std::string& errMsg) {
    int old_top = lua_gettop(L);
    lua_getfield(L, index, field.c_str());
    if (lua_isnil(L, -1)) {
        if (errorIfNotFound) {
            luaL_error(L, errMsg.c_str());
        } else {  // We didn't really care
            lua_pop(L, 1);
            warnOnStackChange(L, old_top, __FILE__, __LINE__);
            return valIfError;
        }
    }
    // Presumably then we have something to look at.
    if (lua_isnumber(L, -1)) {
        auto val = lua_tonumber(L, -1);
        lua_pop(L, 1);
        warnOnStackChange(L, old_top, __FILE__, __LINE__);
        return val;
    }
    // else, failed to find a number value.
    if (errorIfNotValid) {
        luaL_error(L, errMsg.c_str());
    }
    // We didn't want to fail, so give back double.init
    lua_pop(L, 1);
    warnOnStackChange(L, old_top, __FILE__, __LINE__);
    return valIfError;
}  // end getNumberFromTable()

// util/lua_service.d:591-621
int getIntegerFromTable(lua_State* L, int index, const std::string& field,
                        bool errorIfNotFound, int valIfError, bool errorIfNotValid,
                        const std::string& errMsg) {
    int old_top = lua_gettop(L);
    lua_getfield(L, index, field.c_str());
    if (lua_isnil(L, -1)) {
        if (errorIfNotFound) {
            luaL_error(L, errMsg.c_str());
        } else {  // We didn't really care
            lua_pop(L, 1);
            warnOnStackChange(L, old_top, __FILE__, __LINE__);
            return valIfError;
        }
    }
    // Presumably then we have something to look at.
    if (lua_isnumber(L, -1)) {
        auto val = detail::to_int_from_double(lua_tonumber(L, -1));
        lua_pop(L, 1);
        warnOnStackChange(L, old_top, __FILE__, __LINE__);
        return val;
    }
    // else, failed to find an integer value.
    if (errorIfNotValid) {
        luaL_error(L, errMsg.c_str());
    }
    // We didn't want to fail, so give back int.init
    warnOnStackChange(L, old_top, __FILE__, __LINE__);
    return valIfError;
}  // end getIntegerFromTable()

// util/lua_service.d:623-653
bool getBooleanFromTable(lua_State* L, int index, const std::string& field,
                         bool errorIfNotFound, bool valIfError, bool errorIfNotValid,
                         const std::string& errMsg) {
    int old_top = lua_gettop(L);
    lua_getfield(L, index, field.c_str());
    if (lua_isnil(L, -1)) {
        if (errorIfNotFound) {
            luaL_error(L, errMsg.c_str());
        } else {  // We didn't really care
            lua_pop(L, 1);
            warnOnStackChange(L, old_top, __FILE__, __LINE__);
            return valIfError;
        }
    }
    // Presumably then we have something to look at.
    if (lua_isboolean(L, -1)) {
        auto val = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
        warnOnStackChange(L, old_top, __FILE__, __LINE__);
        return val;
    }
    // else, failed to find a boolean value.
    if (errorIfNotValid) {
        luaL_error(L, errMsg.c_str());
    }
    // We didn't want to fail, so give back bool.init
    warnOnStackChange(L, old_top, __FILE__, __LINE__);
    return valIfError;
}  // end getBooleanFromTable()

// util/lua_service.d:655-676
bool checkAllowedNames(lua_State* L, int tblIndx, eil::Slice<std::string> allowedNames) {
    int old_top = lua_gettop(L);
    bool namesOk = true;
    // Iterate through the table and check each key (name).
    lua_pushnil(L);  // first key
    while (lua_next(L, tblIndx) != 0) {
        // next key is left on stack at -2, value at -1
        std::string key = detail::string_or_null(lua_tostring(L, -2));
        bool found = false;
        for (size_t i = 0; i < allowedNames.length(); ++i) {
            if (allowedNames[i] == key) { found = true; break; }
        }
        if (!found) {
            eil::writeln("checkAllowedNames found invalid key: ", key);
            namesOk = false;
            // We stop on the first invalid key because continuing on may lead
            // to a failure message for next, and loss of the location information.
            // The resulting error message is really confusing to the user.
            break;
        }
        lua_pop(L, 1);  // discard value but keep key for next
    }  // end while
    warnOnStackChange(L, old_top, __FILE__, __LINE__);
    return namesOk;
}  // end checkAllowedNames()

}  // namespace util
}  // namespace eil
