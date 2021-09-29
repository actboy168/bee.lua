#include <bee/thread/simplethread.h>
#include <windows.h>
#include <process.h>

namespace bee {
    struct simplethread {
        thread_func func;
        void* ud;
    };

    static unsigned __stdcall thread_function(void *lpParam) {
        struct simplethread * t = (struct simplethread *)lpParam;
        t->func(t->ud);
        free(t);
        _endthreadex(0);
        return 0;
    }

    thread_handle thread_create(thread_func func, void* ud) {
        struct simplethread* thread = (struct simplethread*)malloc(sizeof(*thread));
        thread->func = func;
        thread->ud = ud;
        thread_handle handle = (thread_handle)_beginthreadex(NULL, 0, thread_function, (LPVOID)thread, 0, NULL);
        if (handle == NULL) {
            free(thread);
            return 0;
        }
        return handle;
    }

    void thread_wait(thread_handle handle) {
        HANDLE h = (HANDLE)handle;
        WaitForSingleObject(h, INFINITE);
        CloseHandle(h);
    }

    void thread_sleep(int msec) {
        Sleep(msec);
    }
}
