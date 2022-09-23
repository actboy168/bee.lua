#include <bee/filewatch/filewatch.h>
#include <bee/error.h>
#include <bee/utility/unreachable.h>
#include <assert.h>
#include <functional>
#include <unistd.h>
#include <sys/select.h>
#include <sys/inotify.h>

namespace bee::filewatch {
    class task {
    public:
        taskid      id;
        std::string path;
        task(taskid i, std::string p)
            : id(i)
            , path(p)
        { }
    };

    void watch::add_dir(const fs::path& path)  {
        int desc = inotify_add_watch(m_inotify_fd, path.c_str(), IN_ALL_EVENTS);
        if (desc != -1) {
            m_fd_path.emplace(std::make_pair(desc, path));
            m_path_fd.emplace(std::make_pair(path, desc));
        }
        std::error_code ec;
        fs::directory_iterator iter {path, fs::directory_options::skip_permission_denied, ec };
        fs::directory_iterator end {};
        for (; !ec && iter != end; iter.increment(ec)) {
            auto const& p = iter->path();
            std::error_code _;
            if (fs::is_directory(fs::symlink_status(p, _))) {
                add_dir(p);
            }
        }
    }
    void watch::del_dir(const fs::path& path)  {
        int desc = m_path_fd[path];
        inotify_rm_watch(m_inotify_fd, desc);
        std::error_code ec;
        fs::directory_iterator iter {path, fs::directory_options::skip_permission_denied, ec };
        fs::directory_iterator end {};
        for (; !ec && iter != end; iter.increment(ec)) {
            auto const& p = iter->path();
            std::error_code _;
            if (fs::is_directory(fs::symlink_status(p, _))) {
                del_dir(p);
            }
        }
    }
    void watch::del_dir(int desc)  {
        m_path_fd.erase(m_fd_path[desc]);
        m_fd_path.erase(desc);
    }
    
    watch::watch()
        : m_notify()
        , m_gentask(0)
        , m_tasks()
        , m_fd_path()
        , m_inotify_fd(inotify_init())
    {
        assert(m_inotify_fd != -1);
    }

    watch::~watch() {
        stop();
        close(m_inotify_fd);
        assert(m_tasks.empty());
    }

    void   watch::stop() {
        if (!m_tasks.empty()) {
            for (auto& it : m_tasks) {
                del_dir(it.second->path);
            }
            m_tasks.clear();
        }
    }

    taskid watch::add(const fs::path& path) {
        taskid id = ++m_gentask;
        add_dir(path);
        m_tasks.emplace(std::make_pair(id, std::make_unique<task>(id, path)));
        return id;
    }

    bool   watch::remove(taskid id) {
        auto it = m_tasks.find(id);
        if (it != m_tasks.end()) {
            del_dir(it->second->path);
            m_tasks.erase(it);
        }
        return true;
    }

    void watch::update() {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(m_inotify_fd, &set);
        int msec = 1;
        struct timeval timeout, *timeop = &timeout;
        if (msec < 0) {
            timeop = NULL;
        }
        else {
            timeout.tv_sec = (long)msec / 1000;
            timeout.tv_usec = (long)(msec % 1000 * 1000);
        }
        int rv = ::select(m_inotify_fd + 1, &set, nullptr, nullptr, timeop);
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
            del_dir(event->wd);
        }
        if (event->mask & IN_MOVE_SELF) {
            del_dir(filename);
        }
        if ((event->mask & IN_ISDIR) && (event->mask & IN_CREATE)) {
            add_dir(filename);
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
