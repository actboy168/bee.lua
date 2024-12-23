#include <bee/sys/path.h>
#include <unistd.h>

#if defined(__FreeBSD__)
#    include <sys/param.h>
#    include <sys/sysctl.h>
#elif defined(__OpenBSD__)
#    include <sys/sysctl.h>
#endif

namespace bee::sys {
#if defined(__FreeBSD__)
    std::optional<fs::path> exe_path() noexcept {
        int name[] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
        char exe[1024];
        size_t length = 1024;
        int error     = sysctl(name, 4, exe, &length, NULL, 0);
        if (error < 0 || length <= 1) {
            return std::nullopt;
        }
        return fs::path(exe, exe + length - 1);
    }
#elif defined(__OpenBSD__)
    std::optional<fs::path> exe_path() noexcept {
        int name[] = { CTL_KERN, KERN_PROC_ARGS, getpid(), KERN_PROC_ARGV };
        size_t argc;
        if (sysctl(name, 4, NULL, &argc, NULL, 0) < 0) {
            return std::nullopt;
        }
        const char** argv = (const char**)malloc(argc);
        if (!argv) {
            return std::nullopt;
        }
        if (sysctl(name, 4, argv, &argc, NULL, 0) < 0) {
            return std::nullopt;
        }
        return fs::path(argv[0]);
    }
#else
    std::optional<fs::path> exe_path() noexcept {
        std::error_code ec;
        auto res = fs::read_symlink("/proc/self/exe", ec);
        if (ec) {
            return std::nullopt;
        }
        return res;
    }
#endif
}
