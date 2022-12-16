#include <bee/filewatch/filewatch.h>
#include <bee/error.h>
#include <bee/utility/unreachable.h>
#include <assert.h>
#include <functional>
#include <unistd.h>
#include <sys/select.h>
#include <sys/inotify.h>

namespace bee::filewatch {
    watch::watch()
        : m_notify()
        , m_fd_path()
        , m_inotify_fd(inotify_init())
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

    void watch::add(const fs::path& path) {
        int desc = inotify_add_watch(m_inotify_fd, path.c_str(), IN_ALL_EVENTS);
        if (desc != -1) {
            m_fd_path.emplace(std::make_pair(desc, path));
        }
        std::error_code ec;
        fs::directory_iterator iter {path, fs::directory_options::skip_permission_denied, ec };
        fs::directory_iterator end {};
        for (; !ec && iter != end; iter.increment(ec)) {
            auto const& p = iter->path();
            std::error_code _;
            if (fs::is_directory(fs::symlink_status(p, _))) {
                add(p);
            }
        }
    }

    void watch::update() {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(m_inotify_fd, &set);
        struct timeval timeout = {0, 0};
        int rv = ::select(m_inotify_fd + 1, &set, nullptr, nullptr, &timeout);
        if (rv == 0 || rv == -1) {
            return;
        }
        constexpr size_t sz = (10 * ((sizeof(struct inotify_event)) + 255 + 1));
        char buf[sz];
        ssize_t n = read(m_inotify_fd, buf, sz);
        if (n == 0 || n == -1) {
            return;
        }
        for (char *p = buf; p < buf + n;) {
            struct inotify_event *event = reinterpret_cast<struct inotify_event *> (p);
            event_update(event);
            p += (sizeof(struct inotify_event)) + event->len;
        }
    }

    void watch::event_update(void* e) {
        inotify_event* event = (inotify_event*)e;
        if (event->mask & IN_Q_OVERFLOW) {
            // TODO?
        }

        auto filename = m_fd_path[event->wd];
        if (event->len > 1) {
            filename /= std::string(event->name);
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
        if ((event->mask & IN_ISDIR) && (event->mask & IN_CREATE)) {
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
