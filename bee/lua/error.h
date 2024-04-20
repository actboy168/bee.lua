#pragma once

#include <lua.hpp>
#include <string_view>

namespace bee::lua {
    inline int push_error(lua_State* L, std::string_view error) {
        lua_pushnil(L);
        lua_pushlstring(L, error.data(), error.size());
        return 2;
    }
}
