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
    constexpr int64_t MSecPerSec  = 1000ll;
    constexpr int64_t NSecPerMSec = 1'000'000ll;

    static struct {
        spinlock mutex;
        int (*time_func)(lua_State*);
        int (*monotonic_func)(lua_State*);

#if defined(_WIN32)
        int64_t frequency;
#elif defined(__APPLE__)
        mach_timebase_info_data_t timebase;
#endif
    } G;

#if defined(_WIN32)
    static int lua_time(lua_State* L) {
        FILETIME f;
        GetSystemTimePreciseAsFileTime(&f);
        int64_t t = ((int64_t)f.dwHighDateTime << 32) | f.dwLowDateTime;
        t -= 116444736000000000ll;
        lua_pushinteger(L, t / 10000ll);
        return 1;
    }
#else
    static int lua_time(lua_State* L) {
        struct timespec ti;
        clock_gettime(CLOCK_REALTIME, &ti);
        lua_pushinteger(L, (int64_t)ti.tv_sec * MSecPerSec + ti.tv_nsec / NSecPerMSec);
        return 1;
    }
#endif

#if defined(_WIN32)
    template <int64_t Frequency>
    static int lua_monotonic(lua_State* L) {
        static_assert(Frequency % MSecPerSec == 0);
        constexpr int64_t Div = Frequency / MSecPerSec;
        LARGE_INTEGER li;
        QueryPerformanceCounter(&li);
        int64_t now = (int64_t)li.QuadPart;
        lua_pushinteger(L, now / Div);
        return 1;
    }
    static int lua_monotonic(lua_State* L) {
        LARGE_INTEGER li;
        QueryPerformanceCounter(&li);
        int64_t now   = (int64_t)li.QuadPart;
        int64_t freq  = G.frequency;
        int64_t whole = (now / freq) * MSecPerSec;
        int64_t part  = (now % freq) * MSecPerSec / freq;
        lua_pushinteger(L, whole + part);
        return 1;
    }
#elif defined(__APPLE__)
    static int lua_monotonic(lua_State* L) {
        uint64_t now = mach_continuous_time();
        lua_pushinteger(L, now * G.timebase.numer / G.timebase.denom / NSEC_PER_MSEC);
        return 1;
    }
#else
    static int lua_monotonic(lua_State* L) {
        struct timespec ti;
        clock_gettime(CLOCK_MONOTONIC, &ti);
        lua_pushinteger(L, (int64_t)ti.tv_sec * MSecPerSec + ti.tv_nsec / NSecPerMSec);
        return 1;
    }
#endif

    static void time_initialize() {
        std::unique_lock<spinlock> lock(G.mutex);
#if defined(_WIN32)
        constexpr int64_t TenMHz = 10'000'000ll;
        LARGE_INTEGER li;
        QueryPerformanceFrequency(&li);
        G.frequency = li.QuadPart;
        G.time_func = lua_time;
        if (G.frequency == TenMHz) {
            G.monotonic_func = lua_monotonic<TenMHz>;
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
