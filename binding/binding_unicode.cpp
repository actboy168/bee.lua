#include <lua.hpp>
#include <bee/lua/binding.h>
#include <bee/utility/unicode.h>

static int lu2a(lua_State* L) {
	std::string r = bee::u2a(bee::lua::to_strview(L, 1));
	lua_pushlstring(L, r.data(), r.size());
	return 1;
}

static int la2u(lua_State* L) {
	std::string r = bee::a2u(bee::lua::to_strview(L, 1));
	lua_pushlstring(L, r.data(), r.size());
	return 1;
}

extern "C" __declspec(dllexport)
int luaopen_bee_unicode(lua_State* L) {
	luaL_Reg lib[] = {
		{ "u2a", lu2a },
		{ "a2u", la2u },
		{ NULL, NULL }
	};
	luaL_newlib(L, lib);
	return 1;
}