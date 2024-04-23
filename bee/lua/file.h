#pragma once

#include <lua.hpp>

namespace bee::lua {
    int newfile(lua_State* L, FILE* f);
    luaL_Stream* tofile(lua_State* L, int idx);
}
