#include <bee/filewatch/filewatch.h>
#include <bee/error.h>
#include <bee/nonstd/unreachable.h>
#include <bee/nonstd/filesystem.h>
#include <assert.h>
#include <functional>
#include <unistd.h>
#include <sys/inotify.h>
#include <poll.h>

namespace bee::filewatch {
    const char* watch::type() {
        return "inotify";
    }

    watch::watch()
        : m_notify()
        , m_fd_path()
        , m_inotify_fd(inotify_init1(IN_NONBLOCK | IN_CLOEXEC))
    {
        assert(m_inotify_fd != -1);
    }

    watch::~watch() {
        stop();
    }

    void   watch::stop() {
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

    void watch::add(const string_type& str) {
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
            m_fd_path.emplace(std::make_pair(desc, path.string()));
        }
        if (!m_recursive) {
            return;
        }
        std::error_code ec;
        fs::directory_iterator iter {path, fs::directory_options::skip_permission_denied, ec };
        fs::directory_iterator end {};
        for (; !ec && iter != end; iter.increment(ec)) {
            if (fs::is_directory(m_follow_symlinks? iter->status(): iter->symlink_status())) {
                add(iter->path());
            }
        }
    }

    void watch::set_recursive(bool enable) {
        m_recursive = enable;
    }

    bool watch::set_follow_symlinks(bool enable) {
        m_follow_symlinks = enable;
        return true;
    }

    bool watch::set_filter(filter f) {
        m_filter = f;
        return true;
    }

    void watch::update() {
        if (m_inotify_fd == -1) {
            return;
        }

        struct pollfd pfd_read;
        pfd_read.fd = m_inotify_fd;
        pfd_read.events = POLLIN;
        if (poll(&pfd_read, 1, 0) != 1) {
            return;
        }

        char buf[4096];
        ssize_t n = read(m_inotify_fd, buf, sizeof buf);
        if (n == 0 || n == -1) {
            return;
        }
        for (char *p = buf; p < buf + n; ) {
            auto event = (struct inotify_event *)p;
            event_update(event);
            p += sizeof(*event) + event->len;
        }
    }

    void watch::event_update(void* e) {
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
        }
        else if (event->mask & (IN_MOVE_SELF | IN_ATTRIB | IN_CLOSE_WRITE | IN_MODIFY)) {
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

    std::optional<notify> watch::select() {
        if (m_notify.empty()) {
            return std::nullopt;
        }
        auto n = m_notify.front();
        m_notify.pop();
        return std::move(n);
    }
}
