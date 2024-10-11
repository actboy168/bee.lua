#pragma once

struct lua_State;

int seri_unpack(lua_State* L, void* buffer);
int seri_unpackptr(lua_State* L, void* buffer);
void * seri_pack(lua_State* L, int from, int* sz);
void * seri_packstring(const char* str, int sz);
