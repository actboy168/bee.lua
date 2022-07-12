#pragma once

#include <string>
#include <map>
#include <memory>
#include <queue>

#if defined(_WIN32)
#elif defined(__APPLE__)
#   include <CoreServices/CoreServices.h>
#elif defined(__linux__)
#   include <sys/inotify.h>
#   include <bee/filesystem.h>
#else
#   error unsupport platform
#endif

namespace bee::filewatch {
    class task;
    
#if defined(_WIN32)
    using string_type = std::wstring;
#else
    using string_type = std::string;
#endif

    typedef int taskid;
    enum class notify_type {
        Modify,
        Rename,
    };
    struct notify {
        notify_type   type;
        string_type path;
    };
    
    class watch {
    public:
        watch();
        ~watch();

        void   stop();
        taskid add(const string_type& path);
        bool   remove(taskid id);
        void   update();
        bool   select(notify& notify);

    private:
#if defined(_WIN32)
        bool   event_update(task& task);
#elif defined(__APPLE__)
        bool   create_stream(CFArrayRef cf_paths);
        void   destroy_stream();
        void   update_stream();
    public:
        void   event_update(const char* paths[], const FSEventStreamEventFlags flags[], size_t n);
    private:
#elif defined(__linux__)
        void   event_update(inotify_event* event);
        void   add_dir(const fs::path& path);
        void   del_dir(const fs::path& path);
        void   del_dir(int desc);
#endif

    private:
        std::queue<notify>                      m_notify;
        taskid                                  m_gentask;
        std::map<taskid, std::unique_ptr<task>> m_tasks;
#if defined(_WIN32)
#elif defined(__APPLE__)
        FSEventStreamRef                        m_stream;
        dispatch_queue_t                        m_fsevent_queue;
#elif defined(__linux__)
        static const unsigned int inotify_buf_size = (10 * ((sizeof(struct inotify_event)) + 255 + 1));
        std::map<int, fs::path>                 m_fd_path;
        std::map<fs::path, int>                 m_path_fd;
        int                                     m_inotify_fd;
        char                                    m_inotify_buf[inotify_buf_size];
#endif
    };
}
