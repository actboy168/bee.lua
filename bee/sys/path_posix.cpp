#include <bee/sys/path.h>
#include <dlfcn.h>

namespace bee::sys {
    static std::optional<fs::path> dll_path(void* module_handle) noexcept {
        ::Dl_info dl_info;
        dl_info.dli_fname = 0;
        const int ret     = ::dladdr(module_handle, &dl_info);
        if (0 != ret && dl_info.dli_fname != NULL) {
            std::error_code ec;
            auto path = fs::absolute(dl_info.dli_fname, ec);
            if (ec) {
                return std::nullopt;
            }
            return path.lexically_normal();
        }
        errno = EFAULT;
        return std::nullopt;
    }

    std::optional<fs::path> dll_path() noexcept {
        return dll_path((void*)&exe_path);
    }
}
