#pragma once

#include <signal.h>
#include <sys/ucontext.h>

#include <type_traits>

namespace bee::crash {
    class handler {
    public:
        handler(const char* dump_path) noexcept;
        ~handler() noexcept;
        handler(const handler&)            = delete;
        handler& operator=(const handler&) = delete;
        bool handle_signal(int sig, siginfo_t* info, void* uc) noexcept;

    private:
        static int thread_func(void* arg) noexcept;
        void start_semaphore() noexcept;
        void finish_semaphore() noexcept;
        bool write_dump() noexcept;
        int fdes[2] = { -1, -1 };
        pid_t pid;
        pid_t tid;
        ucontext_t context;
        char dump_path_[1024];
    };
}
