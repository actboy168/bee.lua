#include <bee/async/async_bsd.h>

#include <bee/async/async.h>
#include <bee/net/endpoint.h>
#include <bee/net/socket.h>

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace bee::async {

async::async()
    : m_kqfd(-1) {
    m_kqfd = kqueue();
}

async::~async() {
    stop();
}

bool async::kqueue_register(net::fd_t fd, int filter, pending_op* op) {
    struct kevent ev;
    EV_SET(&ev, fd, filter, EV_ADD | EV_ONESHOT, 0, 0, op);
    return kevent(m_kqfd, &ev, 1, nullptr, 0, nullptr) == 0;
}

bool async::submit_read(net::fd_t fd, void* buffer, size_t len, uint64_t request_id) {
    auto* op       = new pending_op();
    op->request_id = request_id;
    op->fd         = fd;
    op->type       = pending_op::read;
    op->r.buffer   = buffer;
    op->r.len      = len;
    if (!kqueue_register(fd, EVFILT_READ, op)) {
        delete op;
        return false;
    }
    return true;
}

bool async::submit_write(net::fd_t fd, const void* buffer, size_t len, uint64_t request_id) {
    auto* op       = new pending_op();
    op->request_id = request_id;
    op->fd         = fd;
    op->type       = pending_op::write;
    op->w.buffer   = buffer;
    op->w.len      = len;
    if (!kqueue_register(fd, EVFILT_WRITE, op)) {
        delete op;
        return false;
    }
    return true;
}

bool async::submit_accept(net::fd_t listen_fd, uint64_t request_id) {
    auto* op       = new pending_op();
    op->request_id = request_id;
    op->fd         = listen_fd;
    op->type       = pending_op::accept;
    if (!kqueue_register(listen_fd, EVFILT_READ, op)) {
        delete op;
        return false;
    }
    return true;
}

bool async::submit_connect(net::fd_t fd, const net::endpoint& ep, uint64_t request_id) {
    auto status = net::socket::connect(fd, ep);
    if (status == net::socket::status::success) {
        io_completion c;
        c.request_id        = request_id;
        c.op                = async_op::connect;
        c.status            = async_status::success;
        c.bytes_transferred = 0;
        c.error_code        = 0;
        m_sync_completions.push_back(c);
        return true;
    }
    if (status == net::socket::status::wait) {
        auto* op       = new pending_op();
        op->request_id = request_id;
        op->fd         = fd;
        op->type       = pending_op::connect;
        if (!kqueue_register(fd, EVFILT_WRITE, op)) {
            delete op;
            return false;
        }
        return true;
    }
    return false;
}

bool async::submit_file_read(file_handle::value_type fd, void* buffer, size_t len, int64_t offset, uint64_t request_id) {
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
    m_sync_completions.push_back(c);
    return true;
}

bool async::submit_file_write(file_handle::value_type fd, const void* buffer, size_t len, int64_t offset, uint64_t request_id) {
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
    m_sync_completions.push_back(c);
    return true;
}

static io_completion handle_event(struct kevent& ev) {
    auto* op = static_cast<async::pending_op*>(ev.udata);
    io_completion c;
    c.request_id        = op->request_id;
    c.status            = async_status::error;
    c.bytes_transferred = 0;
    c.error_code        = 0;

    net::fd_t fd = op->fd;

    // Check for error flag first (applies to all filter types).
    if (ev.flags & EV_ERROR) {
        c.error_code = static_cast<int>(ev.data);
        switch (op->type) {
        case async::pending_op::read:    c.op = async_op::read;    break;
        case async::pending_op::write:   c.op = async_op::write;   break;
        case async::pending_op::accept:  c.op = async_op::accept;  break;
        case async::pending_op::connect: c.op = async_op::connect; break;
        }
        delete op;
        return c;
    }

    switch (op->type) {
    case async::pending_op::read: {
        c.op = async_op::read;
        // EOF on EVFILT_READ: data == 0 and EV_EOF set.
        if ((ev.flags & EV_EOF) && ev.data == 0) {
            c.status = async_status::close;
            break;
        }
        int rc = 0;
        auto rs = net::socket::recv(fd, rc, static_cast<char*>(op->r.buffer), static_cast<int>(op->r.len));
        switch (rs) {
        case net::socket::recv_status::success:
            c.status            = async_status::success;
            c.bytes_transferred = static_cast<size_t>(rc);
            break;
        case net::socket::recv_status::close:
            c.status = async_status::close;
            break;
        case net::socket::recv_status::failed:
            c.status     = async_status::error;
            c.error_code = errno;
            break;
        case net::socket::recv_status::wait:
            c.status     = async_status::error;
            c.error_code = EAGAIN;
            break;
        }
        break;
    }
    case async::pending_op::write: {
        c.op = async_op::write;
        int rc = 0;
        auto ss = net::socket::send(fd, rc, static_cast<const char*>(op->w.buffer), static_cast<int>(op->w.len));
        switch (ss) {
        case net::socket::status::success:
            c.status            = async_status::success;
            c.bytes_transferred = static_cast<size_t>(rc);
            break;
        case net::socket::status::wait:
            c.status     = async_status::error;
            c.error_code = EAGAIN;
            break;
        case net::socket::status::failed:
            c.status     = async_status::error;
            c.error_code = errno;
            break;
        }
        break;
    }
    case async::pending_op::accept: {
        c.op = async_op::accept;
        net::fd_t newfd = net::retired_fd;
        auto as = net::socket::accept(fd, newfd);
        switch (as) {
        case net::socket::status::success:
            c.status            = async_status::success;
            c.bytes_transferred = static_cast<size_t>(newfd);
            break;
        case net::socket::status::wait:
            c.status     = async_status::error;
            c.error_code = EAGAIN;
            break;
        case net::socket::status::failed:
            c.status     = async_status::error;
            c.error_code = errno;
            break;
        }
        break;
    }
    case async::pending_op::connect: {
        c.op = async_op::connect;
        int err = 0;
        if (net::socket::errcode(fd, err) && err == 0) {
            c.status = async_status::success;
        } else {
            c.status     = async_status::error;
            c.error_code = (err != 0) ? err : errno;
        }
        break;
    }
    }

    delete op;
    return c;
}

static int drain_kqueue(int kqfd, const span<io_completion>& completions, int timeout_ms) {
    struct kevent events[async::kMaxEvents];
    int nev;
    if (timeout_ms < 0) {
        nev = kevent(kqfd, nullptr, 0, events, async::kMaxEvents, nullptr);
    } else {
        struct timespec ts;
        ts.tv_sec  = timeout_ms / 1000;
        ts.tv_nsec = (timeout_ms % 1000) * 1000000L;
        nev = kevent(kqfd, nullptr, 0, events, async::kMaxEvents, &ts);
    }
    if (nev <= 0) {
        return 0;
    }
    int count = 0;
    for (int i = 0; i < nev && count < static_cast<int>(completions.size()); ++i) {
        completions[count++] = handle_event(events[i]);
    }
    return count;
}

int async::poll(const span<io_completion>& completions) {
    int count = 0;

    while (!m_sync_completions.empty() && count < static_cast<int>(completions.size())) {
        completions[count++] = m_sync_completions.front();
        m_sync_completions.erase(m_sync_completions.begin());
    }
    if (count >= static_cast<int>(completions.size())) {
        return count;
    }

    count += drain_kqueue(m_kqfd, span<io_completion>(completions.data() + count, completions.size() - count), 0);
    return count;
}

int async::wait(const span<io_completion>& completions, int timeout) {
    int count = 0;

    while (!m_sync_completions.empty() && count < static_cast<int>(completions.size())) {
        completions[count++] = m_sync_completions.front();
        m_sync_completions.erase(m_sync_completions.begin());
    }
    if (count > 0 || completions.size() == 0) {
        return count;
    }

    count += drain_kqueue(m_kqfd, completions, timeout);
    return count;
}

void async::stop() {
    if (m_kqfd >= 0) {
        close(m_kqfd);
        m_kqfd = -1;
    }
}

}  // namespace bee::async
