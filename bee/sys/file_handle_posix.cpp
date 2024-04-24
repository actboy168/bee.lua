#include <bee/nonstd/unreachable.h>
#include <bee/sys/file_handle.h>
#include <unistd.h>

#include <cstdio>

namespace bee {
    FILE* file_handle::to_file(mode mode) const noexcept {
        switch (mode) {
        case mode::read:
            return fdopen(h, "rb");
        case mode::write:
            return fdopen(h, "wb");
        default:
            std::unreachable();
        }
    }

    file_handle file_handle::from_file(FILE* f) noexcept {
        return from_native(fileno(f));
    }

    file_handle file_handle::dup(FILE* f) noexcept {
        return from_native(::dup(from_file(f).value()));
    }

    void file_handle::close() noexcept {
        if (valid()) {
            ::close(h);
            h = file_handle {}.h;
        }
    }
}
