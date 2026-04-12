#include <bee/async/async_osx.h>

#include <bee/async/async.h>
#include <bee/net/endpoint.h>
#include <bee/net/socket.h>

#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace bee::async {

async::async()
    : m_queue(dispatch_queue_create("bee.async.gcd", DISPATCH_QUEUE_SERIAL))
    , m_signal(dispatch_semaphore_create(0))
    , m_stopped(false) {}

async::~async() {
    stop();
}

// Called on m_queue — no lock needed.
void async::enqueue_completion(const io_completion& c) {
    m_completions.push_back(c);
    dispatch_semaphore_signal(m_signal);
}

// Called on m_queue via dispatch_sync — no lock needed.
int async::drain(const span<io_completion>& completions) {
    int count = 0;
    while (!m_completions.empty() && count < static_cast<int>(completions.size())) {
        completions[count++] = m_completions.front();
        m_completions.pop_front();
    }
    return count;
}

bool async::submit_read(net::fd_t fd, void* buffer, size_t len, uint64_t request_id) {
    auto* op       = new pending_op();
    op->request_id = request_id;
    op->fd         = fd;
    op->type       = pending_op::read;
    op->r.buffer   = buffer;
    op->r.len      = len;

    dispatch_source_t source = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, fd, 0, m_queue);
    if (!source) {
        delete op;
        return false;
    }

    dispatch_source_set_event_handler(source, ^{
        int rc = 0;
        auto rs = net::socket::recv(fd, rc, static_cast<char*>(op->r.buffer), static_cast<int>(op->r.len));
        io_completion c;
        c.request_id = op->request_id;
        c.op         = async_op::read;
        switch (rs) {
        case net::socket::recv_status::success:
            c.status            = async_status::success;
            c.bytes_transferred = static_cast<size_t>(rc);
            c.error_code        = 0;
            break;
        case net::socket::recv_status::close:
            c.status            = async_status::close;
            c.bytes_transferred = 0;
            c.error_code        = 0;
            break;
        case net::socket::recv_status::failed:
            c.status            = async_status::error;
            c.bytes_transferred = 0;
            c.error_code        = errno;
            break;
        case net::socket::recv_status::wait:
            return;
        }
        dispatch_source_cancel(source);
        this->enqueue_completion(c);
        delete op;
    });

    dispatch_source_set_cancel_handler(source, ^{
        dispatch_release(source);
    });

    dispatch_resume(source);
    return true;
}

bool async::submit_write(net::fd_t fd, const void* buffer, size_t len, uint64_t request_id) {
    auto* op       = new pending_op();
    op->request_id = request_id;
    op->fd         = fd;
    op->type       = pending_op::write;
    op->w.buffer   = buffer;
    op->w.len      = len;

    dispatch_source_t source = dispatch_source_create(DISPATCH_SOURCE_TYPE_WRITE, fd, 0, m_queue);
    if (!source) {
        delete op;
        return false;
    }

    dispatch_source_set_event_handler(source, ^{
        int rc = 0;
        auto ss = net::socket::send(fd, rc, static_cast<const char*>(op->w.buffer), static_cast<int>(op->w.len));
        io_completion c;
        c.request_id = op->request_id;
        c.op         = async_op::write;
        switch (ss) {
        case net::socket::status::success:
            c.status            = async_status::success;
            c.bytes_transferred = static_cast<size_t>(rc);
            c.error_code        = 0;
            break;
        case net::socket::status::wait:
            return;
        case net::socket::status::failed:
            c.status            = async_status::error;
            c.bytes_transferred = 0;
            c.error_code        = errno;
            break;
        }
        dispatch_source_cancel(source);
        this->enqueue_completion(c);
        delete op;
    });

    dispatch_source_set_cancel_handler(source, ^{
        dispatch_release(source);
    });

    dispatch_resume(source);
    return true;
}

bool async::submit_accept(net::fd_t listen_fd, uint64_t request_id) {
    auto* op       = new pending_op();
    op->request_id = request_id;
    op->fd         = listen_fd;
    op->type       = pending_op::accept;

    dispatch_source_t source = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, listen_fd, 0, m_queue);
    if (!source) {
        delete op;
        return false;
    }

    dispatch_source_set_event_handler(source, ^{
        net::fd_t newfd = net::retired_fd;
        auto as    = net::socket::accept(listen_fd, newfd);
        io_completion c;
        c.request_id = op->request_id;
        c.op         = async_op::accept;
        switch (as) {
        case net::socket::status::success:
            c.status            = async_status::success;
            c.bytes_transferred = static_cast<size_t>(newfd);
            c.error_code        = 0;
            break;
        case net::socket::status::wait:
            return;
        case net::socket::status::failed:
            c.status            = async_status::error;
            c.bytes_transferred = 0;
            c.error_code        = errno;
            break;
        }
        dispatch_source_cancel(source);
        this->enqueue_completion(c);
        delete op;
    });

    dispatch_source_set_cancel_handler(source, ^{
        dispatch_release(source);
    });

    dispatch_resume(source);
    return true;
}

bool async::submit_connect(net::fd_t fd, const net::endpoint& ep, uint64_t request_id) {
    auto status = net::socket::connect(fd, ep);
    if (status == net::socket::status::success) {
        // Immediate success: enqueue directly on m_queue.
        dispatch_async(m_queue, ^{
            io_completion c;
            c.request_id        = request_id;
            c.op                = async_op::connect;
            c.status            = async_status::success;
            c.bytes_transferred = 0;
            c.error_code        = 0;
            this->enqueue_completion(c);
        });
        return true;
    }
    if (status == net::socket::status::wait) {
        auto* op       = new pending_op();
        op->request_id = request_id;
        op->fd         = fd;
        op->type       = pending_op::connect;

        dispatch_source_t source = dispatch_source_create(DISPATCH_SOURCE_TYPE_WRITE, fd, 0, m_queue);
        if (!source) {
            delete op;
            return false;
        }

        dispatch_source_set_event_handler(source, ^{
            int err = 0;
            io_completion c;
            c.request_id = op->request_id;
            c.op         = async_op::connect;
            if (net::socket::errcode(fd, err) && err == 0) {
                c.status            = async_status::success;
                c.bytes_transferred = 0;
                c.error_code        = 0;
            } else {
                c.status            = async_status::error;
                c.bytes_transferred = 0;
                c.error_code        = (err != 0) ? err : errno;
            }
            dispatch_source_cancel(source);
            this->enqueue_completion(c);
            delete op;
        });

        dispatch_source_set_cancel_handler(source, ^{
            dispatch_release(source);
        });

        dispatch_resume(source);
        return true;
    }
    return false;
}

bool async::submit_file_read(file_handle::value_type fd, void* buffer, size_t len, int64_t offset, uint64_t request_id) {
    dispatch_async(m_queue, ^{
        io_completion c;
        c.request_id = request_id;
        c.op         = async_op::file_read;
        ssize_t n    = pread(fd, buffer, len, offset);
        if (n >= 0) {
            c.status            = async_status::success;
            c.bytes_transferred = static_cast<size_t>(n);
            c.error_code        = 0;
        } else {
            c.status            = async_status::error;
            c.bytes_transferred = 0;
            c.error_code        = errno;
        }
        this->enqueue_completion(c);
    });
    return true;
}

bool async::submit_file_write(file_handle::value_type fd, const void* buffer, size_t len, int64_t offset, uint64_t request_id) {
    dispatch_async(m_queue, ^{
        io_completion c;
        c.request_id = request_id;
        c.op         = async_op::file_write;
        ssize_t n    = pwrite(fd, buffer, len, offset);
        if (n >= 0) {
            c.status            = async_status::success;
            c.bytes_transferred = static_cast<size_t>(n);
            c.error_code        = 0;
        } else {
            c.status            = async_status::error;
            c.bytes_transferred = 0;
            c.error_code        = errno;
        }
        this->enqueue_completion(c);
    });
    return true;
}

int async::poll(const span<io_completion>& completions) {
    // dispatch_sync runs drain on m_queue, serializing with enqueue_completion.
    __block int count = 0;
    dispatch_sync(m_queue, ^{
        count = drain(completions);
    });
    // Consume any semaphore signals corresponding to drained completions.
    for (int i = 0; i < count; ++i) {
        dispatch_semaphore_wait(m_signal, DISPATCH_TIME_NOW);
    }
    return count;
}

int async::wait(const span<io_completion>& completions, int timeout) {
    dispatch_time_t when = (timeout < 0)
        ? DISPATCH_TIME_FOREVER
        : dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(timeout) * NSEC_PER_MSEC);
    // Block until at least one completion is available.
    if (dispatch_semaphore_wait(m_signal, when) != 0) {
        return 0;  // timed out
    }
    // One signal consumed; drain all available completions via dispatch_sync.
    __block int count = 0;
    dispatch_sync(m_queue, ^{
        count = drain(completions);
    });
    // We already consumed one signal above; consume the rest for drained items.
    for (int i = 1; i < count; ++i) {
        dispatch_semaphore_wait(m_signal, DISPATCH_TIME_NOW);
    }
    return count;
}

void async::stop() {
    if (!m_stopped) {
        m_stopped = true;
        if (m_queue) {
            dispatch_release(m_queue);
            m_queue = nullptr;
        }
        if (m_signal) {
            dispatch_release(m_signal);
            m_signal = nullptr;
        }
    }
}

}  // namespace bee::async
