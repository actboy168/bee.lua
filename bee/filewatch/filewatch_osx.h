#pragma once

#include <CoreServices/CoreServices.h>
#include <memory>
#include <string>
#include <map>
#include <queue>

namespace bee::osx::filewatch {
    typedef int taskid;
    enum class notifytype {
        Modify,
        Rename,
    };
    struct notify {
        notifytype  type;
        std::string path;
    };
    class watch {
    public:
        watch();
        ~watch();
        taskid add(const std::string&  path);
        bool   remove(taskid id);
        void   stop();
        void   update();
        bool   select(notify& notify);
    private:
        bool   create_stream(CFArrayRef cf_paths);
        void   destroy_stream();
        void   update_stream();
   public:
        void   event_update(const char* paths[], const FSEventStreamEventFlags flags[], size_t n);

    private:
        FSEventStreamRef              m_stream;
        dispatch_queue_t              m_fsevent_queue;
        std::queue<notify>            m_notify;
        std::map<taskid, std::string> m_tasks; 
        taskid                        m_gentask;
    };
}
