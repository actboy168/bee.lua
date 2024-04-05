#include <Windows.h>
#include <bee/thread/simplethread.h>
#include <process.h>

#include <new>

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

    void thread_sleep(int msec) noexcept {
        Sleep(msec);
    }

    void thread_yield() noexcept {
        SwitchToThread();
    }
}
