#include <bee/thread/simplethread.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

#include <cstdlib>
#include <new>

namespace bee {
    struct simplethread {
        thread_func func;
        void* ud;
    };

    static void* thread_function(void* args) noexcept {
        simplethread* t = static_cast<simplethread*>(args);
        t->func(t->ud);
        delete t;
        return NULL;
    }

    thread_handle thread_create(thread_func func, void* ud) noexcept {
        simplethread* thread = new (std::nothrow) simplethread;
        if (!thread) {
            return 0;
        }
        thread->func = func;
        thread->ud   = ud;
        pthread_t id;
        int ret = pthread_create(&id, NULL, thread_function, thread);
        if (ret != 0) {
            delete thread;
            return 0;
        }
        return (thread_handle)id;
    }

    void thread_wait(thread_handle handle) noexcept {
        pthread_t pid = (pthread_t)handle;
        pthread_join(pid, NULL);
    }

    void thread_sleep(int msec) noexcept {
        usleep(msec * 1000);
    }

    void thread_yield() noexcept {
        sched_yield();
    }
}
