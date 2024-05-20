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
        struct CrashContext {
            siginfo_t siginfo;
            pid_t tid;
            ucontext_t context;
#if defined(__aarch64__)
            struct fpsimd_context float_state;
#else
            std::remove_pointer<fpregset_t>::type float_state;
#endif
        };
        static int thread_func(void* arg) noexcept;
        void start_semaphore() noexcept;
        void finish_semaphore() noexcept;
        bool write_dump() noexcept;
        int fdes[2] = { -1, -1 };
        pid_t pid;
        CrashContext context;
        char dump_path_[1024];
    };
}
