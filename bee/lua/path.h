#pragma once

#include <lua.hpp>
#include <optional>
#include <bee/lua/binding.h>

#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#define ENABLE_FILESYSTEM
#endif

namespace bee::lua {
    inline bool luaL_ismetaname(lua_State* L, int idx, const char* name) {
        switch (luaL_getmetafield(L, idx, "__name")) {
        case LUA_TNIL:
            return false;
        default:
            lua_pop(L, 1);
            return false;
        case LUA_TSTRING:
            break;
        }
        bool res = (strcmp(name, lua_tostring(L, idx)) == 0);
        lua_pop(L, 1);
        return res;
    }

    inline std::optional<lua::string_type> get_path(lua_State* L, int idx) {
        switch (lua_type(L, idx)) {
        case LUA_TSTRING:
            return lua::string_type(lua::to_string(L, idx));
#ifdef ENABLE_FILESYSTEM
        case LUA_TUSERDATA: {
            const fs::path& path = *(fs::path*)getObject(L, idx, "filesystem");
            return path.string<lua::string_type::value_type>();
        }
#else
        case LUA_TTABLE:
            if (luaL_ismetaname(L, idx, "filesystem")) {
                if (luaL_callmeta(L, idx, "__tostring") && lua_type(L, -1) == LUA_TSTRING) {
                    auto res = lua::string_type(lua::to_string(L, idx));
                    lua_pop(L, 1);
                    return res;
                }
                lua_pop(L, 1);
            }
            [[fallthrough]];
#endif
        case LUA_TNIL:
        default:
            return std::optional<lua::string_type>();
        }
    }
}
