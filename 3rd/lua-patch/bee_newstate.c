#include "bee_newstate.h"

#include "lua.h"

#if LUA_VERSION_NUM >= 505

#    include <stdio.h>
#    include <stdlib.h>
#    include <string.h>

#    include "bee_lua55.h"

static void *l_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
    (void)ud;
    (void)osize;
    if (nsize == 0) {
        free(ptr);
        return NULL;
    } else
        return realloc(ptr, nsize);
}

static int panic(lua_State *L) {
    const char *msg = (lua_type(L, -1) == LUA_TSTRING)
                          ? lua_tostring(L, -1)
                          : "error object is not a string";
    lua_writestringerror("PANIC: unprotected error in call to Lua API (%s)\n", msg);
    return 0;
}

static void warnfoff(void *ud, const char *message, int tocont);
static void warnfon(void *ud, const char *message, int tocont);
static void warnfcont(void *ud, const char *message, int tocont);

static int checkcontrol(lua_State *L, const char *message, int tocont) {
    if (tocont || *(message++) != '@')
        return 0;
    else {
        if (strcmp(message, "off") == 0)
            lua_setwarnf(L, warnfoff, L);
        else if (strcmp(message, "on") == 0)
            lua_setwarnf(L, warnfon, L);
        return 1;
    }
}

static void warnfoff(void *ud, const char *message, int tocont) {
    checkcontrol((lua_State *)ud, message, tocont);
}

static void warnfcont(void *ud, const char *message, int tocont) {
    lua_State *L = (lua_State *)ud;
    lua_writestringerror("%s", message);
    if (tocont)
        lua_setwarnf(L, warnfcont, L);
    else {
        lua_writestringerror("%s", "\n");
        lua_setwarnf(L, warnfon, L);
    }
}

static void warnfon(void *ud, const char *message, int tocont) {
    if (checkcontrol((lua_State *)ud, message, tocont))
        return;
    lua_writestringerror("%s", "Lua warning: ");
    warnfcont(ud, message, tocont);
}

struct lua_State *bee_lua_newstate() {
    lua_State *L = lua_newstate(l_alloc, NULL, *(unsigned int *)"Lua\0Lua\0");
    if (L) {
        lua_atpanic(L, &panic);
        lua_setwarnf(L, warnfoff, L);
    }
    return L;
}

#else

#    include "lauxlib.h"

struct lua_State* bee_lua_newstate() {
    return luaL_newstate();
}

#endif
