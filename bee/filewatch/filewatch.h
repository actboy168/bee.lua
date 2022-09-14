#pragma once

#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <bee/filesystem.h>

#if defined(_WIN32)
#elif defined(__APPLE__)
#   include <CoreServices/CoreServices.h>
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#else
#   error unsupport platform
#endif

namespace bee::filewatch {
    class task;

    typedef int taskid;
    struct notify {
        enum class flag {
            modify,
            rename,
        };
        flag     flags;
        fs::path path;
        notify(flag const& flags, fs::path const& path)
            : flags(flags)
            , path(path)
        { }
    };
    
    class watch {
    public:
        watch();
        ~watch();

        void   stop();
        taskid add(const fs::path& path);
        bool   remove(taskid id);
        void   update();
        std::optional<notify> select();

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
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
        void   event_update(void* event);
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
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
        std::map<int, fs::path>                 m_fd_path;
        std::map<fs::path, int>                 m_path_fd;
        int                                     m_inotify_fd;
#endif
    };
}
