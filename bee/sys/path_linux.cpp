#include <bee/error.h>
#include <bee/sys/path.h>
#include <unistd.h>

namespace bee::sys {
    path_expected exe_path() noexcept {
        std::error_code ec;
        auto res = fs::read_symlink("/proc/self/exe", ec);
        if (ec) {
            return unexpected<std::string>(error::sys_errmsg("exe_path"));
        }
        return res;
    }
}
