#include <lua.hpp>
#include <bee/lua/binding.h>
#include <bee/nonstd/embed.h>
#if !defined(_MSC_VER)
define_embed(bee, "script/bee.lua");
#endif

static int do_span(lua_State* L, const char* name, const nonstd::span<const std::byte>& span) {
    if (luaL_loadbuffer(L, (const char*)span.data(), span.size() - 1, name) != LUA_OK) {
        return lua_error(L);
    }
    lua_call(L, 0, 1);
    return 1;
}

BEE_LUA_API
int luaopen_bee(lua_State* L) {
    return do_span(L, "=module 'bee'", nonstd::embed(bee, "script/bee.lua"));
}

#if defined(__APPLE__) || defined(__MINGW32__)
#include <bee/nonstd/embed.h>
define_embed(filesystem, "script/filesystem.lua");

BEE_LUA_API
int luaopen_bee_filesystem(lua_State* L) {
    return do_span(L, "=module 'bee.filesystem'", nonstd::embed(filesystem, "script/filesystem.lua"));
}

#endif
