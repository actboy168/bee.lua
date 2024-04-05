#pragma once

#if __has_include(<version>)
#    include <version>
#endif

#if defined(__cpp_lib_debugging)
#    include <debugging>
#else
#    if defined(_WIN32)
#        include <intrin.h>
#        include <windows.h>
#    elif defined(__APPLE__)
#        include <sys/sysctl.h>
#        include <unistd.h>
#    endif

namespace std {
    inline void breakpoint() noexcept {
#    if defined(_MSC_VER)
        __debugbreak();
#    elif defined(__clang__)
        __builtin_debugtrap();
#    endif
    }
    inline bool is_debugger_present() noexcept {
#    if defined(_WIN32)
        return IsDebuggerPresent() != 0;
#    elif defined(__APPLE__)
        int mib[4];
        struct kinfo_proc info;
        size_t size         = sizeof(info);
        info.kp_proc.p_flag = 0;
        mib[0]              = CTL_KERN;
        mib[1]              = KERN_PROC;
        mib[2]              = KERN_PROC_PID;
        mib[3]              = getpid();
        if (sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, nullptr, 0) != 0) {
            return false;
        }
        return (info.kp_proc.p_flag & P_TRACED) != 0;
#    else
        return false;
#    endif
    }
    inline void breakpoint_if_debugging() noexcept {
        if (is_debugger_present()) {
            breakpoint();
        }
    }
}

#endif
