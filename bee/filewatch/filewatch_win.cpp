#include <Windows.h>
#include <bee/filewatch/filewatch.h>
#include <bee/nonstd/unreachable.h>
#include <bee/win/wtf8.h>

#include <array>
#include <cassert>

namespace bee::filewatch {
    class task : public OVERLAPPED {
        static const size_t kBufSize = 16 * 1024;

    public:
        task() noexcept;
        ~task() noexcept;

        enum class result {
            success,
            wait,
            failed,
            zero,
        };

        bool open(const std::wstring& path) noexcept;
        bool start(bool recursive) noexcept;
        void cancel() noexcept;
        result try_read() noexcept;
        const std::wstring& path() const noexcept;
        const std::byte* data() const noexcept;

    private:
        std::wstring m_path;
        HANDLE m_directory;
        std::array<std::byte, kBufSize> m_buffer;
    };

    task::task() noexcept
        : m_path()
        , m_directory(INVALID_HANDLE_VALUE)
        , m_buffer() {
        memset((OVERLAPPED*)this, 0, sizeof(OVERLAPPED));
        hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    }

    task::~task() noexcept {
        assert(m_directory == INVALID_HANDLE_VALUE);
    }

    bool task::open(const std::wstring& path) noexcept {
        if (m_directory != INVALID_HANDLE_VALUE) {
            return true;
        }
        if (path.back() != L'/') {
            m_path = path + L"/";
        } else {
            m_path = path;
        }
        m_directory = ::CreateFileW(m_path.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
        if (m_directory == INVALID_HANDLE_VALUE) {
            return false;
        }
        return true;
    }

    void task::cancel() noexcept {
        if (m_directory != INVALID_HANDLE_VALUE) {
            ::CancelIo(m_directory);
            ::CloseHandle(m_directory);
            m_directory = INVALID_HANDLE_VALUE;
        }
    }

    bool task::start(bool recursive) noexcept {
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
                recursive,
                FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                    FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION,
                NULL,
                this,
                NULL
            )) {
            ::CloseHandle(m_directory);
            m_directory = INVALID_HANDLE_VALUE;
            return false;
        }
        return true;
    }

    task::result task::try_read() noexcept {
        DWORD dwNumberOfBytesTransfered = 0;
        const bool ok                   = GetOverlappedResult(m_directory, this, &dwNumberOfBytesTransfered, FALSE);
        const DWORD dwErrorCode         = ::GetLastError();
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

    const std::wstring& task::path() const noexcept {
        return m_path;
    }

    const std::byte* task::data() const noexcept {
        return m_buffer.data();
    }

    watch::watch() noexcept
        : m_notify()
        , m_tasks() {}

    watch::~watch() noexcept {
        stop();
    }

    void watch::stop() noexcept {
        if (m_tasks.empty()) {
            return;
        }
        for (auto& task : m_tasks) {
            task.cancel();
        }
        m_tasks.clear();
    }

    void watch::add(const string_type& path) noexcept {
        auto& t = m_tasks.emplace_back();
        if (t.open(path)) {
            if (t.start(m_recursive)) {
                return;
            }
        }
        m_tasks.pop_back();
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

    bool watch::event_update(task& task) noexcept {
        switch (task.try_read()) {
        case task::result::wait:
            return true;
        case task::result::failed:
            task.cancel();
            return false;
        case task::result::zero:
            return task.start(m_recursive);
        case task::result::success:
            break;
        }
        const std::byte* data = task.data();
        for (;;) {
            const FILE_NOTIFY_INFORMATION& fni = (const FILE_NOTIFY_INFORMATION&)*data;
            std::wstring path(fni.FileName, fni.FileNameLength / sizeof(wchar_t));
            path = task.path() + path;
            switch (fni.Action) {
            case FILE_ACTION_MODIFIED:
                m_notify.emplace(notify::flag::modify, wtf8::w2u(path));
                break;
            case FILE_ACTION_ADDED:
            case FILE_ACTION_REMOVED:
            case FILE_ACTION_RENAMED_OLD_NAME:
            case FILE_ACTION_RENAMED_NEW_NAME:
                m_notify.emplace(notify::flag::rename, wtf8::w2u(path));
                break;
            default:
                std::unreachable();
                break;
            }
            if (!fni.NextEntryOffset) {
                break;
            }
            data += fni.NextEntryOffset;
        }
        return task.start(m_recursive);
    }

    std::optional<notify> watch::select() noexcept {
        for (auto iter = m_tasks.begin(); iter != m_tasks.end();) {
            if (event_update(*iter)) {
                ++iter;
            } else {
                iter = m_tasks.erase(iter);
            }
        }
        if (m_notify.empty()) {
            return std::nullopt;
        }
        auto n = m_notify.front();
        m_notify.pop();
        return n;
    }
}
