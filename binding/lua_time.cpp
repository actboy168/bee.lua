#include <bee/thread/spinlock.h>
#include <binding/binding.h>

#include <cassert>
#include <cstdint>
#include <mutex>

#if defined(_WIN32)
#    include <Windows.h>
#elif defined(__APPLE__)
#    include <mach/mach_time.h>
#else
#    include <time.h>
#endif

namespace bee::lua_time {
    constexpr uint64_t MSecPerSec  = UINT64_C(1000);
    constexpr uint64_t NSecPerMSec = UINT64_C(1'000'000);

    static struct {
        spinlock mutex;
        int (*time_func)(lua_State*);
        int (*monotonic_func)(lua_State*);

#if defined(_WIN32)
        uint64_t frequency;
#elif defined(__APPLE__)
        mach_timebase_info_data_t timebase;
#endif
    } G;

    static uint64_t time_time() {
#if defined(_WIN32)
        FILETIME f;
        GetSystemTimePreciseAsFileTime(&f);
        uint64_t t = ((uint64_t)f.dwHighDateTime << 32) | f.dwLowDateTime;
        return t / (uint64_t)10000 - (uint64_t)11644473600000;
#else
        struct timespec ti;
        clock_gettime(CLOCK_REALTIME, &ti);
        return (uint64_t)ti.tv_sec * MSecPerSec + ti.tv_nsec / NSecPerMSec;
#endif
    }

    static int lua_time(lua_State* L) {
        lua_pushinteger(L, time_time());
        return 1;
    }

#if defined(_WIN32)
    constexpr uint64_t TenMHz = UINT64_C(10'000'000);
    static int lua_monotonic_TenMHz(lua_State* L) {
        static_assert(TenMHz % MSecPerSec == 0);
        constexpr uint64_t Frequency = TenMHz / MSecPerSec;
        LARGE_INTEGER li;
        QueryPerformanceCounter(&li);
        uint64_t now = (uint64_t)li.QuadPart;
        lua_pushinteger(L, now / Frequency);
        return 1;
    }
    static int lua_monotonic(lua_State* L) {
        LARGE_INTEGER li;
        QueryPerformanceCounter(&li);
        uint64_t now   = (uint64_t)li.QuadPart;
        uint64_t freq  = G.frequency;
        uint64_t whole = (now / freq) * MSecPerSec;
        uint64_t part  = (now % freq) * MSecPerSec / freq;
        lua_pushinteger(L, whole + part);
        return 1;
    }
#elif defined(__APPLE__)
    static int lua_monotonic(lua_State* L) {
        uint64_t now = mach_continuous_time();
        lua_pushinteger(L, now * G.timebase.numer / G.timebase.denom / NSecPerMSec);
        return 1;
    }
#else
    static int lua_monotonic(lua_State* L) {
        struct timespec ti;
        clock_gettime(CLOCK_MONOTONIC, &ti);
        lua_pushinteger(L, (uint64_t)ti.tv_sec * MSecPerSec + ti.tv_nsec / NSecPerMSec);
        return 1;
    }
#endif

    static void time_initialize() {
        std::unique_lock<spinlock> lock(G.mutex);
#if defined(_WIN32)
        LARGE_INTEGER li;
        QueryPerformanceFrequency(&li);
        G.frequency = li.QuadPart;
        G.time_func = lua_time;
        if (G.frequency == TenMHz) {
            G.monotonic_func = lua_monotonic_TenMHz;
        }
        else {
            G.monotonic_func = lua_monotonic;
        }
#elif defined(__APPLE__)
        if (KERN_SUCCESS != mach_timebase_info(&G.timebase)) {
            abort();
        }
        G.time_func      = lua_time;
        G.monotonic_func = lua_monotonic;
#else
        G.time_func      = lua_time;
        G.monotonic_func = lua_monotonic;
#endif
    }

    static int luaopen(lua_State* L) {
        time_initialize();
        luaL_Reg lib[] = {
            { "time", G.time_func },
            { "monotonic", G.monotonic_func },
            { NULL, NULL },
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);
        return 1;
    }
}

DEFINE_LUAOPEN(time)
