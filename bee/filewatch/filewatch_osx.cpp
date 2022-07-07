#include <bee/filewatch/filewatch_osx.h>
#include <bee/format.h>
#include <bee/utility/unreachable.h>

namespace bee::osx::filewatch {
    static void watch_event_cb(ConstFSEventStreamRef streamRef,
        void* info,
        size_t numEvents,
        void* eventPaths,
        const FSEventStreamEventFlags eventFlags[],
        const FSEventStreamEventId eventIds[])
    {
        (void)streamRef;
        (void)eventIds;
        watch* self = (watch*)info;
        self->event_cb((const char**)eventPaths, eventFlags, numEvents);
    }

    watch::watch() 
        : m_stream(NULL)
        , m_gentask(kInvalidTaskId)
    { }
    watch::~watch() {
        stop();
    }
    void watch::stop() {
        destroy_stream();
        m_tasks.clear();
        m_notify.push({
            tasktype::TaskTerminate,
            ""
        });
    }

    bool watch::create_stream(CFArrayRef cf_paths) {
        if (m_stream) {
            return false;
        }
        FSEventStreamContext ctx = { 0 , this, NULL , NULL , NULL };

        FSEventStreamRef ref = 
            FSEventStreamCreate(NULL,
                &watch_event_cb,
                &ctx,
                cf_paths,
                kFSEventStreamEventIdSinceNow,
                0.05,
                kFSEventStreamCreateFlagNoDefer | kFSEventStreamCreateFlagFileEvents
            );
        assert(ref != NULL);
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

    void watch::destroy_stream() {
        if (!m_stream) {
            return;
        }
        FSEventStreamStop(m_stream);
        FSEventStreamInvalidate(m_stream);
        FSEventStreamRelease(m_stream);
        dispatch_release(m_fsevent_queue);
        m_stream = NULL;
    }

    taskid watch::add(const std::string& path) {
        taskid id = ++m_gentask;
        m_tasks.insert(std::make_pair(id, path));
        update_stream();
        m_notify.push({
            tasktype::TaskAdd,
            std::format("({}){}", id, path)
        });
        return id;
    }

    bool watch::remove(taskid id) {
        m_tasks.erase(id);
        update_stream();
        m_notify.push({
            tasktype::TaskRemove,
            std::format("{}", id)
        });
        return true;
    }

    void watch::update_stream() {
        destroy_stream();
        if (m_tasks.empty()) {
            return;
        }
        std::unique_ptr<CFStringRef[]> paths(new CFStringRef[m_tasks.size()]);
        size_t i = 0;
        for (auto task : m_tasks) {
            paths[i] = CFStringCreateWithCString(NULL, task.second.c_str(), kCFStringEncodingUTF8);
            if (paths[i] == NULL) {
                while (i != 0) {
                    CFRelease(paths[--i]);
                }
                return;
            }
            i++;
        }
        CFArrayRef cf_paths = CFArrayCreate(NULL, (const void **)&paths[0], m_tasks.size(), NULL);
        if (create_stream(cf_paths)) {
            return;
        }
        CFRelease(cf_paths);
    }

    bool watch::select(notify& notify) {
        return m_notify.pop(notify);
    }

    void watch::event_cb(const char* paths[], const FSEventStreamEventFlags flags[], size_t n) {
        for (size_t i = 0; i < n; ++i) {
            if (flags[i] & (
                kFSEventStreamEventFlagItemCreated |
                kFSEventStreamEventFlagItemRemoved |
                kFSEventStreamEventFlagItemRenamed
            )) {
                m_notify.push({
                    tasktype::Rename, paths[i]
                });
            }
            else if (flags[i] & (
                kFSEventStreamEventFlagItemFinderInfoMod |
                kFSEventStreamEventFlagItemModified |
                kFSEventStreamEventFlagItemInodeMetaMod |
                kFSEventStreamEventFlagItemChangeOwner |
                kFSEventStreamEventFlagItemXattrMod
            )) {
                m_notify.push({
                    tasktype::Modify, paths[i]
                });
            }
        }
    }
}
