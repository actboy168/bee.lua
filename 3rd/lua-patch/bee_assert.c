#include "bee_assert.h"

#include "lauxlib.h"

void _bee_lua_assert(const char* message, const char* file, unsigned line) {
    fprintf(stderr, "(%s:%d) %s\n", file, line, message);
    fflush(stderr);
    abort();
}

void _bee_lua_apicheck(void* l, const char* message, const char* file, unsigned line) {
    lua_State* L = l;
    fprintf(stderr, "(%s:%d) %s\n", file, line, message);
    fflush(stderr);
    if (!lua_checkstack(L, LUA_MINSTACK)) {
        abort();
    }
    luaL_traceback(L, L, 0, 0);
    fprintf(stderr, "%s\n", lua_tostring(L, -1));
    fflush(stderr);
    lua_pop(L, 1);
    abort();
}
