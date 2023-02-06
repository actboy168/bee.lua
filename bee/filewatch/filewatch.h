#pragma once

#include <optional>
#include <queue>
#include <string>
#include <functional>

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

struct lua_State;

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
    
    class basic_watch {
    public:
    private:
    };

    class watch {
    public:
#if defined(_WIN32)
        using string_type = std::wstring;
#else
        using string_type = std::string;
#endif
        using filter = std::function<bool(const char*)>;
        static const char* type();
        static inline filter DefaultFilter = [](const char*) { return true; };

        watch();
        ~watch();

        void   stop();
        void   add(const string_type& path);
        void   set_recursive(bool enable);
        bool   set_follow_symlinks(bool enable);
        bool   set_filter(filter f = DefaultFilter);
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
        std::map<int, std::string>              m_fd_path;
        int                                     m_inotify_fd;
        bool                                    m_follow_symlinks = false;
        filter                                  m_filter = DefaultFilter;
#endif
    };
}
