#pragma once

#include <bee/async/async_types.h>
#include <bee/net/endpoint.h>
#include <bee/sys/file_handle.h>
#include <bee/utility/span.h>

#include <cstddef>
#include <cstdint>
#include <memory>

#if defined(__linux__)
// Only include platform headers where they are strictly needed.
// async_uring_linux.h and async_epoll_linux.h inherit from the abstract
// base defined here; they are not needed by consumers of async.h.
#elif defined(_WIN32)
#    include <bee/async/async_win.h>
#elif defined(__APPLE__)
#    include <bee/async/async_macos.h>
#else
#    error "Unsupported platform for bee::async"
#endif

namespace bee::async {

#if defined(__linux__)

class async {
public:
    virtual ~async()                                                                                                                      = default;
    virtual bool submit_read(net::fd_t fd, void* buffer, size_t len, uint64_t request_id)                                               = 0;
    virtual bool submit_write(net::fd_t fd, const void* buffer, size_t len, uint64_t request_id)                                        = 0;
    virtual bool submit_accept(net::fd_t listen_fd, uint64_t request_id)                                                                = 0;
    virtual bool submit_connect(net::fd_t fd, const net::endpoint& ep, uint64_t request_id)                                             = 0;
    virtual bool submit_file_read(file_handle::value_type fd, void* buffer, size_t len, int64_t offset, uint64_t request_id)        = 0;
    virtual bool submit_file_write(file_handle::value_type fd, const void* buffer, size_t len, int64_t offset, uint64_t request_id) = 0;
    virtual int  poll(const span<io_completion>& completions)                                                                           = 0;
    virtual int  wait(const span<io_completion>& completions, int timeout)                                                               = 0;
    virtual void stop()                                                                                                                  = 0;
};

#endif

// Factory function: create the async backend for the current platform.
std::unique_ptr<async> create();

}  // namespace bee::async
