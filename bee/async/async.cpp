#include <bee/async/async.h>

#if defined(__linux__)
#    include <bee/async/async_epoll_linux.h>
#    include <bee/async/async_uring_linux.h>
#endif

namespace bee::async {

std::unique_ptr<async> create() {
#if defined(__linux__)
    auto uring = std::make_unique<async_uring>();
    if (uring->valid()) {
        return uring;
    }
    return std::make_unique<async_epoll>();
#else
    return std::make_unique<async>();
#endif
}

}  // namespace bee::async
