#include <binding/binding.h>

#if defined(_WIN32)
#   include <Windows.h>
#else
#   include <time.h>
#endif

namespace bee::lua_time {
    static uint64_t time_monotonic() {
#if defined(_WIN32)
        return GetTickCount64();
#else
        struct timespec ti;
        clock_gettime(CLOCK_MONOTONIC, &ti);
        return (uint64_t)ti.tv_sec * 1000 + ti.tv_nsec / 1000000;
#endif
    }

    static uint64_t time_time() {
#if defined(_WIN32)
        FILETIME f;
        GetSystemTimeAsFileTime(&f);
        uint64_t t = ((uint64_t)f.dwHighDateTime << 32) | f.dwLowDateTime;
        t = t / (uint64_t)10000 - (uint64_t)11644473600000;
        return t;
#else
        struct timespec ti;
        clock_gettime(CLOCK_REALTIME, &ti);
        return (uint64_t)ti.tv_sec * 1000 + ti.tv_nsec / 1000000;
#endif
    }

    static uint64_t time_counter() {
#if defined(_WIN32)
        LARGE_INTEGER li;
        QueryPerformanceCounter(&li);
        return li.QuadPart;
#else
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        return now.tv_sec*(uint64_t)(1000000000) + now.tv_nsec;
#endif
    }

    static uint64_t time_frequency() {
#if defined(_WIN32)
        LARGE_INTEGER li;
        QueryPerformanceFrequency(&li);
        return li.QuadPart;
#else
        return (uint64_t)(1000000000);
#endif
    }

    static int lmonotonic(lua_State* L) {
        lua_pushinteger(L, time_monotonic());
        return 1;
    }
    static int ltime(lua_State* L) {
        lua_pushinteger(L, time_time());
        return 1;
    }
    static int lcounter(lua_State* L) {
        uint64_t freq = lua_tointeger(L, lua_upvalueindex(1));
        uint64_t ti = time_counter();
        lua_pushnumber(L, (double)ti * 1000 / freq);
        return 1;
    }

    static int luaopen(lua_State* L) {
        luaL_Reg lib[] = {
            {"time", ltime},
            {"monotonic", lmonotonic},
            {"counter", NULL},
            {NULL, NULL},
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);

        lua_pushinteger(L, time_frequency());
        lua_pushcclosure(L, lcounter, 1);
        lua_setfield(L, -2, "counter");

        return 1;
    }
}

DEFINE_LUAOPEN(time)
