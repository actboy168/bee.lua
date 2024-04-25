#include <bee/nonstd/format.h>
#include <bee/sys/file_handle.h>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace bee {
    file_handle file_handle::lock(path_view filename) noexcept {
        int fd = ::open(filename.data(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
            return {};
        }
        if (::flock(fd, LOCK_EX | LOCK_NB) == -1) {
            ::close(fd);
            return {};
        }
        return from_native(fd);
    }

    file_handle file_handle::open_link(path_view filename) noexcept {
        int fd = ::open(filename.data(), O_PATH | O_NOFOLLOW);
        return from_native(fd);
    }

    std::optional<fs::path> file_handle::path() const {
        if (!valid()) {
            return std::nullopt;
        }
        std::error_code ec;
        auto res = fs::read_symlink(std::format("/proc/self/fd/{}", h), ec);
        if (ec) {
            return std::nullopt;
        }
        return res;
    }
}
