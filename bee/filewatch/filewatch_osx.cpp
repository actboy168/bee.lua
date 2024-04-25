#include <bee/filewatch/filewatch.h>
#include <bee/nonstd/unreachable.h>

namespace bee::filewatch {
    static void event_cb(ConstFSEventStreamRef streamRef, void* info, size_t numEvents, void* eventPaths, const FSEventStreamEventFlags eventFlags[], const FSEventStreamEventId eventIds[]) noexcept {
        (void)streamRef;
        (void)eventIds;
        watch* self = (watch*)info;
        self->event_update((const char**)eventPaths, eventFlags, numEvents);
    }

    watch::watch() noexcept
        : m_notify()
        , m_paths()
        , m_stream(NULL) {}

    watch::~watch() noexcept {
        stop();
    }

    void watch::stop() noexcept {
        destroy_stream();
        m_paths.clear();
    }

    bool watch::create_stream(CFArrayRef cf_paths) noexcept {
        if (m_stream) {
            return false;
        }
        FSEventStreamContext ctx = { 0, this, NULL, NULL, NULL };

        FSEventStreamRef ref = FSEventStreamCreate(
            NULL,
            &event_cb,
            &ctx,
            cf_paths,
            kFSEventStreamEventIdSinceNow,
            0.05,
            kFSEventStreamCreateFlagNoDefer | kFSEventStreamCreateFlagFileEvents
        );
        if (ref == NULL) {
            return false;
        }
        m_fsevent_queue = dispatch_queue_create("fsevent_queue", NULL);
        FSEventStreamSetDispatchQueue(ref, m_fsevent_queue);
        if (!FSEventStreamStart(ref)) {
            FSEventStreamInvalidate(ref);
            FSEventStreamRelease(ref);
            return false;
        }
        m_stream = ref;
        return true;
    }

    void watch::destroy_stream() noexcept {
        if (!m_stream) {
            return;
        }
        FSEventStreamStop(m_stream);
        FSEventStreamInvalidate(m_stream);
        FSEventStreamRelease(m_stream);
        dispatch_release(m_fsevent_queue);
        m_stream = NULL;
    }

    void watch::add(const string_type& path) noexcept {
        m_paths.emplace(path);
        update_stream();
    }

    void watch::set_recursive(bool enable) noexcept {
        m_recursive = enable;
    }

    bool watch::set_follow_symlinks(bool enable) noexcept {
        return false;
    }

    bool watch::set_filter(filter f) noexcept {
        return false;
    }

    void watch::update_stream() noexcept {
        destroy_stream();
        if (m_paths.empty()) {
            return;
        }
        std::unique_ptr<CFStringRef[]> paths(new CFStringRef[m_paths.size()]);
        size_t i = 0;
        for (auto& path : m_paths) {
            paths[i] = CFStringCreateWithCString(NULL, path.c_str(), kCFStringEncodingUTF8);
            if (paths[i] == NULL) {
                while (i != 0) {
                    CFRelease(paths[--i]);
                }
                return;
            }
            i++;
        }
        CFArrayRef cf_paths = CFArrayCreate(NULL, (const void**)&paths[0], m_paths.size(), NULL);
        if (create_stream(cf_paths)) {
            return;
        }
        CFRelease(cf_paths);
    }

    void watch::event_update(const char* paths[], const FSEventStreamEventFlags flags[], size_t n) noexcept {
        std::unique_lock<std::mutex> lock(m_mutex);
        for (size_t i = 0; i < n; ++i) {
            const char* path = paths[i];
            if (!m_recursive && path[0] != '\0' && strchr(path + 1, '/') != NULL) {
                continue;
            }
            if (flags[i] & (kFSEventStreamEventFlagItemCreated | kFSEventStreamEventFlagItemRemoved | kFSEventStreamEventFlagItemRenamed)) {
                m_notify.emplace(notify::flag::rename, path);
            } else if (flags[i] & (kFSEventStreamEventFlagItemFinderInfoMod | kFSEventStreamEventFlagItemModified | kFSEventStreamEventFlagItemInodeMetaMod | kFSEventStreamEventFlagItemChangeOwner | kFSEventStreamEventFlagItemXattrMod)) {
                m_notify.emplace(notify::flag::modify, path);
            }
        }
    }

    std::optional<notify> watch::select() noexcept {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_notify.empty()) {
            return std::nullopt;
        }
        auto n = m_notify.front();
        m_notify.pop();
        return n;
    }
}
