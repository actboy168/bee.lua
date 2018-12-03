#include <lua.hpp>
#include <bee/nonstd/embed.h>
#include <bee/lua/binding.h>

static int do_span(lua_State* L, const nonstd::span<const std::byte>& span) {
	if (luaL_loadbuffer(L, (const char*)span.data(), span.size() - 1, (const char*)span.data()) != LUA_OK) {
		return lua_error(L);
	}
	lua_call(L, 0, 1);
	return 1;
}

BEE_LUA_API
int luaopen_bee(lua_State* L) {
	return do_span(L, nonstd::embed(bee, "script/bee.lua"));
}
