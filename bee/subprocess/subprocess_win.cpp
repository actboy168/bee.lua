#include <bee/subprocess.h>
#include <bee/format.h>
#include <Windows.h>
#include <Shobjidl.h>
#include <memory>
#include <thread>
#include <deque>
#include <assert.h>
#include <fcntl.h>
#include <io.h>
#include <signal.h>

#define SIGKILL 9

namespace bee::win::subprocess {
    template <class char_t>
    struct strbuilder {
        struct node {
            size_t size;
            size_t maxsize;
            char_t* data;
            node(size_t maxsize)
                : size(0)
                , maxsize(maxsize)
                , data(new char_t[maxsize])
            { }
            ~node() {
                delete[] data;
            }
            char_t* release() {
                char_t* r = data;
                data = nullptr;
                return r;
            }
            bool append(const char_t* str, size_t n) {
                if (size + n > maxsize) {
                    return false;
                }
                memcpy(data + size, str, n * sizeof(char_t));
                size += n;
                return true;
            }
            template <class T, size_t n>
            void operator +=(T(&str)[n]) {
                append(str, n - 1);
            }
            void operator +=(const std::basic_string_view<char_t>& str) {
                append(str.data(), str.size());
            }
        };
        strbuilder() : size(0) { }
        void clear() {
            size = 0;
            data.clear();
        }
        bool append(const char_t* str, size_t n) {
            if (!data.empty() && data.back().append(str, n)) {
                size += n;
                return true;
            }
            size_t m = 1024;
            while (m < n) {
                m *= 2;
            }
            data.emplace_back(m).append(str, n);
            size += n;
            return true;
        }
        template <class T, size_t n>
        strbuilder& operator +=(T(&str)[n]) {
            append(str, n - 1);
            return *this;
        }
        strbuilder& operator +=(const std::basic_string_view<char_t>& s) {
            append(s.data(), s.size());
            return *this;
        }
        char_t* string() {
            node r(size + 1);
            for (auto& s : data) {
                r.append(s.data, s.size);
            }
            char_t empty[] = { '\0' };
            r.append(empty, 1);
            return r.release();
        }
        std::deque<node> data;
        size_t size;
    };

    template <class char_t>
    inline std::basic_string<char_t> quote_arg(const std::basic_string<char_t>& source) {
        size_t len = source.size();
        if (len == 0) {
            return {'\"','\"','\0'};
        }
        if (std::basic_string<char_t>::npos == source.find_first_of({' ','\t','\"'})) {
            return source;
        }
        if (std::basic_string<char_t>::npos == source.find_first_of({'\"','\\'})) {
            return std::basic_string<char_t>({'\"'}) + source + std::basic_string<char_t>({'\"'});
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

    static wchar_t* make_args(const args_t& args, std::wstring_view prefix = std::wstring_view()) {
        strbuilder<wchar_t> res;
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

    static wchar_t* make_env(std::map<std::wstring, std::wstring, ignore_case::less<std::wstring>>& set, std::set<std::wstring, ignore_case::less<std::wstring>>& del) {
        wchar_t* es = GetEnvironmentStringsW();
        if (es == 0) {
            return nullptr;
        }
        try {
            strbuilder<wchar_t> res;
            wchar_t* escp = es;
            while (*escp != L'\0') {
                std::wstring str = escp;
                std::wstring::size_type pos = str.find(L'=');
                std::wstring key = str.substr(0, pos);
                if (del.find(key) != del.end()) {
                    escp += str.length() + 1;
                    continue;
                }
                std::wstring val = str.substr(pos + 1, str.length());
                auto it = set.find(key);
                if (it != set.end()) {
                    val = it->second;
                    set.erase(it);
                }
                res += key;
                res += L"=";
                res += val;
                res += L"\0";

                escp += str.length() + 1;
            }
            for (auto& e : set) {
                const std::wstring& key = e.first;
                const std::wstring& val = e.second;
                if (del.find(key) != del.end()) {
                    continue;
                }
                res += key;
                res += L"=";
                res += val;
                res += L"\0";
            }
            return res.string();
        }
        catch (...) {
        }
        FreeEnvironmentStringsW(es);
        return nullptr;
    }

    static HANDLE create_job() {
        SECURITY_ATTRIBUTES attr;
        memset(&attr, 0, sizeof attr);
        attr.bInheritHandle = FALSE;

        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info;
        memset(&info, 0, sizeof info);
        info.BasicLimitInformation.LimitFlags =
            JOB_OBJECT_LIMIT_BREAKAWAY_OK
            | JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK
            | JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
            | JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION
            ;

        HANDLE job = CreateJobObjectW(&attr, NULL);
        if (job == NULL) {
            return NULL;
        }
        if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof info)) {
            return NULL;
        }
        return job;
    }

    static bool join_job(HANDLE process) {
        static HANDLE job = create_job();
        if (!job) {
            return false;
        }
        if (!AssignProcessToJobObject(job, process)) {
            DWORD err = GetLastError();
            if (err != ERROR_ACCESS_DENIED) {
                return false;
            }
        }
        return true;
    }

    static HWND console_window(DWORD pid) {
        DWORD wpid;
        HWND wnd = NULL;
        do {
            wnd = FindWindowExW(NULL, wnd, L"ConsoleWindowClass", NULL);
            if (wnd == NULL) {
                break;
            }
            GetWindowThreadProcessId(wnd, &wpid);
        } while (pid != wpid);
        return wnd;
    }

    bool hide_taskbar(HWND w) {
        ITaskbarList* taskbar;
        ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
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
        if (!::DuplicateHandle(
            ::GetCurrentProcess(),
            pi.hProcess,
            ::GetCurrentProcess(),
            &hProcess,
            0, FALSE, DUPLICATE_SAME_ACCESS)
            ) {
            return false;
        }

        std::thread thd([=]() {
            PROCESS_INFORMATION cpi;
            cpi.dwProcessId = pi.dwProcessId;
            cpi.dwThreadId = pi.dwThreadId;
            cpi.hThread = NULL;
            cpi.hProcess = hProcess;
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

    spawn::spawn()
    {
        memset(&si_, 0, sizeof(STARTUPINFOW));
        memset(&pi_, 0, sizeof(PROCESS_INFORMATION));
        si_.cb = sizeof(STARTUPINFOW);
        si_.dwFlags = 0;
        si_.hStdInput = INVALID_HANDLE_VALUE;
        si_.hStdOutput = INVALID_HANDLE_VALUE;
        si_.hStdError = INVALID_HANDLE_VALUE;
    }

    spawn::~spawn() {
        ::CloseHandle(pi_.hThread);
        ::CloseHandle(pi_.hProcess);
    }

    void spawn::search_path() {
        search_path_ = true;
    }

    bool spawn::set_console(console type) {
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
            return false;
        }
        return true;
    }

    bool spawn::hide_window() {
        si_.dwFlags |= STARTF_USESHOWWINDOW;
        si_.wShowWindow = SW_HIDE;
        return true;
    }

    void spawn::suspended() {
        flags_ |= CREATE_SUSPENDED;
    }

    void spawn::detached() {
        set_console(console::eDetached);
        detached_ = true;
    }

    void spawn::redirect(stdio type, file::handle h) {
        si_.dwFlags |= STARTF_USESTDHANDLES;
        inherit_handle_ = true;
        switch (type) {
        case stdio::eInput:
            si_.hStdInput = h;
            break;
        case stdio::eOutput:
            si_.hStdOutput = h;
            break;
        case stdio::eError:
            si_.hStdError = h;
            break;
        default:
            break;
        }
    }

    void spawn::do_duplicate_shutdown() {
        ::CloseHandle(si_.hStdInput);
        ::CloseHandle(si_.hStdOutput);
        if (si_.hStdError != INVALID_HANDLE_VALUE && si_.hStdOutput != si_.hStdError) {
            ::CloseHandle(si_.hStdError);
        }
    }

    bool spawn::raw_exec(const wchar_t* application, wchar_t* commandline, const wchar_t* cwd) {
        std::unique_ptr<wchar_t[]> command_line(commandline);
        std::unique_ptr<wchar_t[]> environment;
        if (!set_env_.empty() || !del_env_.empty()) {
            environment.reset(make_env(set_env_, del_env_));
            flags_ |= CREATE_UNICODE_ENVIRONMENT;
        }
        ::SetHandleInformation(si_.hStdInput, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        ::SetHandleInformation(si_.hStdOutput, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        ::SetHandleInformation(si_.hStdError, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        if (!::CreateProcessW(
            application,
            command_line.get(),
            NULL, NULL,
            inherit_handle_,
            flags_ | NORMAL_PRIORITY_CLASS,
            environment.get(),
            cwd,
            &si_, &pi_
        ))
        {
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

    bool spawn::exec(const args_t& args, const wchar_t* cwd) {
        if (args.size() == 0) {
            return false;
        }
        wchar_t* command = make_args(args);
        if (!command) {
            return false;
        }
        return raw_exec(search_path_ ? 0 : args[0].c_str(), command, cwd);
    }

    void spawn::env_set(const std::wstring& key, const std::wstring& value) {
        set_env_[key] = value;
    }

    void spawn::env_del(const std::wstring& key) {
        del_env_.insert(key);
    }

    process::process(spawn& spawn)
        : pi_(spawn.pi_)
    {
        memset(&spawn.pi_, 0, sizeof(PROCESS_INFORMATION));
    }

    process::~process() {
        close();
    }

    void process::close() {
        if (pi_.hThread != NULL) {
            ::CloseHandle(pi_.hThread);
            pi_.hThread = NULL;
        }
        if (pi_.hProcess != NULL) {
            ::CloseHandle(pi_.hProcess);
            pi_.hProcess = NULL;
        }
    }

    uint32_t process::wait() {
        wait(INFINITE);
        return exit_code();
    }

    bool process::wait(uint32_t timeout) {
        return ::WaitForSingleObject(pi_.hProcess, timeout) == WAIT_OBJECT_0;
    }

    bool process::is_running() {
        if (exit_code() == STILL_ACTIVE) {
            return !wait(0);
        }
        return false;
    }

    bool process::kill(int signum) {
        switch (signum) {
        case SIGTERM:
        case SIGKILL:
        case SIGINT:
            if (TerminateProcess(pi_.hProcess, (signum << 8))) {
                return wait(5000);
            }
            return false;
        case 0:
            return is_running();
        default:
            return false;
        }
    }

    bool process::resume() {
        return (DWORD)-1 != ::ResumeThread(pi_.hThread);
    }

    uint32_t process::exit_code() {
        DWORD ret = 0;
        if (!::GetExitCodeProcess(pi_.hProcess, &ret)) {
            return 0;
        }
        return (uint32_t)ret;
    }

    uint32_t process::get_id() const {
        return (uint32_t)pi_.dwProcessId;
    }

    uintptr_t process::native_handle() {
        return (uintptr_t)pi_.hProcess;
    }

    namespace pipe {
        FILE* open_result::open_read() {
            return file::open_read(rd);
        }

        FILE* open_result::open_write() {
            return file::open_write(wr);
        }

        open_result open() {
            SECURITY_ATTRIBUTES sa;
            sa.nLength = sizeof(SECURITY_ATTRIBUTES);
            sa.bInheritHandle = FALSE;
            sa.lpSecurityDescriptor = NULL;
            HANDLE read_pipe = NULL, write_pipe = NULL;
            if (!::CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
                return { file::handle::invalid(), file::handle::invalid() };
            }
            return { file::handle(read_pipe), file::handle(write_pipe) };
        }

        int peek(FILE* f) {
            DWORD rlen = 0;
            if (PeekNamedPipe(file::get_handle(f), 0, 0, 0, &rlen, 0)) {
                return rlen;
            }
            return -1;
        }
    }
}
