#include <bee/utility/file_handle.h>
#include <fcntl.h>
#include <sys/file.h>

namespace bee {
    file_handle file_handle::lock(const fs::path& filename) {
        int fd = ::open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_EXLOCK | O_NONBLOCK, 0644);
        return {fd};
    }

    file_handle file_handle::open_link(const fs::path& filename) {
        int fd = ::open(filename.c_str(), O_SYMLINK);
        return {fd};
    }

    std::optional<fs::path> file_handle::path() const {
        if (!valid()) {
            return std::nullopt;
        }
        char path[PATH_MAX];
        if (::fcntl(h, F_GETPATH, path) < 0) {
            return std::nullopt;
        }
        return fs::path(path);
    }
}
