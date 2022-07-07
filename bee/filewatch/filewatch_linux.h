#pragma once

#include <string>
#include <map>
#include <memory>
#include <bee/thread/lockqueue.h>
#include <sys/inotify.h>
#include <bee/filesystem.h>

namespace bee::linux::filewatch {
    class task;

    typedef int taskid;
    enum class tasktype {
        Modify,
        Rename,
    };
    struct notify {
        tasktype    type;
        std::string path;
    };
    static const taskid kInvalidTaskId = 0;

    class watch {
    public:
        watch();
        ~watch();

        void   stop();
        taskid add(const std::string& path);
        bool   remove(taskid id);
        bool   select(notify& notify, int msec = 1);

    private:
        void update(int msec);
        void event_update(inotify_event* event);
        void add_dir(const fs::path& path);
        void del_dir(const fs::path& path);
        void del_dir(int desc);

    private:
        static const unsigned int inotify_buf_size = (10 * ((sizeof(struct inotify_event)) + 255 + 1));

        lockqueue<notify>                       m_notify;
        taskid                                  m_gentask;
        std::map<taskid, std::unique_ptr<task>> m_tasks;
        std::map<int, fs::path>                 m_fd_path;
        std::map<fs::path, int>                 m_path_fd;
        int                                     m_inotify_fd;
        char                                    m_inotify_buf[inotify_buf_size];
    };
}
