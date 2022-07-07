#pragma once

#include <CoreServices/CoreServices.h>
#include <memory>
#include <string>
#include <map>
#include <bee/thread/lockqueue.h>

namespace bee::osx::filewatch {
    typedef int taskid;
    static const taskid kInvalidTaskId = 0;
    enum class tasktype {
        Error,
        TaskAdd,
        TaskRemove,
        TaskTerminate,
        Modify,
        Rename,
    };
    struct notify {
        tasktype    type;
        std::string path;
    };
    class watch {
    public:
        watch();
        ~watch();
        taskid add(const std::string&  path);
        bool   remove(taskid id);
        void   stop();
        bool   select(notify& notify);
    private:
        bool create_stream(CFArrayRef cf_paths);
        void destroy_stream();
        void update_stream();
   public:
        void event_cb(const char* paths[], const FSEventStreamEventFlags flags[], size_t n);

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

        FSEventStreamRef              m_stream;
        dispatch_queue_t              m_fsevent_queue;
        lockqueue<notify>             m_notify;
        std::map<taskid, std::string> m_tasks; 
        taskid                        m_gentask;
    };
}
