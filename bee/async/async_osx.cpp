#include <bee/async/async.h>
#include <bee/async/async_osx.h>
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

    void async::enqueue_completion(const io_completion& c) {
        m_completions.push_back(c);
        dispatch_semaphore_signal(m_signal);
    }

    int async::drain(const span<io_completion>& completions) {
        int count = 0;
        while (!m_completions.empty() && count < static_cast<int>(completions.size())) {
            completions[count++] = m_completions.front();
            m_completions.pop_front();
        }
        return count;
    }

    // Get or create per-fd sources. Called from Lua thread before submit.
    async::fd_sources* async::get_or_create(net::fd_t fd) {
        auto it = m_fd_map.find(fd);
        if (it != m_fd_map.end()) return it->second;

        auto* s      = new fd_sources();
        m_fd_map[fd] = s;

        // Create persistent read source (starts suspended).
        s->read_src = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, fd, 0, m_queue);
        if (!s->read_src) { delete s; m_fd_map.erase(fd); return nullptr; }
        dispatch_source_set_event_handler(s->read_src, ^{
            if (!s->r.pending) return;
            io_completion c;
            c.request_id = s->r.request_id;
            c.error_code = 0;
            c.op    = async_op::read;
            int rc  = 0;
            auto rs = net::socket::recvv(fd, rc, span<net::socket::iobuf>(s->r.iov.data(), s->r.iov.size()));
            switch (rs) {
            case net::socket::recv_status::success:
                c.status            = async_status::success;
                c.bytes_transferred = static_cast<size_t>(rc);
                break;
            case net::socket::recv_status::close:
                c.status            = async_status::close;
                c.bytes_transferred = 0;
                break;
            case net::socket::recv_status::wait:
                return;  // spurious wakeup, stay resumed
            case net::socket::recv_status::failed:
                c.status            = async_status::error;
                c.bytes_transferred = 0;
                c.error_code        = errno;
                break;
            }
            s->r.pending = false;
            dispatch_suspend(s->read_src);
            this->enqueue_completion(c);
        });
        dispatch_source_set_cancel_handler(s->read_src, ^{
            dispatch_release(s->read_src);
            s->read_src = nullptr;
        });

        // Create persistent write source (starts suspended).
        s->write_src = dispatch_source_create(DISPATCH_SOURCE_TYPE_WRITE, fd, 0, m_queue);
        if (!s->write_src) {
            dispatch_source_cancel(s->read_src);
            delete s; m_fd_map.erase(fd); return nullptr;
        }
        dispatch_source_set_event_handler(s->write_src, ^{
            if (!s->w.pending) return;
            io_completion c;
            c.request_id = s->w.request_id;
            c.error_code = 0;
            c.op    = async_op::write;
            int rc  = 0;
            auto ss = net::socket::sendv(fd, rc, span<const net::socket::iobuf>(s->w.iov.data(), s->w.iov.size()));
            switch (ss) {
            case net::socket::status::success:
                c.status            = async_status::success;
                c.bytes_transferred = static_cast<size_t>(rc);
                break;
            case net::socket::status::wait:
                return;
            case net::socket::status::failed:
                c.status            = async_status::error;
                c.bytes_transferred = 0;
                c.error_code        = errno;
                break;
            }
            s->w.pending = false;
            dispatch_suspend(s->write_src);
            this->enqueue_completion(c);
        });
        dispatch_source_set_cancel_handler(s->write_src, ^{
            dispatch_release(s->write_src);
            s->write_src = nullptr;
        });

        // Both sources start suspended; resume only on demand.
        return s;
    }

    void async::release_fd(net::fd_t fd) {
        auto it = m_fd_map.find(fd);
        if (it == m_fd_map.end()) return;
        fd_sources* s = it->second;
        m_fd_map.erase(it);
        // Cancel sources; cancel handlers will release and set ptr to nullptr.
        if (s->read_src) {
            // Must resume before cancel if suspended, otherwise cancel blocks.
            if (!s->r.pending) dispatch_resume(s->read_src);
            dispatch_source_cancel(s->read_src);
        }
        if (s->write_src) {
            if (!s->w.pending) dispatch_resume(s->write_src);
            dispatch_source_cancel(s->write_src);
        }
        // Delete the fd_sources struct after dispatch finishes on m_queue.
        dispatch_async(m_queue, ^{ delete s; });
    }

    bool async::associate(net::fd_t fd) {
        return get_or_create(fd) != nullptr;
    }

    bool async::submit_read(net::fd_t fd, span<const net::socket::iobuf> bufs, uint64_t request_id) {
        fd_sources* s = get_or_create(fd);
        if (!s || s->r.pending) return false;
        s->r.iov        = dynarray<net::socket::iobuf>(bufs.size());
        for (size_t i = 0; i < bufs.size(); ++i) s->r.iov[i] = bufs[i];
        s->r.request_id = request_id;
        s->r.pending    = true;
        dispatch_resume(s->read_src);
        return true;
    }

    bool async::submit_write(net::fd_t fd, span<const net::socket::iobuf> bufs, uint64_t request_id) {
        fd_sources* s = get_or_create(fd);
        if (!s || s->w.pending) return false;
        s->w.iov        = dynarray<net::socket::iobuf>(bufs.size());
        for (size_t i = 0; i < bufs.size(); ++i) s->w.iov[i] = bufs[i];
        s->w.request_id = request_id;
        s->w.pending    = true;
        dispatch_resume(s->write_src);
        return true;
    }

    bool async::submit_accept(net::fd_t listen_fd, uint64_t request_id) {
        fd_sources* s = get_or_create(listen_fd);
        if (!s || s->r.pending) return false;
        // accept uses a one-shot source (rare operation)

        dispatch_source_t src = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, listen_fd, 0, m_queue);
        if (!src) return false;
        dispatch_source_set_event_handler(src, ^{
            net::fd_t newfd = net::retired_fd;
            auto as         = net::socket::accept(listen_fd, newfd);
            io_completion c;
            c.request_id = request_id;
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
            dispatch_source_cancel(src);
            this->enqueue_completion(c);
        });
        dispatch_source_set_cancel_handler(src, ^{ dispatch_release(src); });
        dispatch_resume(src);
        return true;
    }

    bool async::submit_connect(net::fd_t fd, const net::endpoint& ep, uint64_t request_id) {
        auto status = net::socket::connect(fd, ep);
        if (status == net::socket::status::success) {
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
            // Connect completion comes as write-ready; use one-shot (rare).
            dispatch_source_t src = dispatch_source_create(DISPATCH_SOURCE_TYPE_WRITE, fd, 0, m_queue);
            if (!src) return false;
            dispatch_source_set_event_handler(src, ^{
                int err = 0;
                io_completion c;
                c.request_id = request_id;
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
                dispatch_source_cancel(src);
                this->enqueue_completion(c);
            });
            dispatch_source_set_cancel_handler(src, ^{ dispatch_release(src); });
            dispatch_resume(src);
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

    void async::cancel(net::fd_t fd) {
        release_fd(fd);
    }

    bool async::submit_poll(net::fd_t fd, uint64_t request_id) {
        dispatch_source_t source = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, fd, 0, m_queue);
        if (!source) {
            return false;
        }

        dispatch_source_set_event_handler(source, ^{
          // fd_poll: 只通知 fd 可读，不消费任何数据
          io_completion c;
          c.request_id        = request_id;
          c.op                = async_op::fd_poll;
          c.status            = async_status::success;
          c.bytes_transferred = 0;
          c.error_code        = 0;
          dispatch_source_cancel(source);
          this->enqueue_completion(c);
        });

        dispatch_source_set_cancel_handler(source, ^{
          dispatch_release(source);
        });

        dispatch_resume(source);
        return true;
    }

    int async::poll(const span<io_completion>& completions) {
        __block int count = 0;
        dispatch_sync(m_queue, ^{ count = drain(completions); });
        for (int i = 0; i < count; ++i)
            dispatch_semaphore_wait(m_signal, DISPATCH_TIME_NOW);
        return count;
    }

    int async::wait(const span<io_completion>& completions, int timeout) {
        dispatch_time_t when = (timeout < 0)
            ? DISPATCH_TIME_FOREVER
            : dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(timeout) * NSEC_PER_MSEC);
        if (dispatch_semaphore_wait(m_signal, when) != 0) return 0;
        __block int count = 0;
        dispatch_sync(m_queue, ^{ count = drain(completions); });
        for (int i = 1; i < count; ++i)
            dispatch_semaphore_wait(m_signal, DISPATCH_TIME_NOW);
        return count;
    }

    void async::stop() {
        if (!m_stopped) {
            m_stopped = true;
            // Cancel all fd sources.
            for (auto& [fd, s] : m_fd_map) {
                if (s->read_src)  { if (!s->r.pending) dispatch_resume(s->read_src);  dispatch_source_cancel(s->read_src);  }
                if (s->write_src) { if (!s->w.pending) dispatch_resume(s->write_src); dispatch_source_cancel(s->write_src); }
            }
            if (m_queue) {
                dispatch_sync(m_queue, ^{
                    for (auto& [fd, s] : m_fd_map) delete s;
                    m_fd_map.clear();
                });
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
