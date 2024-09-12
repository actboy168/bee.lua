#include <bee/crash/allocator.h>
#include <bee/crash/handler_linux.h>
#include <bee/crash/nanoid.h>
#include <bee/crash/stacktrace.h>
#include <bee/crash/unwind_linux.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstring>

#ifndef PR_SET_PTRACER
#    define PR_SET_PTRACER 0x59616d61
#endif

namespace bee::crash {
    constexpr int kExceptionSignals[]  = { SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS, SIGTRAP };
    constexpr int kNumHandledSignals   = sizeof(kExceptionSignals) / sizeof(kExceptionSignals[0]);
    constexpr unsigned kChildStackSize = 16000;

    static struct sigaction old_handlers[kNumHandledSignals];
    static stack_t old_stack;
    static stack_t new_stack;
    static bool handlers_installed        = false;
    static bool altstack_installed        = false;
    static std::atomic<handler*> handler_ = nullptr;

    static void install_altstack() noexcept {
        if (altstack_installed)
            return;
        memset(&old_stack, 0, sizeof(old_stack));
        memset(&new_stack, 0, sizeof(new_stack));
        const unsigned kSigStackSize = std::max<unsigned>(16384, SIGSTKSZ);
        if (sigaltstack(NULL, &old_stack) == -1 || !old_stack.ss_sp || old_stack.ss_size < kSigStackSize) {
            new_stack.ss_sp   = calloc(1, kSigStackSize);
            new_stack.ss_size = kSigStackSize;
            if (sigaltstack(&new_stack, NULL) == -1) {
                free(new_stack.ss_sp);
                return;
            }
            altstack_installed = true;
        }
    }

    static void uninstall_altstack() noexcept {
        if (!altstack_installed)
            return;
        stack_t current_stack;
        if (sigaltstack(NULL, &current_stack) == -1)
            return;
        if (current_stack.ss_sp == new_stack.ss_sp) {
            if (old_stack.ss_sp) {
                if (sigaltstack(&old_stack, NULL) == -1)
                    return;
            } else {
                stack_t disable_stack;
                disable_stack.ss_flags = SS_DISABLE;
                if (sigaltstack(&disable_stack, NULL) == -1)
                    return;
            }
        }
        free(new_stack.ss_sp);
        altstack_installed = false;
    }

    static void install_default_handler(int sig) noexcept {
#if defined(__ANDROID__)
        struct kernel_sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sys_sigemptyset(&sa.sa_mask);
        sa.sa_handler_ = SIG_DFL;
        sa.sa_flags    = SA_RESTART;
        sys_rt_sigaction(sig, &sa, NULL, sizeof(kernel_sigset_t));
#else
        signal(sig, SIG_DFL);
#endif
    }

    static void uninstall_handlers() noexcept {
        if (!handlers_installed)
            return;
        for (int i = 0; i < kNumHandledSignals; ++i) {
            if (sigaction(kExceptionSignals[i], &old_handlers[i], NULL) == -1) {
                install_default_handler(kExceptionSignals[i]);
            }
        }
        handlers_installed = false;
    }

    static void signal_handler(int sig, siginfo_t* info, void* uc) noexcept {
        install_default_handler(sig);
        if (!handler_.load()->handle_signal(sig, info, uc)) {
            uninstall_handlers();
        }
        if (info->si_code <= 0 || sig == SIGABRT) {
            if (::syscall(SYS_tgkill, getpid(), ::syscall(__NR_gettid), sig) < 0) {
                _exit(1);
            }
        }
    }

    static bool install_handlers() noexcept {
        if (handlers_installed)
            return false;
        for (int i = 0; i < kNumHandledSignals; ++i) {
            if (sigaction(kExceptionSignals[i], NULL, &old_handlers[i]) == -1)
                return false;
        }
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sigemptyset(&sa.sa_mask);
        for (int i = 0; i < kNumHandledSignals; ++i) {
            sigaddset(&sa.sa_mask, kExceptionSignals[i]);
        }
        sa.sa_sigaction = signal_handler;
        sa.sa_flags     = SA_ONSTACK | SA_SIGINFO;
        for (int i = 0; i < kNumHandledSignals; ++i) {
            sigaction(kExceptionSignals[i], &sa, NULL);
        }
        handlers_installed = true;
        return true;
    }

    handler::handler(const char* dump_path) noexcept {
        handler* expected = nullptr;
        if (handler_.compare_exchange_strong(expected, this)) {
            if (dump_path[0] == '-' && dump_path[1] == '\0') {
                dump_path_[0] = '\0';
            } else {
                snprintf(dump_path_, sizeof(dump_path_) / sizeof(dump_path_[0]), "%s/crash_%s.log", dump_path, nanoid().c_str());
            }
            memset(&context, 0, sizeof(context));
            install_altstack();
            install_handlers();
        }
    }

    handler::~handler() noexcept {
        uninstall_altstack();
        uninstall_handlers();
        auto expected = this;
        handler_.compare_exchange_strong(expected, nullptr);
    }

    int handler::thread_func(void* arg) noexcept {
        handler* self = reinterpret_cast<handler*>(arg);
        assert(self);
        close(self->fdes[1]);
        self->finish_semaphore();
        close(self->fdes[0]);
        return self->write_dump() == false;
    }

    bool handler::handle_signal(int /*sig*/, siginfo_t* info, void* uc) noexcept {
        bool signal_trusted     = info->si_code > 0;
        bool signal_pid_trusted = info->si_code == SI_USER || info->si_code == SI_TKILL;
        if (signal_trusted || (signal_pid_trusted && info->si_pid == getpid())) {
            prctl(PR_SET_DUMPABLE, 1, 0, 0, 0);
        }
        memcpy(&context, uc, sizeof(ucontext_t));
        tid = syscall(__NR_gettid);
        pid = getpid();
        allocator alloc;
        uint8_t* stack = reinterpret_cast<uint8_t*>(alloc.allocate(kChildStackSize));
        if (!stack)
            return false;
        stack += kChildStackSize;
        uint8_t* p = stack - 16;
        for (int i = 0; i < 16; ++i) {
            *p++ = 0;
        }
        if (pipe(fdes) == -1) {
            fdes[0] = fdes[1] = -1;
            return false;
        }
        const pid_t child = clone(thread_func, stack, CLONE_FS | CLONE_UNTRACED, this, NULL, NULL, NULL);
        if (child == -1) {
            close(fdes[0]);
            close(fdes[1]);
            return false;
        }
        close(fdes[0]);
        prctl(PR_SET_PTRACER, child, 0, 0, 0);
        start_semaphore();
        int status = 0;
        int r;
        do {
            r = waitpid(child, &status, __WALL);
        } while (r == -1 && errno == EINTR);
        assert(r != -1);
        close(fdes[1]);
        return r != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }

    void handler::start_semaphore() noexcept {
        int r;
        do {
            r = write(fdes[1], "a", sizeof(char));
        } while (r == -1 && errno == EINTR);
        assert(r != -1);
    }

    void handler::finish_semaphore() noexcept {
        char msg;
        int r;
        do {
            r = read(fdes[0], &msg, sizeof(char));
        } while (r == -1 && errno == EINTR);
        assert(r != -1);
    }

    struct writefile {
        writefile(const char* filename) noexcept
            : file(open(filename, O_WRONLY | O_CREAT | O_EXCL, 0600)) {}
        ~writefile() noexcept {
            if (file != -1) {
                close(file);
            }
        }
        explicit operator bool() const noexcept {
            return file != -1;
        }
        bool write(const std::string& str) noexcept {
            return ::write(file, str.data(), (ssize_t)str.size()) == (ssize_t)str.size();
        }
        template <size_t n>
        bool write(char (&str)[n]) noexcept {
            return ::write(file, str, n - 1) == (n - 1);
        }
        int file;
    };

    static std::string get_stacktrace(ucontext_t* ctx) noexcept {
        stacktrace s;
        if (!s.initialize()) {
            return {};
        }
        constexpr auto func = +[](void* pc, void* ud) {
            stacktrace* s = (stacktrace*)ud;
            s->add_frame(pc);
            return true;
        };
        unwind(ctx, 0, func, &s);
        return s.to_string();
    }

    bool handler::write_dump() noexcept {
        auto str = get_stacktrace(&context);
        if (dump_path_[0] == L'\0') {
            printf("\n\nCrash log: \n%s\n", str.c_str());
            return true;
        }
        do {
            writefile file(dump_path_);
            if (!file) {
                break;
            }
            if (!file.write("stacktrace:\n")) {
                break;
            }
            if (!file.write(str)) {
                break;
            }
        } while (false);
        printf("\n\nCrash log generated at: %s\n", dump_path_);
        return true;
    }
}
