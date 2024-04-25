#include <bee/sys/file_handle.h>
#include <fcntl.h>
#include <sys/file.h>

namespace bee {
    file_handle file_handle::lock(path_view filename) noexcept {
        int fd = ::open(filename.data(), O_WRONLY | O_CREAT | O_TRUNC | O_EXLOCK | O_NONBLOCK, 0644);
        return from_native(fd);
    }

    file_handle file_handle::open_link(path_view filename) noexcept {
        int fd = ::open(filename.data(), O_SYMLINK);
        return from_native(fd);
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
