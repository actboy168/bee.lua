#include <bee/filewatch/filewatch.h>
#include <bee/error.h>
#include <bee/utility/unreachable.h>
#include <array>
#include <assert.h>
#include <Windows.h>

namespace bee::filewatch {
    class task : public OVERLAPPED {
        static const size_t kBufSize = 16 * 1024;
    public:
        task(watch* watch, taskid id);
        ~task();

        enum class result {
            success,
            wait,
            failed,
            zero,
        };

        bool   open(const fs::path& path);
        bool   start();
        void   cancel();
        taskid getid();
        result try_read();
        const fs::path& path() const;
        const uint8_t* data() const;

    private:
        taskid                        m_id;
        fs::path                      m_path;
        HANDLE                        m_directory;
        std::array<uint8_t, kBufSize> m_buffer;
    };

    task::task(watch* watch, taskid id)
        : m_id(id)
        , m_path()
        , m_directory(INVALID_HANDLE_VALUE)
        , m_buffer()
    {
        memset((OVERLAPPED*)this, 0, sizeof(OVERLAPPED));
        hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    }

    task::~task() {
        assert(m_directory == INVALID_HANDLE_VALUE);
    }

    bool task::open(const fs::path& path) {
        if (m_directory != INVALID_HANDLE_VALUE) {
            return true;
        }
        m_path = path;
        m_directory = ::CreateFileW(m_path.c_str(),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            NULL);
        if (m_directory == INVALID_HANDLE_VALUE) {
            return false;
        }
        return true;
    }

    void task::cancel() {
        if (m_directory != INVALID_HANDLE_VALUE) {
            ::CancelIo(m_directory);
            ::CloseHandle(m_directory);
            m_directory = INVALID_HANDLE_VALUE;
        }
    }

    taskid task::task::getid() {
        return m_id;
    }

    bool task::start() {
        if (m_directory == INVALID_HANDLE_VALUE) {
            return false;
        }
        if (!ResetEvent(hEvent)) {
            return false;
        }
        if (!::ReadDirectoryChangesW(
            m_directory,
            &m_buffer[0],
            static_cast<DWORD>(m_buffer.size()),
            TRUE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION,
            NULL,
            this,
            NULL))
        {
            ::CloseHandle(m_directory);
            m_directory = INVALID_HANDLE_VALUE;
            return false;
        }
        return true;
    }

    task::result task::try_read() {
        DWORD dwNumberOfBytesTransfered;
        bool ok = GetOverlappedResult(m_directory, this, &dwNumberOfBytesTransfered, FALSE);
        DWORD dwErrorCode = ::GetLastError();
        if (!ok) {
            if (dwErrorCode == ERROR_IO_INCOMPLETE) {
                return result::wait;
            }
        }
        if (dwErrorCode != 0) {
            if (dwErrorCode == ERROR_NOTIFY_ENUM_DIR) {
                // TODO notify overflow
                return result::zero;
            }
            cancel();
            return result::failed;
        }
        if (!dwNumberOfBytesTransfered) {
            return result::zero;
        }
        assert(dwNumberOfBytesTransfered >= offsetof(FILE_NOTIFY_INFORMATION, FileName) + sizeof(WCHAR));
        assert(dwNumberOfBytesTransfered <= m_buffer.size());
        return result::success;
    }

    const fs::path& task::path() const {
        return m_path;
    }

    const uint8_t* task::data() const {
        return m_buffer.data();
    }

    watch::watch()
        : m_notify()
        , m_gentask(0)
        , m_tasks()
    { }

    watch::~watch() {
        stop();
    }

    void watch::stop() {
        if (m_tasks.empty()) {
            return;
        }
        for (auto& it : m_tasks) {
            it.second->cancel();
        }
        m_tasks.clear();
    }

    taskid watch::add(const fs::path& path) {
        taskid id = ++m_gentask;
        auto t = std::make_unique<task>(this, id);
        if (t->open(path)) {
            if (t->start()) {
                m_tasks.emplace(std::make_pair(id, std::move(t)));
            }
        }
        return id;
    }

    bool watch::remove(taskid id) {
        auto it = m_tasks.find(id);
        if (it != m_tasks.end()) {
            it->second->cancel();
        }
        return true;
    }

    bool watch::event_update(task& task) {
        switch (task.try_read()) {
        case task::result::wait:
            return true;
        case task::result::failed:
            task.cancel();
            return false;
        case task::result::zero:
            return task.start();
        case task::result::success:
            break;
        }
        const uint8_t* data = task.data();
        for (;;) {
            FILE_NOTIFY_INFORMATION& fni = (FILE_NOTIFY_INFORMATION&)*data;
            std::wstring path(fni.FileName, fni.FileNameLength / sizeof(wchar_t));
            switch (fni.Action) {
            case FILE_ACTION_MODIFIED:
                m_notify.emplace(notify::flag::modify, task.path() / path);
                break;
            case FILE_ACTION_ADDED:
            case FILE_ACTION_REMOVED:
            case FILE_ACTION_RENAMED_OLD_NAME:
            case FILE_ACTION_RENAMED_NEW_NAME:
                m_notify.emplace(notify::flag::rename, task.path() / path);
                break;
            default:
                assert(false);
                break;
            }
            if (!fni.NextEntryOffset) {
                break;
            }
            data += fni.NextEntryOffset;
        }
        return task.start();
    }

    void watch::update() {
        for (auto iter = m_tasks.begin(); iter != m_tasks.end();) {
            if (event_update(*iter->second)) {
                ++iter;
            }
            else {
                iter = m_tasks.erase(iter);
            }
        }
    }

    std::optional<notify> watch::select() {
        if (m_notify.empty()) {
            return std::nullopt;
        }
        auto n = m_notify.front();
        m_notify.pop();
        return std::move(n);
    }
}
