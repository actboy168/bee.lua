#include <bee/utility/file_handle.h>
#include <bee/utility/unreachable.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>

namespace bee {
    FILE* file_handle::to_file(mode mode) const {
        switch (mode) {
        case mode::read:
            return fdopen(h, "rb");
        case mode::write:
            return fdopen(h, "wb");
        default:
            unreachable();
        }
    }

    file_handle file_handle::from_file(FILE* f) {
        return {fileno(f)};
    }

    file_handle file_handle::dup(FILE* f) {
        return {::dup(from_file(f).value())};
    }

    file_handle file_handle::lock(const string_type& filename) {
#if defined(__APPLE__) 
        int fd = ::open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_EXLOCK | O_NONBLOCK, 0644);
        return {fd};
#else
        int fd = ::open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
            return {};
        }
        if (::flock(fd, LOCK_EX | LOCK_NB) == -1) {
            close(fd);
            return {};
        }
        return {fd};
#endif
    }
}
