#pragma once

namespace bee {
    using thread_handle = void*;
    using thread_func   = void (*)(void*) noexcept;
    thread_handle thread_create(thread_func func, void* ud) noexcept;
    void thread_wait(thread_handle handle) noexcept;
    void thread_sleep(int msec) noexcept;
    void thread_yield() noexcept;
}
