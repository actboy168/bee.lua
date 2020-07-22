#include <bee/lua/binding.h>
#include <lua.hpp>

extern "C" {
#include <lua-seri.h>
}

namespace bee::lua_serialization {
    static int unpack(lua_State* L) {
        switch (lua_type(L, 1)) {
        case LUA_TLIGHTUSERDATA:
            return seri_unpackptr(L, lua_touserdata(L, 1));
        case LUA_TSTRING: {
            return seri_unpack(L);
        }
        }
        return 0;
    }
    static int pack(lua_State* L) {
        void* data = seri_pack(L, 0, NULL);
        lua_pushlightuserdata(L, data);
        return 1;
    }
    static int packstring(lua_State* L) {
        int sz;
        void* data = seri_pack(L, 0, &sz);
        lua_pushlstring(L, (const char*)data, sz);
        free(data);
        return 1;
    }
    static int lightuserdata(lua_State* L) {
        switch (lua_type(L, 1)) {
        case LUA_TLIGHTUSERDATA:
            lua_pushinteger(L, (intptr_t)lua_touserdata(L, 1));
            return 1;
        case LUA_TNUMBER:
            lua_pushlightuserdata(L, (void*)(intptr_t)luaL_checkinteger(L, 1));
            return 1;
        default:
            luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
            return 0;
        }
    }
    static int luaopen(lua_State* L) {
        luaL_Reg lib[] = {
            {"unpack", unpack},
            {"pack", pack},
            {"packstring", packstring},
            {"lightuserdata", lightuserdata},
            {NULL, NULL}};
        luaL_newlib(L, lib);
        return 1;
    }
}

DEFINE_LUAOPEN(serialization)
