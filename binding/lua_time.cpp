#include <bee/lua/binding.h>

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace bee::lua_time {
    static uint64_t time_monotonic() {
        uint64_t t;
#if defined(_WIN32)
        t = GetTickCount64();
#else
        struct timespec ti;
        clock_gettime(CLOCK_MONOTONIC, &ti);
        t = (uint64_t)ti.tv_sec * 1000 + ti.tv_nsec / 1000000;
#endif
        return t;
    }

    static uint64_t time_time() {
        uint64_t t;
#if defined(_WIN32)
        FILETIME f;
        GetSystemTimeAsFileTime(&f);
        t = ((uint64_t)f.dwHighDateTime << 32) | f.dwLowDateTime;
        t = t / (uint64_t)10000 - (uint64_t)11644473600000;
#else
        struct timespec ti;
        clock_gettime(CLOCK_REALTIME, &ti);
        t = (uint64_t)ti.tv_sec * 1000 + ti.tv_nsec / 1000000;
#endif
        return t;
    }

    static int lmonotonic(lua_State* L) {
        lua_pushinteger(L, time_monotonic());
        return 1;
    }
    static int ltime(lua_State* L) {
        lua_pushinteger(L, time_time());
        return 1;
    }
    static int luaopen(lua_State* L) {
        luaL_Reg lib[] = {
            {"time", ltime},
            {"monotonic", lmonotonic},
            {NULL, NULL},
        };
        lua_newtable(L);
        luaL_setfuncs(L, lib, 0);
        return 1;
    }
}

DEFINE_LUAOPEN(time)
