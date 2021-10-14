#include <bee/fsevent/fsevent_linux.h>
#include <bee/format.h>
#include <bee/error.h>
#include <assert.h>
#include <functional>
#include <unistd.h>
#include <sys/select.h>

namespace bee::linux::fsevent {
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
        fs::directory_iterator iter(path, fs::directory_options::skip_permission_denied, ec);
        try {
            if (ec) {
                for (auto& p : iter) {
                    if (fs::is_directory(p, ec) && ec) {
                        add_dir(p);
                    }
                }
            }
        } catch(...) {
        }
    }
    void watch::del_dir(const fs::path& path)  {
        int desc = m_path_fd[path];
        inotify_rm_watch(m_inotify_fd, desc);
        std::error_code ec;
        fs::directory_iterator iter(path, fs::directory_options::skip_permission_denied, ec);
        try {
            if (ec) {
                for (auto& p : iter) {
                    if (fs::is_directory(p, ec) && ec) {
                        del_dir(p);
                    }
                }
            }
        } catch(...) {
        }
    }
    void watch::del_dir(int desc)  {
        m_path_fd.erase(m_fd_path[desc]);
        m_fd_path.erase(desc);
    }
    
    watch::watch()
        : m_thread()
        , m_apc_queue()
        , m_notify()
        , m_gentask(kInvalidTaskId)
        , m_tasks()
        , m_fd_path()
        , m_terminate(false)
        , m_inotify_fd(-1)
    { }

    watch::~watch() {
        stop();
        assert(m_tasks.empty());
    }

    void   watch::stop() {
        if (!m_thread) {
            return;
        }
        if (!m_thread->joinable()) {
            m_thread.reset();
            return;
        }
        m_apc_queue.push({
            apc_arg::type::Terminate,
            kInvalidTaskId,
            std::string(),
        });
        m_thread->join();
        m_thread.reset();
    }

    taskid watch::add(const std::string& path) {
        if (!thread_init()) {
            return kInvalidTaskId;
        }
        taskid id = ++m_gentask;
        m_apc_queue.push({
            apc_arg::type::Add,
            id,
            path
        });
        return id;
    }

    bool   watch::remove(taskid id) {
        if (!m_thread) {
            return false;
        }
        m_apc_queue.push({
            apc_arg::type::Remove,
            id,
            std::string(),
        });
        return true;
    }

    bool   watch::select(notify& n) {
        return m_notify.pop(n);
    }

    bool watch::thread_init() {
        if (m_thread) {
            return true;
        }
        m_thread.reset(new std::thread(std::bind(&watch::thread_cb, this)));
        return true;
    }

    void watch::thread_cb() {
        m_inotify_fd = inotify_init();
        if (m_inotify_fd == -1) {
            return;
        }
        while (!m_terminate || !m_tasks.empty()) {
            apc_update();
            thread_update();
        }
        close(m_inotify_fd);
        m_inotify_fd = -1;
    }

    void watch::thread_update() {
        fd_set set;
        struct timeval timeout;
        FD_ZERO(&set);
        FD_SET(m_inotify_fd, &set);
        timeout.tv_sec = 0;
        timeout.tv_usec = 1000 * 20;
        int rv = ::select(m_inotify_fd + 1, &set, nullptr, nullptr, &timeout);
        if (rv == 0 || rv == -1) {
            return;
        }
        ssize_t n = read(m_inotify_fd, m_inotify_buf, inotify_buf_size);
        if (n == 0 || n == -1) {
            return;
        }
        for (char *p = m_inotify_buf; p < m_inotify_buf + n;) {
            struct inotify_event *event = reinterpret_cast<struct inotify_event *> (p);
            event_update(event);
            p += (sizeof(struct inotify_event)) + event->len;
        }
    }

    void watch::event_update(inotify_event* event) {
        if (event->mask & IN_Q_OVERFLOW) {
            // TODO?
        }

        std::string filename = m_fd_path[event->wd];
        if (event->len > 1) {
            filename += "/";
            filename += event->name;
        }
        if (event->mask & (IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO)) {
            m_notify.push({
                tasktype::Rename, filename
            });
        }
        else if (event->mask & (IN_MOVE_SELF | IN_ATTRIB | IN_CLOSE_WRITE | IN_MODIFY)) {
            m_notify.push({
                tasktype::Modify, filename
            });
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

    void watch::apc_update() {
        apc_arg arg;
        while (m_apc_queue.pop(arg)) {
            switch (arg.m_type) {
            case apc_arg::type::Add:
                apc_add(arg.m_id, arg.m_path);
                m_notify.push({
                    tasktype::Confirm,
                    std::format("add `{}` `{}`", arg.m_id, arg.m_path)
                });
                break;
            case apc_arg::type::Remove:
                apc_remove(arg.m_id);
                m_notify.push({
                    tasktype::Confirm,
                    std::format("remove `{}`", arg.m_id)
                });
                break;
            case apc_arg::type::Terminate:
                apc_terminate();
                m_notify.push({
                    tasktype::Confirm,
                    "terminate"
                });
                return;
            }
        }
    }

    void watch::apc_add(taskid id, const std::string& path) {
        add_dir(path);
        m_tasks.emplace(std::make_pair(id, std::make_unique<task>(id, path)));
    }

    void watch::apc_remove(taskid id) {
        auto it = m_tasks.find(id);
        if (it != m_tasks.end()) {
            del_dir(it->second->path);
            m_tasks.erase(it);
        }
    }

    void watch::apc_terminate() {
        m_terminate = true;
        if (m_tasks.empty()) {
            return;
        }
        for (auto& it : m_tasks) {
            del_dir(it.second->path);
        }
        m_tasks.clear();
    }
}
