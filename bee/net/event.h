#pragma once

#include <bee/net/fd.h>
#include <bee/nonstd/atomic_flag.h>

namespace bee::net {
    struct event {
        fd_t pipe[2] = { retired_fd, retired_fd };
        atomic_flag e;
        ~event() noexcept;
        bool open() noexcept;
        void set() noexcept;
        void clear() noexcept;
        fd_t fd() const noexcept;
    };
}
