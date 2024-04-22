#include <Windows.h>
#include <bee/nonstd/unreachable.h>
#include <bee/utility/hybrid_array.h>
#include <bee/win/afd/afd.h>
#include <bee/win/afd/poller.h>
#include <bee/win/afd/poller_fd.h>

namespace bee::net::afd {
    poller::poller(afd_context&& afd) noexcept
        : afd(std::move(afd)) {
    }

    poller::~poller() noexcept {
        afd_destroy(afd);
        for (const auto& [_, fd] : fds) {
            delete fd;
        }
    }

    int poller::wait(const span<bpoll_event_t>& events, int timeout) noexcept {
        uint64_t deadline  = timeout > 0 ? (GetTickCount64() + (uint64_t)timeout) : 0;
        uint32_t u_timeout = timeout < 0 ? INFINITE : (uint32_t)timeout;
        hybrid_array<OVERLAPPED_ENTRY, 256> iocp_events(events.size());
        for (;;) {
            for (auto iter = update_queue.begin(); iter != update_queue.end(); ++iter) {
                poller_fd* fd = *iter;
                switch (fd->update(afd)) {
                case poller_fd::update_status::succee:
                    break;
                case poller_fd::update_status::close:
                    destory_fd(fd);
                    break;
                case poller_fd::update_status::failed: {
                    flatset<poller_fd*> queue;
                    for (; iter != update_queue.end(); ++iter) {
                        queue.insert(*iter);
                    }
                    std::swap(queue, update_queue);
                    return -1;
                }
                default:
                    std::unreachable();
                }
            }
            update_queue.clear();
            uint32_t count = afd_wait(afd, iocp_events, u_timeout);
            if (count == (uint32_t)-1) {
                if (GetLastError() == WAIT_TIMEOUT) {
                    return 0;
                }
                return -1;
            }
            int epoll_event_count = 0;
            for (uint32_t i = 0; i < count; ++i) {
                poller_fd* fd               = (poller_fd*)iocp_events[i].lpOverlapped;
                auto [status, epoll_events] = fd->feed_event(events[epoll_event_count]);
                switch (status) {
                case afd_poll_status::succeeded:
                case afd_poll_status::failed:
                    request_update(fd);
                    if (epoll_events != bpoll_event::null) {
                        epoll_event_count++;
                    }
                    break;
                case afd_poll_status::cancelled:
                case afd_poll_status::nothing:
                    request_update(fd);
                    break;
                case afd_poll_status::closed:
                    destory_fd(fd);
                    break;
                default:
                    std::unreachable();
                }
            }
            if (epoll_event_count > 0) {
                return epoll_event_count;
            }
            if (timeout < 0) {
                continue;
            }
            uint64_t now = GetTickCount64();
            if (now >= deadline) {
                SetLastError(WAIT_TIMEOUT);
                return 0;
            }
            u_timeout = (uint32_t)(deadline - now);
        }
    }

    bool poller::ctl_add(fd_t socket, const bpoll_event_t& ev) noexcept {
        if (fds.contains(socket)) {
            SetLastError(ERROR_ALREADY_EXISTS);
            return false;
        }
        fd_t base_socket = afd_get_base_socket(socket);
        if (base_socket == INVALID_SOCKET) {
            return false;
        }
        poller_fd* fd = new (std::nothrow) poller_fd(socket, base_socket);
        if (fd == NULL) {
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return false;
        }
        fds.insert(socket, fd);
        if (fd->set_event(ev)) {
            request_update(fd);
        }
        return true;
    }

    bool poller::ctl_mod(fd_t socket, const bpoll_event_t& ev) noexcept {
        auto pfd = fds.find(socket);
        if (!pfd) {
            SetLastError(ERROR_NOT_FOUND);
            return false;
        }
        poller_fd* fd = *pfd;
        if (fd->set_event(ev)) {
            request_update(fd);
        }
        return true;
    }

    bool poller::ctl_del(fd_t socket) noexcept {
        auto pfd = fds.find(socket);
        if (!pfd) {
            SetLastError(ERROR_NOT_FOUND);
            return false;
        }
        poller_fd* fd = *pfd;
        destory_fd(fd);
        return true;
    }

    void poller::request_update(poller_fd* fd) noexcept {
        update_queue.insert(fd);
    }

    void poller::destory_fd(poller_fd* fd) noexcept {
        switch (fd->destroy(afd)) {
        case poller_fd::destroy_status::nothing:
            break;
        case poller_fd::destroy_status::unregister:
            update_queue.erase(fd);
            fds.erase(fd->get_socket());
            break;
        case poller_fd::destroy_status::destroy:
            delete fd;
            break;
        case poller_fd::destroy_status::unregister_then_destroy:
            update_queue.erase(fd);
            fds.erase(fd->get_socket());
            delete fd;
            break;
        default:
            std::unreachable();
        }
    }
}
