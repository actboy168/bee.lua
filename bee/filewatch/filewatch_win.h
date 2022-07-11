#pragma once

#include <string>
#include <map>
#include <memory>
#include <queue>

namespace bee::win::filewatch {
    class task;

    typedef int taskid;
    enum class notifytype {
        Modify,
        Rename,
    };
    struct notify {
        notifytype   type;
        std::wstring path;
    };

    class watch {
    public:
        watch();
        ~watch();

        void   stop();
        taskid add(const std::wstring& path);
        bool   remove(taskid id);
        void   update();
        bool   select(notify& notify);

    private:
        bool   event_update(task& task);

    private:
        std::queue<notify>                      m_notify;
        taskid                                  m_gentask;
        std::map<taskid, std::unique_ptr<task>> m_tasks;
    };
}
