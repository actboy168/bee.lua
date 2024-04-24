#include <bee/sys/path.h>
#include <bee/utility/dynarray.h>
#include <mach-o/dyld.h>

namespace bee::sys {
    path_expected exe_path() noexcept {
        uint32_t path_len = 0;
        _NSGetExecutablePath(0, &path_len);
        if (path_len <= 1) {
            return unexpected<std::string>("_NSGetExecutablePath failed.");
        }
        dynarray<char> buf(path_len);
        int rv = _NSGetExecutablePath(buf.data(), &path_len);
        if (rv != 0) {
            return unexpected<std::string>("_NSGetExecutablePath failed.");
        }
        return fs::path(buf.data(), buf.data() + path_len - 1);
    }
}
