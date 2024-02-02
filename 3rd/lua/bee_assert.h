#pragma once

#include "lauxlib.h"

void _bee_lua_assert(const char* message, const char* file, unsigned line);
void _bee_lua_apicheck(lua_State* L, const char* message, const char* file, unsigned line);
