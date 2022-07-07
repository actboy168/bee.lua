#include <bee/filewatch/filewatch_win.h>
#include <bee/utility/unicode_win.h>
#include <bee/format.h>
#include <bee/error.h>
#include <bee/utility/unreachable.h>
#include <array>
#include <functional>
#include <assert.h>
#include <Windows.h>
#include <filesystem>

namespace fs = std::filesystem;

namespace bee::win::filewatch {
    class task : public OVERLAPPED {
        static const size_t kBufSize = 16 * 1024;
    public:
        task(watch* watch, taskid id);
        ~task();

        bool   open(const std::wstring& path);
        bool   start();
        void   cancel();
        taskid getid();
        void   push_notify(tasktype type, std::wstring&& message);
        bool   event_cb(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered);
        bool   update();

    private:
        watch*                        m_watch;
        taskid                        m_id;
        fs::path                      m_path;
        HANDLE                        m_directory;
        std::array<uint8_t, kBufSize> m_buffer;
        std::array<uint8_t, kBufSize> m_bakbuffer;
    };

    task::task(watch* watch, taskid id)
        : m_watch(watch)
        , m_id(id)
        , m_path()
        , m_directory(INVALID_HANDLE_VALUE)
    {
        memset((OVERLAPPED*)this, 0, sizeof(OVERLAPPED));
        hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    }

    task::~task() {
        assert(m_directory == INVALID_HANDLE_VALUE);
    }

    bool task::open(const std::wstring& path) {
        if (m_directory != INVALID_HANDLE_VALUE) {
            return true;
        }
        
        std::error_code ec;
        m_path = fs::absolute(path, ec);
        if (ec) {
            return false;
        }

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

    bool task::update() {
        DWORD bytes_returned;
        bool ok = GetOverlappedResult(m_directory, this, &bytes_returned, FALSE);
        if (!ok) {
            if (::GetLastError() == ERROR_IO_INCOMPLETE) {
                return true;
            }
        }
        return event_cb(::GetLastError(), bytes_returned);
    }

    bool task::event_cb(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered) {
        if (dwErrorCode != 0) {
            cancel();
            return false;
        }
        if (!dwNumberOfBytesTransfered) {
            return true;
        }
        assert(dwNumberOfBytesTransfered >= offsetof(FILE_NOTIFY_INFORMATION, FileName) + sizeof(WCHAR));
        assert(dwNumberOfBytesTransfered <= m_bakbuffer.size());
        memcpy(&m_bakbuffer[0], &m_buffer[0], dwNumberOfBytesTransfered);
        start();

        uint8_t* data = m_bakbuffer.data();
        for (;;) {
            FILE_NOTIFY_INFORMATION& fni = (FILE_NOTIFY_INFORMATION&)*data;
            std::wstring path(fni.FileName, fni.FileNameLength / sizeof(wchar_t));
            switch (fni.Action) {
            case FILE_ACTION_MODIFIED:
                push_notify(tasktype::Modify, (m_path / path).wstring());
                break;
            case FILE_ACTION_ADDED:
            case FILE_ACTION_REMOVED:
            case FILE_ACTION_RENAMED_OLD_NAME:
            case FILE_ACTION_RENAMED_NEW_NAME:
                push_notify(tasktype::Rename, (m_path / path).wstring());
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
        return true;
    }

    void task::push_notify(tasktype type, std::wstring&& message) {
        m_watch->m_notify.push({
            type,
            std::forward<std::wstring>(message),
        });
    }

    watch::watch()
        : m_notify()
        , m_gentask(kInvalidTaskId)
        , m_tasks()
    { }

    watch::~watch() {
        stop();
        assert(m_tasks.empty());
    }

    void watch::stop() {
        if (m_tasks.empty()) {
            return;
        }
        for (auto& it : m_tasks) {
            it.second->cancel();
        }
    }

    taskid watch::add(const std::wstring& path) {
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

    bool watch::select(notify& n) {
        for (auto iter = m_tasks.begin(); iter != m_tasks.end();) {
            if (iter->second->update()) {
                ++iter;
            }
            else {
                iter = m_tasks.erase(iter);
            }
        }
        if (m_notify.empty()) {
            return false;
        }
        n = m_notify.front();
        m_notify.pop();
        return true;
    }
}
