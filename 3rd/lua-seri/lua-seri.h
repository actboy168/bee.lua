#ifndef LUA_SERIALIZE_H
#define LUA_SERIALIZE_H

#include <lua.h>

int seri_unpackptr(lua_State *L, void * buffer);
int seri_unpack(lua_State *L);
void * seri_pack(lua_State *L, int from, int *sz);
void * seri_packstring(const char * str, int sz);

#endif
