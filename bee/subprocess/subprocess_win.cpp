#include <Shobjidl.h>
#include <Windows.h>
#include <bee/net/socket.h>
#include <bee/nonstd/bit.h>
#include <bee/nonstd/format.h>
#include <bee/nonstd/unreachable.h>
#include <bee/subprocess.h>
#include <bee/utility/dynarray.h>
#include <bee/win/wtf8.h>
#include <signal.h>

#include <deque>
#include <memory>
#include <thread>

#define SIGKILL 9

namespace bee::subprocess {
    void args_t::push(zstring_view v) noexcept {
        data_.emplace_back(wtf8::u2w(v));
    }
    void args_t::push(const std::wstring& v) noexcept {
        data_.emplace_back(v);
    }

    template <class char_t>
    struct strbuilder {
        struct node {
            dynarray<char_t> data;
            size_t size;
            node(size_t maxsize) noexcept
                : data(maxsize)
                , size(0) {}
            bool has(size_t n) const noexcept {
                return size + n <= data.size();
            }
            void append(const char_t* str, size_t n) noexcept {
                assert(has(n));
                memcpy(data.data() + size, str, n * sizeof(char_t));
                size += n;
            }
        };
        strbuilder(size_t defsize) noexcept
            : deque()
            , size(0)
            , defsize(defsize) {
            deque.emplace_back(defsize);
        }
        void append(const char_t* str, size_t n) noexcept {
            auto& back = deque.back();
            if (back.has(n)) {
                back.append(str, n);
            } else if (n >= defsize) {
                deque.emplace_back(n).append(str, n);
            } else {
                deque.emplace_back(defsize).append(str, n);
            }
            size += n;
        }
        template <class T, size_t n>
        strbuilder& operator+=(T (&str)[n]) noexcept {
            append(str, n - 1);
            return *this;
        }
        strbuilder& operator+=(const std::basic_string_view<char_t>& s) noexcept {
            append(s.data(), s.size());
            return *this;
        }
        dynarray<char_t> string() noexcept {
            dynarray<char_t> r(size + 1);
            size_t pos = 0;
            for (auto& s : deque) {
                memcpy(r.data() + pos, s.data.data(), sizeof(char_t) * s.size);
                pos += s.size;
            }
            r[pos] = char_t { '\0' };
            return r;
        }
        std::deque<node> deque;
        size_t size;
        size_t defsize;
    };

    struct EnvironmentStrings {
        ~EnvironmentStrings() noexcept {
            if (str) {
                FreeEnvironmentStringsW(str);
            }
        }
        operator wchar_t*() const noexcept {
            return str;
        }
        operator bool() const noexcept {
            return !!str;
        }
        wchar_t* str = GetEnvironmentStringsW();
    };

    template <class char_t>
    std::basic_string<char_t> quote_arg(const std::basic_string<char_t>& source) noexcept {
        const size_t len = source.size();
        if (len == 0) {
            return { '\"', '\"', '\0' };
        }
        if (std::basic_string<char_t>::npos == source.find_first_of({ ' ', '\t', '\"' })) {
            return source;
        }
        if (std::basic_string<char_t>::npos == source.find_first_of({ '\"', '\\' })) {
            return std::basic_string<char_t>({ '\"' }) + source + std::basic_string<char_t>({ '\"' });
        }
        std::basic_string<char_t> target;
        target += '"';
        int quote_hit = 1;
        for (size_t i = len; i > 0; --i) {
            target += source[i - 1];

            if (quote_hit && source[i - 1] == '\\') {
                target += '\\';
            } else if (source[i - 1] == '"') {
                quote_hit = 1;
                target += '\\';
            } else {
                quote_hit = 0;
            }
        }
        target += '"';
        for (size_t i = 0; i < target.size() / 2; ++i) {
            std::swap(target[i], target[target.size() - i - 1]);
        }
        return target;
    }

    static dynarray<wchar_t> make_args(const args_t& args, std::wstring_view prefix = std::wstring_view()) noexcept {
        strbuilder<wchar_t> res(1024);
        if (!prefix.empty()) {
            res += prefix;
        }
        for (size_t i = 0; i < args.size(); ++i) {
            res += quote_arg(args[i]);
            if (i + 1 != args.size()) {
                res += L" ";
            }
        }
        return res.string();
    }

    void envbuilder::set(const std::wstring& key, const std::wstring& value) noexcept {
        set_env_[key] = value;
    }

    void envbuilder::del(const std::wstring& key) noexcept {
        set_env_[key] = std::nullopt;
    }

    static void env_append(strbuilder<wchar_t>& envs, const std::wstring& k, const std::wstring& v) noexcept {
        envs += k;
        envs += L"=";
        envs += v;
        envs += L"\0";
    }

    environment envbuilder::release() noexcept {
        EnvironmentStrings es;
        if (!es) {
            return nullptr;
        }
        if (set_env_.empty()) {
            return nullptr;
        }
        strbuilder<wchar_t> res(1024);
        wchar_t* escp = es;
        while (*escp != L'\0') {
            std::wstring str = escp;
            auto pos         = str.find(L'=');
            std::wstring key = str.substr(0, pos);
            std::wstring val = str.substr(pos + 1, str.length());
            const auto it    = set_env_.find(key);
            if (it == set_env_.end()) {
                env_append(res, key, val);
            } else {
                if (it->second.has_value()) {
                    env_append(res, key, *it->second);
                }
                set_env_.erase(it);
            }
            escp += str.length() + 1;
        }
        for (auto& e : set_env_) {
            const std::wstring& key = e.first;
            if (e.second.has_value()) {
                env_append(res, key, *e.second);
            }
        }
        return res.string();
    }

    static HANDLE create_job() noexcept {
        SECURITY_ATTRIBUTES attr;
        memset(&attr, 0, sizeof attr);
        attr.bInheritHandle = FALSE;

        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info;
        memset(&info, 0, sizeof info);
        info.BasicLimitInformation.LimitFlags =
            JOB_OBJECT_LIMIT_BREAKAWAY_OK | JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK | JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;

        HANDLE job = CreateJobObjectW(&attr, NULL);
        if (job == NULL) {
            return NULL;
        }
        if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof info)) {
            return NULL;
        }
        return job;
    }

    static bool join_job(HANDLE process) noexcept {
        static HANDLE job = create_job();
        if (!job) {
            return false;
        }
        if (!AssignProcessToJobObject(job, process)) {
            const DWORD err = GetLastError();
            if (err != ERROR_ACCESS_DENIED) {
                return false;
            }
        }
        return true;
    }

    static HWND console_window(DWORD pid) noexcept {
        DWORD wpid = 0;
        HWND wnd   = NULL;
        do {
            wnd = FindWindowExW(NULL, wnd, L"ConsoleWindowClass", NULL);
            if (wnd == NULL) {
                break;
            }
            GetWindowThreadProcessId(wnd, &wpid);
        } while (pid != wpid);
        return wnd;
    }

    static bool hide_taskbar(HWND w) noexcept {
        ITaskbarList* taskbar = nullptr;
        if (!SUCCEEDED(::CoInitializeEx(NULL, COINIT_MULTITHREADED))) {
            return false;
        }
        if (SUCCEEDED(CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, IID_ITaskbarList, (void**)&taskbar))) {
            taskbar->HrInit();
            taskbar->DeleteTab(w);
            taskbar->Release();
            return true;
        }
        return false;
    }

    static void hide_console(process& proc) noexcept {
        std::thread thd([&]() {
            auto dup_proc = proc.dup();
            if (!dup_proc) {
                return;
            }
            for (;; std::this_thread::sleep_for(std::chrono::milliseconds(10))) {
                if (!dup_proc->is_running()) {
                    return;
                }
                HWND wnd = console_window(dup_proc->get_id());
                if (wnd) {
                    SetWindowPos(wnd, NULL, -10000, -10000, 0, 0, SWP_HIDEWINDOW);
                    hide_taskbar(wnd);
                    return;
                }
            }
        });
        thd.detach();
    }

    spawn::spawn() noexcept {
        fds_[0] = INVALID_HANDLE_VALUE;
        fds_[1] = INVALID_HANDLE_VALUE;
        fds_[2] = INVALID_HANDLE_VALUE;
    }

    spawn::~spawn() noexcept {
    }

    void spawn::search_path() noexcept {
        search_path_ = true;
    }

    void spawn::set_console(console type) noexcept {
        console_ = type;
        flags_ &= ~(CREATE_NO_WINDOW | CREATE_NEW_CONSOLE | DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP);
        switch (type) {
        case console::eInherit:
            break;
        case console::eDetached:
            flags_ |= DETACHED_PROCESS & CREATE_NEW_PROCESS_GROUP;
            break;
        case console::eDisable:
            flags_ |= CREATE_NO_WINDOW;
            break;
        case console::eNew:
        case console::eHide:
            flags_ |= CREATE_NEW_CONSOLE;
            break;
        default:
            std::unreachable();
        }
    }

    bool spawn::hide_window() noexcept {
        hide_window_ = true;
        return true;
    }

    void spawn::suspended() noexcept {
        flags_ |= CREATE_SUSPENDED;
    }

    void spawn::detached() noexcept {
        set_console(console::eDetached);
        detached_ = true;
    }

    void spawn::redirect(stdio type, file_handle h) noexcept {
        inherit_handle_ = true;
        switch (type) {
        case stdio::eInput:
            fds_[0] = h.value();
            break;
        case stdio::eOutput:
            fds_[1] = h.value();
            break;
        case stdio::eError:
            fds_[2] = h.value();
            break;
        default:
            std::unreachable();
        }
    }

    static void startupinfo_release(STARTUPINFOW& si) noexcept {
        if (si.hStdInput != INVALID_HANDLE_VALUE) {
            ::CloseHandle(si.hStdInput);
        }
        if (si.hStdOutput != INVALID_HANDLE_VALUE) {
            ::CloseHandle(si.hStdOutput);
        }
        if (si.hStdError != INVALID_HANDLE_VALUE && si.hStdOutput != si.hStdError) {
            ::CloseHandle(si.hStdError);
        }
    }

    bool spawn::exec(const args_t& args, const wchar_t* cwd) noexcept {
        if (args.size() == 0) {
            return false;
        }
        dynarray<wchar_t> command_line = make_args(args);
        if (command_line.empty()) {
            return false;
        }
        const wchar_t* application = search_path_ ? 0 : args[0].c_str();
        if (env_) {
            flags_ |= CREATE_UNICODE_ENVIRONMENT;
        }
        STARTUPINFOW si;
        memset(&si, 0, sizeof(STARTUPINFOW));
        si.cb      = sizeof(STARTUPINFOW);
        si.dwFlags = 0;
        if (inherit_handle_) {
            si.dwFlags |= STARTF_USESTDHANDLES;
            si.hStdInput  = fds_[0];
            si.hStdOutput = fds_[1];
            si.hStdError  = fds_[2];
            ::SetHandleInformation(si.hStdInput, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
            ::SetHandleInformation(si.hStdOutput, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
            ::SetHandleInformation(si.hStdError, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        } else {
            si.hStdInput  = INVALID_HANDLE_VALUE;
            si.hStdOutput = INVALID_HANDLE_VALUE;
            si.hStdError  = INVALID_HANDLE_VALUE;
        }
        if (hide_window_) {
            si.dwFlags |= STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
        }
        if (!::CreateProcessW(application, command_line.data(), NULL, NULL, inherit_handle_, flags_ | NORMAL_PRIORITY_CLASS, env_, cwd, &si, (LPPROCESS_INFORMATION)&pi_)) {
            startupinfo_release(si);
            return false;
        }
        startupinfo_release(si);
        if (!detached_) {
            join_job(pi_.native_handle());
        }
        if (console_ == console::eHide) {
            hide_console(pi_);
        }
        return true;
    }

    void spawn::env(environment&& env) noexcept {
        env_ = std::move(env);
    }

    static_assert(sizeof(PROCESS_INFORMATION) == sizeof(process));

    process::process() noexcept
        : hProcess(NULL)
        , hThread(NULL)
        , dwProcessId(0)
        , dwThreadId(0) {}

    process::process(process&& o) noexcept
        : process() {
        std::swap(hProcess, o.hProcess);
        std::swap(hThread, o.hThread);
        std::swap(dwProcessId, o.dwProcessId);
        std::swap(dwThreadId, o.dwThreadId);
    }

    process::process(spawn& spawn) noexcept
        : process(std::move(spawn.pi_)) {}

    process::~process() noexcept {
        detach();
    }

    bool process::detach() noexcept {
        if (hThread != NULL) {
            ::CloseHandle(hThread);
            hThread = NULL;
        }
        if (hProcess != NULL) {
            ::CloseHandle(hProcess);
            hProcess = NULL;
        }
        return true;
    }

    std::optional<process> process::dup() noexcept {
        HANDLE handle = NULL;
        if (!::DuplicateHandle(::GetCurrentProcess(), native_handle(), ::GetCurrentProcess(), &handle, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
            return std::nullopt;
        }
        process proc;
        proc.hProcess    = handle;
        proc.dwProcessId = get_id();
        return proc;
    }

    std::optional<uint32_t> process::wait() noexcept {
        ::WaitForSingleObject(hProcess, INFINITE);
        DWORD status = 0;
        if (!::GetExitCodeProcess(hProcess, &status)) {
            return std::nullopt;
        }
        return (uint32_t)status;
    }

    bool process::is_running() noexcept {
        DWORD status = 0;
        if (!::GetExitCodeProcess(hProcess, &status) || status != STILL_ACTIVE) {
            return false;
        }
        return ::WaitForSingleObject(hProcess, 0) != WAIT_OBJECT_0;
    }

    bool process::kill(int signum) noexcept {
        switch (signum) {
        case SIGTERM:
        case SIGKILL:
        case SIGINT:
            if (TerminateProcess(hProcess, (signum << 8))) {
                return true;
            }
            return false;
        case 0:
            return is_running();
        default:
            return false;
        }
    }

    bool process::resume() noexcept {
        return (DWORD)-1 != ::ResumeThread(hThread);
    }

    namespace pipe {
        open_result open() noexcept {
            SECURITY_ATTRIBUTES sa;
            sa.nLength              = sizeof(SECURITY_ATTRIBUTES);
            sa.bInheritHandle       = FALSE;
            sa.lpSecurityDescriptor = NULL;
            HANDLE rd               = NULL;
            HANDLE wr               = NULL;
            if (!::CreatePipe(&rd, &wr, &sa, 0)) {
                return {};
            }
            return {
                file_handle::from_native(rd),
                file_handle::from_native(wr)
            };
        }

        int peek(file_handle h) noexcept {
            DWORD rlen = 0;
            if (PeekNamedPipe(h.value(), 0, 0, 0, &rlen, 0)) {
                return rlen;
            }
            return -1;
        }
    }
}
