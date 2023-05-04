#include <Windows.h>
#include <bee/thread/simplethread.h>
#include <process.h>

#include <new>

namespace bee {
    struct simplethread {
        thread_func func;
        void* ud;
    };

    static unsigned __stdcall thread_function(void* lpParam) noexcept {
        simplethread* t = static_cast<simplethread*>(lpParam);
        t->func(t->ud);
        delete t;
        _endthreadex(0);
        return 0;
    }

    thread_handle thread_create(thread_func func, void* ud) noexcept {
        simplethread* thread = new (std::nothrow) simplethread;
        if (!thread) {
            return 0;
        }
        thread->func         = func;
        thread->ud           = ud;
        thread_handle handle = (thread_handle)_beginthreadex(NULL, 0, thread_function, (LPVOID)thread, 0, NULL);
        if (handle == NULL) {
            delete thread;
            return 0;
        }
        return handle;
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
