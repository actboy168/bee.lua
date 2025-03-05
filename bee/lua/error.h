#pragma once

#include <lua.hpp>
#include <string_view>
#include <system_error>

namespace bee::lua {
    void push_sys_error(lua_State* L, std::string_view msg, int err);
    void push_sys_error(lua_State* L, std::string_view msg);
    void push_net_error(lua_State* L, std::string_view msg);

    int return_error(lua_State* L, std::string_view msg);
    int return_crt_error(lua_State* L, std::string_view msg);
    int return_sys_error(lua_State* L, std::string_view msg);
    int return_net_error(lua_State* L, std::string_view msg, int err);
    int return_net_error(lua_State* L, std::string_view msg);
}
