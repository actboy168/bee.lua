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
        friend class task;
    public:
        watch();
        ~watch();

        void   stop();
        taskid add(const std::wstring& path);
        bool   remove(taskid id);
        bool   select(notify& notify);

    private:
        std::queue<notify>                      m_notify;
        taskid                                  m_gentask;
        std::map<taskid, std::unique_ptr<task>> m_tasks;
    };
}
