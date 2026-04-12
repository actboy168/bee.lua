#include <bee/async/async.h>

#if defined(__linux__)
#    include <bee/async/async_epoll_linux.h>
#    if !defined(BEE_ASYNC_BACKEND_EPOLL)
#        include <bee/async/async_uring_linux.h>
#    endif
#endif

namespace bee::async {

    std::unique_ptr<async> create() {
#if defined(__linux__)
#    if defined(BEE_ASYNC_BACKEND_EPOLL)
        return std::make_unique<async_epoll>();
#    else
        auto uring = std::make_unique<async_uring>();
        if (uring->valid()) {
            return uring;
        }
        return std::make_unique<async_epoll>();
#    endif
#else
        return std::make_unique<async>();
#endif
    }

}  // namespace bee::async
