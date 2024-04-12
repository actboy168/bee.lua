#include <bee/nonstd/format.h>
#include <bee/utility/file_handle.h>
#include <sys/file.h>
#include <unistd.h>

#if defined(__FreeBSD__)
#    include <sys/user.h>
#endif

namespace bee {
    file_handle file_handle::lock(const fs::path& filename) noexcept {
        int fd = ::open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
            return {};
        }
        if (::flock(fd, LOCK_EX | LOCK_NB) == -1) {
            ::close(fd);
            return {};
        }
        return { fd };
    }

    file_handle file_handle::open_link(const fs::path& filename) noexcept {
        int fd = ::open(filename.c_str(), O_PATH | O_NOFOLLOW);
        return { fd };
    }

    std::optional<fs::path> file_handle::path() const {
#if defined(F_KINFO)
        if (!valid()) {
            return std::nullopt;
        }
        struct kinfo_file kf;
        kf.kf_structsize = sizeof(kf);
        if (::fcntl(h, F_KINFO, &kf) != 0) {
            return std::nullopt;
        }
        return fs::path(kf.kf_path);
#else
        
        return std::nullopt;
#endif
    }
}
