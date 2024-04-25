#include <bee/lua/module.h>
#include <bee/thread/spinlock.h>

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
    constexpr int64_t MSecPerSec = 1000ll;
#if !defined(_WIN32)
    constexpr int64_t NSecPerMSec = 1'000'000ll;
#endif

    static struct {
        spinlock mutex;
        int (*time_func)(lua_State*);
        int (*monotonic_func)(lua_State*);
        int (*thread_func)(lua_State*);

#if defined(_WIN32)
        int64_t frequency;
#elif defined(__APPLE__)
        mach_timebase_info_data_t timebase;
#endif
    } G;

#if defined(_WIN32)
    static int64_t tomsec(FILETIME* f) {
        int64_t t = ((int64_t)f->dwHighDateTime << 32) | f->dwLowDateTime;
        return t / 10000ll;
    }

    static int lua_time(lua_State* L) {
        FILETIME f;
        GetSystemTimePreciseAsFileTime(&f);
        int64_t t = tomsec(&f);
        t -= 11644473600000ll;
        lua_pushinteger(L, t);
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
    template <uint32_t Numer, uint32_t Denom>
    static int lua_monotonic(lua_State* L) {
        static_assert((NSEC_PER_MSEC * Denom) % Numer == 0);
        constexpr uint64_t Freq = (NSEC_PER_MSEC * Denom) / Numer;
        uint64_t now            = mach_continuous_time();
        lua_pushinteger(L, now / Freq);
        return 1;
    }
    static int lua_monotonic(lua_State* L) {
        uint64_t now   = mach_continuous_time();
        uint64_t freq  = NSEC_PER_MSEC * G.timebase.denom;
        uint64_t whole = (now / freq) * G.timebase.numer;
        uint64_t part  = (now % freq) * G.timebase.numer / freq;
        lua_pushinteger(L, whole + part);
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

#if defined(_WIN32)
    static int lua_thread(lua_State* L) {
        FILETIME created;
        FILETIME exited;
        FILETIME kernel;
        FILETIME user;
        if (0 == GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user)) {
            lua_pushinteger(L, 0);
            return 1;
        }
        lua_pushinteger(L, tomsec(&kernel) + tomsec(&user));
        return 1;
    }
#else
    static int lua_thread(lua_State* L) {
        struct timespec ti;
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ti);
        lua_pushinteger(L, (int64_t)ti.tv_sec * MSecPerSec + ti.tv_nsec / NSecPerMSec);
        return 1;
    }
#endif

    static void time_initialize() {
        std::unique_lock<spinlock> lock(G.mutex);
        G.time_func      = lua_time;
        G.monotonic_func = lua_monotonic;
        G.thread_func    = lua_thread;
#if defined(_WIN32)
        constexpr int64_t TenMHz = 10'000'000ll;
        LARGE_INTEGER li;
        QueryPerformanceFrequency(&li);
        G.frequency = li.QuadPart;
        if (G.frequency == TenMHz) {
            G.monotonic_func = lua_monotonic<TenMHz>;
        }
#elif defined(__APPLE__)
        if (KERN_SUCCESS != mach_timebase_info(&G.timebase)) {
            abort();
        }
        if (G.timebase.numer == 125 && G.timebase.denom == 3) {
            G.monotonic_func = lua_monotonic<125, 3>;
        } else if (G.timebase.numer == 1 && G.timebase.denom == 1) {
            G.monotonic_func = lua_monotonic<1, 1>;
        }
#endif
    }

    static int luaopen(lua_State* L) {
        time_initialize();
        luaL_Reg lib[] = {
            { "time", G.time_func },
            { "monotonic", G.monotonic_func },
            { "thread", G.thread_func },
            { NULL, NULL },
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);
        return 1;
    }
}

DEFINE_LUAOPEN(time)
