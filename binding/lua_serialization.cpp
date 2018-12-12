#include <lua.hpp>      
#include <bee/lua/binding.h>

extern "C" {
#include <lua-seri.h>
}

namespace bee::lua_serialization {
    static int unpack(lua_State* L) {
        switch (lua_type(L, 1)) {
        case LUA_TLIGHTUSERDATA:
            return seri_unpack(L, lua_touserdata(L, 1));
        case LUA_TSTRING: {
            int n = lua_gettop(L);
            return seri_unpackbuffer(L, (void*)lua_tostring(L, 1)) - n;
        }
        }
        return 0;
    }
    static int pack(lua_State* L) {
        void* data = seri_pack(L, 0);
        lua_pushlightuserdata(L, data);
        return 1;
    }
    static int packstring(lua_State* L) {
        void* data = seri_pack(L, 0);
        lua_pushlstring(L, (const char*)data, 4 + *(int32_t*)data);
        free(data);
        return 1;
    }
    int luaopen(lua_State* L) {
        luaL_Reg lib[] = {
            { "unpack",     unpack },
            { "pack",       pack },
            { "packstring", packstring },
            { NULL, NULL }
        };
        luaL_newlib(L, lib);
        return 1;
    }
}

DEFINE_LUAOPEN(serialization)
