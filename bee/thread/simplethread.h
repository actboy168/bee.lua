#pragma once

namespace bee {
    using thread_handle = void*;
    using thread_func = void (*)(void *);
    thread_handle thread_create(thread_func func, void* ud);
    void thread_wait(thread_handle handle);
    void thread_sleep(int msec);
}
