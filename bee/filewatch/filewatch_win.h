#pragma once

#include <string>
#include <map>
#include <memory>
#include <bee/thread/lockqueue.h>

namespace bee::win::filewatch {
    class task;

    typedef int taskid;
    enum class tasktype {
        Error,
        TaskAdd,
        TaskRemove,
        TaskTerminate,
        Modify,
        Rename,
    };
    struct notify {
        tasktype     type;
        std::wstring path;
    };
    static const taskid kInvalidTaskId = 0;

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
        struct apc_arg {
            enum class type {
                Add,
                Remove,
                Terminate,
            };
            type                  m_type;
            taskid                m_id;
            std::wstring          m_path;
        };
        void update();
        void apc_add(taskid id, const std::wstring& path);
        void apc_remove(taskid id);
        void apc_terminate();
        void removetask(task* task);

    private:
        lockqueue<notify>                       m_notify;
        taskid                                  m_gentask;
        std::map<taskid, std::unique_ptr<task>> m_tasks;
        bool                                    m_terminate;
    };
}
