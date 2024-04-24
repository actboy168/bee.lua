#pragma once

#include <functional>
#include <optional>
#include <queue>
#include <string>

#if defined(_WIN32)
#    include <list>
#elif defined(__APPLE__)
#    include <CoreServices/CoreServices.h>

#    include <mutex>
#    include <set>
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#    include <map>
#else
#    error unsupport platform
#endif

struct lua_State;

namespace bee::filewatch {
    class task;

    struct notify {
        enum class flag {
            modify,
            rename,
        };
        flag flags;
        std::string path;
        notify(const flag& flags, const std::string& path) noexcept
            : flags(flags)
            , path(path) {}
    };

    class watch {
    public:
#if defined(_WIN32)
        using string_type = std::wstring;
#else
        using string_type = std::string;
#endif
        using filter                       = std::function<bool(const char*)>;
        static inline filter DefaultFilter = [](const char*) { return true; };

        watch() noexcept;
        ~watch() noexcept;
        void stop() noexcept;
        void add(const string_type& path) noexcept;
        void set_recursive(bool enable) noexcept;
        bool set_follow_symlinks(bool enable) noexcept;
        bool set_filter(filter f = DefaultFilter) noexcept;
        std::optional<notify> select() noexcept;
#if defined(__APPLE__)
        void event_update(const char* paths[], const FSEventStreamEventFlags flags[], size_t n) noexcept;
#endif

    private:
#if defined(_WIN32)
        bool event_update(task& task) noexcept;
#elif defined(__APPLE__)
        bool create_stream(CFArrayRef cf_paths) noexcept;
        void destroy_stream() noexcept;
        void update_stream() noexcept;
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
        void event_update(void* event) noexcept;
#endif

    private:
        std::queue<notify> m_notify;
        bool m_recursive = true;
#if defined(_WIN32)
        std::list<task> m_tasks;
#elif defined(__APPLE__)
        std::mutex m_mutex;
        std::set<std::string> m_paths;
        FSEventStreamRef m_stream;
        dispatch_queue_t m_fsevent_queue;
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
        std::map<int, std::string> m_fd_path;
        int m_inotify_fd;
        bool m_follow_symlinks = false;
        filter m_filter        = DefaultFilter;
#endif
    };
}
