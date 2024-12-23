#include <bee/sys/path.h>
#include <unistd.h>

namespace bee::sys {
    std::optional<fs::path> exe_path() noexcept {
        std::error_code ec;
        auto res = fs::read_symlink("/proc/self/exe", ec);
        if (ec) {
            return std::nullopt;
        }
        return res;
    }
}
