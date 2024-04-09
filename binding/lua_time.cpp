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
    static struct {
        spinlock mutex;
        int (*time_func)(lua_State*);
        int (*monotonic_func)(lua_State*);
        int (*counter_func)(lua_State*);

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
        return (uint64_t)ti.tv_sec * 1000 + ti.tv_nsec / 1000000;
#endif
    }

    static int lua_time(lua_State* L) {
        lua_pushinteger(L, time_time());
        return 1;
    }

    static uint64_t time_monotonic() {
#if defined(_WIN32)
        return GetTickCount64();
#elif defined(__APPLE__)
        return mach_continuous_time() * timebase.numer / timebase.denom / 1000000;
#else
        struct timespec ti;
        clock_gettime(CLOCK_MONOTONIC, &ti);
        return (uint64_t)ti.tv_sec * 1000 + ti.tv_nsec / 1000000;
#endif
    }

    static int lua_monotonic(lua_State* L) {
        lua_pushinteger(L, time_monotonic());
        return 1;
    }

#if defined(_WIN32)
    constexpr uint64_t TenMHz       = UINT64_C(10000000);
    constexpr uint64_t MSEC_PER_SEC = UINT64_C(1000);
    static int lua_counter_TenMHz(lua_State* L) {
        LARGE_INTEGER li;
        QueryPerformanceCounter(&li);
        uint64_t now = (uint64_t)li.QuadPart;
        static_assert(TenMHz % MSEC_PER_SEC == 0);
        constexpr uint64_t Frequency = TenMHz / MSEC_PER_SEC;
        lua_pushinteger(L, now / Frequency);
        return 1;
    }
    static int lua_counter(lua_State* L) {
        LARGE_INTEGER li;
        QueryPerformanceCounter(&li);
        uint64_t now   = (uint64_t)li.QuadPart;
        uint64_t freq  = G.frequency;
        uint64_t whole = (now / freq) * MSEC_PER_SEC;
        uint64_t part  = (now % freq) * MSEC_PER_SEC / freq;
        lua_pushinteger(L, whole + part);
        return 1;
    }
#endif

    static void time_initialize() {
        std::unique_lock<spinlock> lock(G.mutex);
#if defined(_WIN32)
        LARGE_INTEGER li;
        QueryPerformanceFrequency(&li);
        G.frequency      = li.QuadPart;
        G.time_func      = lua_time;
        G.monotonic_func = lua_monotonic;
        if (G.frequency == TenMHz) {
            G.counter_func = lua_counter_TenMHz;
        }
        else {
            G.counter_func = lua_counter;
        }
#elif defined(__APPLE__)
        if (KERN_SUCCESS != mach_timebase_info(&G.timebase)) {
            abort();
        }
        G.time_func      = lua_time;
        G.monotonic_func = lua_monotonic;
        G.counter_func   = lua_monotonic;
#else
        G.time_func      = lua_time;
        G.monotonic_func = lua_monotonic;
        G.counter_func   = lua_monotonic;
#endif
    }

    static int luaopen(lua_State* L) {
        time_initialize();
        luaL_Reg lib[] = {
            { "time", G.time_func },
            { "monotonic", G.monotonic_func },
            { "counter", G.counter_func },
            { NULL, NULL },
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);
        return 1;
    }
}

DEFINE_LUAOPEN(time)
