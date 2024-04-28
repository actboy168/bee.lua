#include <Windows.h>
#include <bee/thread/simplethread.h>
#include <process.h>

#include <new>

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#    define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x2
#endif

namespace bee {
    struct simplethread {
        thread_func func;
        void* ud;
    };

    static unsigned __stdcall thread_function(void* args) noexcept {
        simplethread* ptr = static_cast<simplethread*>(args);
        simplethread v    = *ptr;
        delete ptr;
        v.func(v.ud);
        _endthreadex(0);
        return 0;
    }

    thread_handle thread_create(thread_func func, void* ud) noexcept {
        simplethread* thread = new (std::nothrow) simplethread;
        if (!thread) {
            return 0;
        }
        thread->func = func;
        thread->ud   = ud;
        auto handle  = _beginthreadex(NULL, 0, thread_function, (LPVOID)thread, 0, NULL);
        if (handle == 0) {
            delete thread;
            return 0;
        }
        return (thread_handle)handle;
    }

    void thread_wait(thread_handle handle) noexcept {
        HANDLE h = (HANDLE)handle;
        WaitForSingleObject(h, INFINITE);
        CloseHandle(h);
    }

    extern "C" NTSTATUS NTAPI NtSetTimerResolution(ULONG RequestedResolution, BOOLEAN Set, PULONG ActualResolution);

    static DWORD GetTimerFlags() noexcept {
        HANDLE timer = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        if (timer == NULL) {
            ULONG actual_res = 0;
            NtSetTimerResolution(10000, TRUE, &actual_res);
            return 0;
        } else {
            CloseHandle(timer);
            // supported in Windows 10 1803+
            return CREATE_WAITABLE_TIMER_HIGH_RESOLUTION;
        }
    }

    static DWORD TimerFlags = GetTimerFlags();

    void thread_sleep(int msec) noexcept {
        if (msec < 0) {
            return;
        }
        HANDLE timer = CreateWaitableTimerExW(NULL, NULL, TimerFlags, TIMER_ALL_ACCESS);
        if (!timer) {
            return;
        }
        LARGE_INTEGER time;
        time.QuadPart = -(msec * 10000);
        if (SetWaitableTimer(timer, &time, 0, NULL, NULL, 0)) {
            WaitForSingleObject(timer, INFINITE);
        }
        CloseHandle(timer);
    }

    void thread_yield() noexcept {
        SwitchToThread();
    }
}
