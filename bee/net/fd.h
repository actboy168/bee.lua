#pragma once

#if defined(_WIN32)
#    include <cstdint>
#endif

namespace bee::net {
#if defined(_WIN32)
    using fd_t = uintptr_t;
#else
    using fd_t = int;
#endif
    static constexpr inline fd_t retired_fd = (fd_t)-1;
}
