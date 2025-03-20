#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

struct lua_State;

struct lua_State* bee_lua_newstate();

#if defined(__cplusplus)
}
#endif
