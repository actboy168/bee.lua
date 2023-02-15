#include <bee/utility/file_handle.h>
#include <bee/nonstd/unreachable.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

namespace bee {
    FILE* file_handle::to_file(mode mode) const {
        switch (mode) {
        case mode::read:
            return fdopen(h, "rb");
        case mode::write:
            return fdopen(h, "wb");
        default:
            std::unreachable();
        }
    }

    file_handle file_handle::from_file(FILE* f) {
        return {fileno(f)};
    }

    file_handle file_handle::dup(FILE* f) {
        return {::dup(from_file(f).value())};
    }

    void file_handle::close() {
        if (valid()) {
            ::close(h);
            h = file_handle{}.h;
        }
    }
}
