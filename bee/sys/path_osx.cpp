#include <bee/sys/path.h>
#include <bee/utility/dynarray.h>
#include <mach-o/dyld.h>

#include <cassert>

namespace bee::sys {
    std::optional<fs::path> exe_path() noexcept {
        uint32_t path_len = 0;
        _NSGetExecutablePath(0, &path_len);
        assert(path_len > 1);
        dynarray<char> buf(path_len);
        int rv = _NSGetExecutablePath(buf.data(), &path_len);
        (void)rv;
        assert(rv == 0);
        return fs::path(buf.data(), buf.data() + path_len - 1);
    }
}
