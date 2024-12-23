#include <bee/filewatch/filewatch.h>
#include <bee/nonstd/filesystem.h>
#include <bee/nonstd/unreachable.h>
#include <poll.h>
#include <sys/inotify.h>
#include <unistd.h>

#include <cassert>
#include <cstddef>
#include <functional>

namespace bee::filewatch {
    watch::watch() noexcept
        : m_notify()
        , m_fd_path()
        , m_inotify_fd(inotify_init1(IN_NONBLOCK | IN_CLOEXEC)) {
        assert(m_inotify_fd != -1);
    }

    watch::~watch() noexcept {
        stop();
    }

    void watch::stop() noexcept {
        if (m_inotify_fd == -1) {
            return;
        }
        for (auto& [desc, _] : m_fd_path) {
            (void)_;
            inotify_rm_watch(m_inotify_fd, desc);
        }
        m_fd_path.clear();
        close(m_inotify_fd);
        m_inotify_fd = -1;
    }

    void watch::add(const string_type& str) noexcept {
        if (m_inotify_fd == -1) {
            return;
        }
        if (!m_filter(str.c_str())) {
            return;
        }
        fs::path path = str;
        if (m_follow_symlinks) {
            std::error_code ec;
            path = fs::canonical(path, ec);
            if (ec) {
                return;
            }
        }
        int desc = inotify_add_watch(m_inotify_fd, path.c_str(), IN_ALL_EVENTS);
        if (desc != -1) {
            const auto& emplace_result = m_fd_path.emplace(std::make_pair(desc, path.string()));
            if (!emplace_result.second) {
                return;
            }
        }
        if (!m_recursive) {
            return;
        }
        std::error_code ec;
        fs::directory_iterator iter { path, fs::directory_options::skip_permission_denied, ec };
        fs::directory_iterator end {};
        for (; !ec && iter != end; iter.increment(ec)) {
            std::error_code file_status_ec;
            if (fs::is_directory(m_follow_symlinks ? iter->status(file_status_ec) : iter->symlink_status(file_status_ec))) {
                add(iter->path());
            }
        }
    }

    void watch::set_recursive(bool enable) noexcept {
        m_recursive = enable;
    }

    bool watch::set_follow_symlinks(bool enable) noexcept {
        m_follow_symlinks = enable;
        return true;
    }

    bool watch::set_filter(filter f) noexcept {
        m_filter = f;
        return true;
    }

    void watch::event_update(void* e) noexcept {
        inotify_event* event = (inotify_event*)e;
        if (event->mask & IN_Q_OVERFLOW) {
            // TODO?
        }

        auto filename = m_fd_path[event->wd];
        if (event->len > 1) {
            filename += "/";
            filename += std::string(event->name);
        }
        if (event->mask & (IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO)) {
            m_notify.emplace(notify::flag::rename, filename);
        } else if (event->mask & (IN_MOVE_SELF | IN_ATTRIB | IN_CLOSE_WRITE | IN_MODIFY)) {
            m_notify.emplace(notify::flag::modify, filename);
        }

        if (event->mask & (IN_IGNORED | IN_DELETE_SELF)) {
            m_fd_path.erase(event->wd);
        }
        if (event->mask & IN_MOVE_SELF) {
            inotify_rm_watch(m_inotify_fd, event->wd);
            m_fd_path.erase(event->wd);
        }
        if (m_recursive && (event->mask & IN_ISDIR) && (event->mask & IN_CREATE)) {
            add(filename);
        }
    }

    std::optional<notify> watch::select() noexcept {
        do {
            if (m_inotify_fd == -1) {
                break;
            }

            struct pollfd pfd_read;
            pfd_read.fd     = m_inotify_fd;
            pfd_read.events = POLLIN;
            if (poll(&pfd_read, 1, 0) != 1) {
                break;
            }

            std::byte buf[4096];
            ssize_t n = read(m_inotify_fd, buf, sizeof buf);
            if (n == 0 || n == -1) {
                break;
            }
            for (std::byte* p = buf; p < buf + n;) {
                auto event = (struct inotify_event*)p;
                event_update(event);
                p += sizeof(*event) + event->len;
            }
        } while (false);

        if (m_notify.empty()) {
            return std::nullopt;
        }
        auto msg = m_notify.front();
        m_notify.pop();
        return msg;
    }
}
