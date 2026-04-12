#include <bee/async/async.h>

#if defined(__linux__)
#    include <bee/async/async_epoll_linux.h>
#    include <bee/async/async_uring_linux.h>
#    include <linux/io_uring.h>
#    include <sys/syscall.h>
#    include <unistd.h>
#    include <cstring>
#endif

namespace bee::async {

#if defined(__linux__)
static bool io_uring_available() {
    struct io_uring_params params;
    memset(&params, 0, sizeof(params));
    long rc = syscall(__NR_io_uring_setup, 1, &params);
    if (rc >= 0) {
        close(static_cast<int>(rc));
        return true;
    }
    return false;
}
#endif

std::unique_ptr<async> create() {
#if defined(__linux__)
    if (io_uring_available()) {
        return std::make_unique<async_uring>();
    }
    return std::make_unique<async_epoll>();
#elif defined(_WIN32) || defined(__APPLE__)
    return std::make_unique<async>();
#else
#    error "Unsupported platform for bee::async"
#endif
}

}  // namespace bee::async
