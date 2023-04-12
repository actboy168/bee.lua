#include <Shobjidl.h>
#include <bee/net/socket.h>
#include <bee/nonstd/bit.h>
#include <bee/nonstd/format.h>
#include <bee/nonstd/unreachable.h>
#include <bee/platform/win/unicode.h>
#include <bee/subprocess.h>
#include <bee/utility/dynarray.h>
#include <signal.h>

#include <deque>
#include <memory>
#include <thread>

#define SIGKILL 9

namespace bee::subprocess {
    void args_t::push(zstring_view v) {
        data_.emplace_back(win::u2w(v));
    }
    void args_t::push(std::wstring&& v) {
        data_.emplace_back(std::forward<std::wstring>(v));
    }

    template <class char_t>
    struct strbuilder {
        struct node {
            dynarray<char_t> data;
            size_t size;
            node(size_t maxsize)
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
        strbuilder(size_t defsize)
            : deque()
            , size(0)
            , defsize(defsize) {
            deque.emplace_back(defsize);
        }
        void append(const char_t* str, size_t n) {
            auto& back = deque.back();
            if (back.has(n)) {
                back.append(str, n);
            }
            else if (n >= defsize) {
                deque.emplace_back(n).append(str, n);
            }
            else {
                deque.emplace_back(defsize).append(str, n);
            }
            size += n;
        }
        template <class T, size_t n>
        strbuilder& operator+=(T (&str)[n]) {
            append(str, n - 1);
            return *this;
        }
        strbuilder& operator+=(const std::basic_string_view<char_t>& s) {
            append(s.data(), s.size());
            return *this;
        }
        dynarray<char_t> string() {
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
        ~EnvironmentStrings() {
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
    std::basic_string<char_t> quote_arg(const std::basic_string<char_t>& source) {
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
            }
            else if (source[i - 1] == '"') {
                quote_hit = 1;
                target += '\\';
            }
            else {
                quote_hit = 0;
            }
        }
        target += '"';
        for (size_t i = 0; i < target.size() / 2; ++i) {
            std::swap(target[i], target[target.size() - i - 1]);
        }
        return target;
    }

    static dynarray<wchar_t> make_args(const args_t& args, std::wstring_view prefix = std::wstring_view()) {
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

    void envbuilder::set(const std::wstring& key, const std::wstring& value) {
        set_env_[key] = value;
    }

    void envbuilder::del(const std::wstring& key) {
        del_env_.insert(key);
    }

    environment envbuilder::release() {
        EnvironmentStrings es;
        if (!es) {
            return nullptr;
        }
        if (set_env_.empty() && del_env_.empty()) {
            return nullptr;
        }
        strbuilder<wchar_t> res(1024);
        wchar_t* escp = es;
        while (*escp != L'\0') {
            std::wstring str                  = escp;
            const std::wstring::size_type pos = str.find(L'=');
            std::wstring key                  = str.substr(0, pos);
            if (del_env_.find(key) != del_env_.end()) {
                escp += str.length() + 1;
                continue;
            }
            std::wstring val = str.substr(pos + 1, str.length());
            const auto it    = set_env_.find(key);
            if (it != set_env_.end()) {
                val = it->second;
                set_env_.erase(it);
            }
            res += key;
            res += L"=";
            res += val;
            res += L"\0";
            escp += str.length() + 1;
        }
        for (auto& e : set_env_) {
            const std::wstring& key = e.first;
            const std::wstring& val = e.second;
            if (del_env_.find(key) != del_env_.end()) {
                continue;
            }
            res += key;
            res += L"=";
            res += val;
            res += L"\0";
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

    static bool hide_taskbar(HWND w) {
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

    static bool hide_console(PROCESS_INFORMATION& pi) {
        HANDLE hProcess = NULL;
        if (!::DuplicateHandle(::GetCurrentProcess(), pi.hProcess, ::GetCurrentProcess(), &hProcess, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
            return false;
        }
        std::thread thd([=]() {
            PROCESS_INFORMATION cpi;
            cpi.dwProcessId = pi.dwProcessId;
            cpi.dwThreadId  = pi.dwThreadId;
            cpi.hThread     = NULL;
            cpi.hProcess    = hProcess;
            process process(std::move(cpi));
            for (;; std::this_thread::sleep_for(std::chrono::milliseconds(10))) {
                if (!process.is_running()) {
                    return;
                }
                HWND wnd = console_window(process.get_id());
                if (wnd) {
                    SetWindowPos(wnd, NULL, -10000, -10000, 0, 0, SWP_HIDEWINDOW);
                    hide_taskbar(wnd);
                    return;
                }
            }
        });
        thd.detach();
        return true;
    }

    spawn::spawn() noexcept {
        memset(&si_, 0, sizeof(STARTUPINFOW));
        memset(&pi_, 0, sizeof(PROCESS_INFORMATION));
        si_.cb         = sizeof(STARTUPINFOW);
        si_.dwFlags    = 0;
        si_.hStdInput  = INVALID_HANDLE_VALUE;
        si_.hStdOutput = INVALID_HANDLE_VALUE;
        si_.hStdError  = INVALID_HANDLE_VALUE;
    }

    spawn::~spawn() {
        ::CloseHandle(pi_.hThread);
        ::CloseHandle(pi_.hProcess);
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
        si_.dwFlags |= STARTF_USESHOWWINDOW;
        si_.wShowWindow = SW_HIDE;
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
        si_.dwFlags |= STARTF_USESTDHANDLES;
        inherit_handle_ = true;
        switch (type) {
        case stdio::eInput:
            si_.hStdInput = h.value();
            break;
        case stdio::eOutput:
            si_.hStdOutput = h.value();
            break;
        case stdio::eError:
            si_.hStdError = h.value();
            break;
        default:
            std::unreachable();
        }
    }

    void spawn::do_duplicate_shutdown() noexcept {
        ::CloseHandle(si_.hStdInput);
        ::CloseHandle(si_.hStdOutput);
        if (si_.hStdError != INVALID_HANDLE_VALUE && si_.hStdOutput != si_.hStdError) {
            ::CloseHandle(si_.hStdError);
        }
    }

    bool spawn::exec(const args_t& args, const wchar_t* cwd) {
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
        ::SetHandleInformation(si_.hStdInput, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        ::SetHandleInformation(si_.hStdOutput, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        ::SetHandleInformation(si_.hStdError, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        if (!::CreateProcessW(application, command_line.data(), NULL, NULL, inherit_handle_, flags_ | NORMAL_PRIORITY_CLASS, env_, cwd, &si_, &pi_)) {
            do_duplicate_shutdown();
            return false;
        }
        do_duplicate_shutdown();
        if (!detached_) {
            join_job(pi_.hProcess);
        }
        if (console_ == console::eHide) {
            hide_console(pi_);
        }
        return true;
    }

    void spawn::env(environment&& env) noexcept {
        env_ = std::move(env);
    }

    process::process(spawn& spawn) noexcept
        : pi_(spawn.pi_) {
        memset(&spawn.pi_, 0, sizeof(PROCESS_INFORMATION));
    }

    process::~process() noexcept {
        close();
    }

    void process::close() noexcept {
        if (pi_.hThread != NULL) {
            ::CloseHandle(pi_.hThread);
            pi_.hThread = NULL;
        }
        if (pi_.hProcess != NULL) {
            ::CloseHandle(pi_.hProcess);
            pi_.hProcess = NULL;
        }
    }

    std::optional<uint32_t> process::wait() noexcept {
        ::WaitForSingleObject(pi_.hProcess, INFINITE);
        DWORD status = 0;
        if (!::GetExitCodeProcess(pi_.hProcess, &status)) {
            return std::nullopt;
        }
        return (uint32_t)status;
    }

    bool process::is_running() noexcept {
        DWORD status = 0;
        if (!::GetExitCodeProcess(pi_.hProcess, &status) || status != STILL_ACTIVE) {
            return false;
        }
        return ::WaitForSingleObject(pi_.hProcess, 0) != WAIT_OBJECT_0;
    }

    bool process::kill(int signum) noexcept {
        switch (signum) {
        case SIGTERM:
        case SIGKILL:
        case SIGINT:
            if (TerminateProcess(pi_.hProcess, (signum << 8))) {
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
        return (DWORD)-1 != ::ResumeThread(pi_.hThread);
    }

    process_id process::get_id() const noexcept {
        return pi_.dwProcessId;
    }

    process_handle process::native_handle() const noexcept {
        return pi_.hProcess;
    }

    namespace pipe {
        open_result open() noexcept {
            net::socket::fd_t fds[2];
            if (!net::socket::pipe(fds, net::socket::fd_flags::none)) {
                return { {}, {} };
            }
            return { { (bee::file_handle::value_type)fds[0] }, { (bee::file_handle::value_type)fds[1] } };
        }

        int peek(FILE* f) noexcept {
            DWORD rlen = 0;
            if (PeekNamedPipe(file_handle::from_file(f).value(), 0, 0, 0, &rlen, 0)) {
                return rlen;
            }
            return -1;
        }
    }
}
