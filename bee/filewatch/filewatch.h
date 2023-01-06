#pragma once

#include <optional>
#include <queue>
#include <bee/filesystem.h>

#if defined(_WIN32)
#include <memory>
#include <set>
#elif defined(__APPLE__)
#include <set>
#   include <CoreServices/CoreServices.h>
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <map>
#else
#   error unsupport platform
#endif

namespace bee::filewatch {
    class task;

    struct notify {
        enum class flag {
            modify,
            rename,
        };
        flag        flags;
        std::string path;
        notify(flag const& flags, std::string const& path)
            : flags(flags)
            , path(path)
        { }
    };
    
    class watch {
    public:
        watch();
        ~watch();

        void   stop();
        void   add(const fs::path& path);
        bool   recursive(bool enable);
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
#endif

    private:
        std::queue<notify>                      m_notify;
        bool                                    m_recursive = true;
#if defined(_WIN32)
        std::set<std::unique_ptr<task>>         m_tasks;
#elif defined(__APPLE__)
        std::set<std::string>                   m_paths;
        FSEventStreamRef                        m_stream;
        dispatch_queue_t                        m_fsevent_queue;
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
        std::map<int, fs::path>                 m_fd_path;
        int                                     m_inotify_fd;
#endif
    };
}
