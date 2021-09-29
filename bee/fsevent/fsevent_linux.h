#pragma once

#include <string>
#include <map>
#include <memory>
#include <thread>
#include <bee/thread/lockqueue.h>
#include <sys/inotify.h>
#include <bee/filesystem.h>

namespace bee::linux::fsevent {
    class task;

    typedef int taskid;
    enum class tasktype {
        Error,
        Confirm,
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
        bool   select(notify& notify);

    private:
        struct apc_arg {
            enum class type {
                Add,
                Remove,
                Terminate,
            };
            type                  m_type;
            taskid                m_id;
            std::string           m_path;
        };
        bool thread_init();
        void thread_cb();
        void thread_update();
        void event_update(inotify_event* event);
        void apc_update();
        void apc_add(taskid id, const std::string& path);
        void apc_remove(taskid id);
        void apc_terminate();
        void add_dir(const fs::path& path);
        void del_dir(const fs::path& path);
        void del_dir(int desc);

    private:
        static const unsigned int inotify_buf_size = (10 * ((sizeof(struct inotify_event)) + 255 + 1));

        std::unique_ptr<std::thread>            m_thread;
        lockqueue<apc_arg>                      m_apc_queue;
        lockqueue<notify>                       m_notify;
        taskid                                  m_gentask;
        std::map<taskid, std::unique_ptr<task>> m_tasks;
        std::map<int, fs::path>                 m_fd_path;
        std::map<fs::path, int>                 m_path_fd;
        bool                                    m_terminate;
        int                                     m_inotify_fd;
        char                                    m_inotify_buf[inotify_buf_size];
    };
}
