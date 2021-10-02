#include <bee/thread/simplethread.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>

namespace bee {
    struct simplethread {
        thread_func func;
        void* ud;
    };

    static void* thread_function(void * args) {
        struct simplethread* t = (struct simplethread*)args;
        t->func(t->ud);
        free(t);
        return NULL;
    }

    thread_handle thread_create(thread_func func, void* ud) {
        struct simplethread* thread = (struct simplethread*)malloc(sizeof(*thread));
        thread->func = func;
        thread->ud = ud;
        pthread_t id;
        int ret = pthread_create(&id, NULL, thread_function, thread);
        if (ret != 0) {
            free(thread);
            return 0;
        }
        return (thread_handle)id;
    }

    void thread_wait(thread_handle handle) {
        pthread_t pid = (pthread_t)handle;
        pthread_join(pid, NULL);
    }

    void thread_sleep(int msec) {
        usleep(msec * 1000);
    }
}
