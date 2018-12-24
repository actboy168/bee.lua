#include <bee/subprocess.h>
#include <bee/subprocess/sharedmemory_win.h>
#include <bee/nonstd/span.h>
#include <bee/utility/format.h>
#include <Windows.h>
#include <memory>
#include <deque>
#include <assert.h>
#include <fcntl.h>
#include <io.h>
#include <signal.h>

#define SIGKILL 9

namespace bee::win::subprocess {

    struct strbuilder {
        struct node {
            size_t size;
            size_t maxsize;
            wchar_t* data;
            node(size_t maxsize)
                : size(0)
                , maxsize(maxsize)
                , data(new wchar_t[maxsize])
            { }
            ~node() {
                delete[] data;
            }
            wchar_t* release() {
                wchar_t* r = data;
                data = nullptr;
                return r;
            }
            bool append(const wchar_t* str, size_t n) {
                if (size + n > maxsize) {
                    return false;
                }
                memcpy(data + size, str, n * sizeof(wchar_t));
                size += n;
                return true;
            }
            template <class T, size_t n>
            void operator +=(T(&str)[n]) {
                append(str, n - 1);
            }
        };
        strbuilder() : size(0) { }
        void clear() {
            size = 0;
            data.clear();
        }
        bool append(const wchar_t* str, size_t n) {
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
        strbuilder& operator +=(const std::wstring& s) {
            append(s.data(), s.size());
            return *this;
        }
        wchar_t* string() {
            node r(size + 1);
            for (auto& s : data) {
                r.append(s.data, s.size);
            }
            r += L"\0";
            return r.release();
        }
        std::deque<node> data;
        size_t size;
    };

    static std::wstring quote_arg(const std::wstring& source) {
        size_t len = source.size();
        if (len == 0) {
            return L"\"\"";
        }
        if (std::wstring::npos == source.find_first_of(L" \t\"")) {
            return source;
        }
        if (std::wstring::npos == source.find_first_of(L"\"\\")) {
            return L"\"" + source + L"\"";
        }
        std::wstring target;
        target += L'"';
        int quote_hit = 1;
        for (size_t i = len; i > 0; --i) {
            target += source[i - 1];

            if (quote_hit && source[i - 1] == L'\\') {
                target += L'\\';
            }
            else if (source[i - 1] == L'"') {
                quote_hit = 1;
                target += L'\\';
            }
            else {
                quote_hit = 0;
            }
        }
        target += L'"';
        for (size_t i = 0; i < target.size() / 2; ++i) {
            std::swap(target[i], target[target.size() - i - 1]);
        }
        return target;
    }

    static wchar_t* make_args(const std::vector<std::wstring>& args) {
        strbuilder res; 
        for (size_t i = 0; i < args.size(); ++i) {
            res += quote_arg(args[i]);
            if (i + 1 != args.size()) {
                res += L" ";
            }
        }
        return res.string();
    }

    static wchar_t* make_args(const std::wstring& app, const std::wstring& cmd) {
        strbuilder res;
        res += quote_arg(app);
        res += L" ";
        res += cmd;
        return res.string();
    }

    static wchar_t* make_env(std::map<std::wstring, std::wstring, ignore_case::less<std::wstring>>& set, std::set<std::wstring, ignore_case::less<std::wstring>>& del)
    {
        wchar_t* es = GetEnvironmentStringsW();
        if (es == 0) {
            return nullptr;
        }
        try {
            strbuilder res;
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

    spawn::spawn()
        : inherit_handle_(false)
        , flags_(0)
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

    bool spawn::set_console(console type) {
        flags_ &= ~(CREATE_NO_WINDOW | CREATE_NEW_CONSOLE);
        switch (type) {
        case console::eInherit:
            break;
        case console::eDisable:
            flags_ |= CREATE_NO_WINDOW;
            break;
        case console::eNew:
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
    
    void spawn::redirect(stdio type, pipe::handle h) {
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

    void spawn::duplicate(net::socket::fd_t fd) {
        inherit_handle_ = true;
        sockets_.push_back(fd);
    }

    void spawn::do_duplicate_start(bool& resume) {
        ::SetHandleInformation(si_.hStdInput, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        ::SetHandleInformation(si_.hStdOutput, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        ::SetHandleInformation(si_.hStdError, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        if (sockets_.empty()) {
            return;
        }
        resume = !(flags_ & CREATE_SUSPENDED);
        flags_ |= CREATE_SUSPENDED;
        for (auto& fd : sockets_) {
            fd = net::socket::dup(fd);
        }
    }

    void spawn::do_duplicate_shutdown() {
        ::CloseHandle(si_.hStdInput);
        ::CloseHandle(si_.hStdOutput);
        ::CloseHandle(si_.hStdError);
        for (auto& fd : sockets_) {
            if (fd != net::socket::retired_fd) {
                net::socket::close(fd);
                fd = net::socket::retired_fd;
            }
        }
    }

    void spawn::do_duplicate_finish() {
        if (sockets_.empty()) {
            return;
        }
        sharedmemory sh(create_only
            , format(L"bee-subprocess-dup-sockets-%d", pi_.dwProcessId).c_str()
            , sizeof(HANDLE) + sizeof(size_t) + sockets_.size() * sizeof(net::socket::fd_t)
        );
        if (!sh.ok()) {
            return;
        }
        std::byte* data = sh.data();
        if (!::DuplicateHandle(
            ::GetCurrentProcess(),
            sh.handle(),
            pi_.hProcess,
            (HANDLE*)data,
            0, FALSE, DUPLICATE_SAME_ACCESS)
            ) {
            return;
        }
        data += sizeof(HANDLE);
        *(size_t*)data = sockets_.size();
        data += sizeof(size_t);
        net::socket::fd_t* fds = (net::socket::fd_t*)data;
        for (auto& fd : sockets_) {
            *fds++ = fd;
        }
    }

    bool spawn::raw_exec(const wchar_t* application, wchar_t* commandline, const wchar_t* cwd) {
        std::unique_ptr<wchar_t[]> command_line(commandline);
        std::unique_ptr<wchar_t[]> environment;
        if (!set_env_.empty() || !del_env_.empty()) {
            environment.reset(make_env(set_env_, del_env_));
            flags_ |= CREATE_UNICODE_ENVIRONMENT;
        }
        bool resume = false;
        do_duplicate_start(resume);
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
        do_duplicate_finish();
        do_duplicate_shutdown();
        if (resume) {
            ::ResumeThread(pi_.hThread);
        }
        return true;
    }

    bool spawn::exec(const args_t& args, const wchar_t* cwd) {
        switch (args.type) {
        case args_t::type::array:
            return raw_exec(args[0].c_str(), make_args(args), cwd);
        case args_t::type::string:
            return raw_exec(args[0].c_str(), make_args(args[0], args[1]), cwd);
        default:
            return false;
        }
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
        ::CloseHandle(pi_.hThread);
        ::CloseHandle(pi_.hProcess);
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
        return (int32_t)ret;
    }

    uint32_t process::get_id() const {
        return (uint32_t)pi_.dwProcessId;
    }

    uintptr_t process::native_handle() {
        return (uintptr_t)pi_.hProcess;
    }

    namespace pipe {
        FILE* open_result::open_file(mode m) {
            switch (m) {
            case mode::eRead:
                return _fdopen(_open_osfhandle((intptr_t)rd, _O_RDONLY | _O_BINARY), "rb");
            case mode::eWrite:
                return _fdopen(_open_osfhandle((intptr_t)wr, _O_WRONLY | _O_BINARY), "wb");
            default:
                assert(false);
                return 0;
            }
        }

        static handle to_handle(FILE* f) {
            int n = _fileno(f);
            return (n >= 0) ? (HANDLE)_get_osfhandle(n) : INVALID_HANDLE_VALUE;
        }

        handle dup(FILE* f) {
            handle h = to_handle(f);
            if (h == INVALID_HANDLE_VALUE) {
                return 0;
            }
            handle newh;
            if (!::DuplicateHandle(::GetCurrentProcess(), h, ::GetCurrentProcess(), &newh, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
                return 0;
            }
            return newh;
        }

        open_result open() {
            SECURITY_ATTRIBUTES sa;
            sa.nLength = sizeof(SECURITY_ATTRIBUTES);
            sa.bInheritHandle = FALSE;
            sa.lpSecurityDescriptor = NULL;
            HANDLE read_pipe = NULL, write_pipe = NULL;
            if (!::CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
                return { NULL, NULL };
            }
            return { read_pipe, write_pipe };
        }

        int peek(FILE* f) {
            DWORD rlen = 0;
            if (PeekNamedPipe(to_handle(f), 0, 0, 0, &rlen, 0)) {
                return rlen;
            }
            return -1;
        }

        static std::vector<net::socket::fd_t> init_sockets() {
            std::vector<net::socket::fd_t> sockets;
            sharedmemory sh(open_only
                , format(L"bee-subprocess-dup-sockets-%d", ::GetCurrentProcessId()).c_str()
            );
            if (!sh.ok()) {
                return sockets;
            }
            std::byte* data = sh.data();
            HANDLE mapping = *(HANDLE*)data;
            ::CloseHandle(mapping);
            data += sizeof(HANDLE);
            size_t n = *(size_t*)data;
            data += sizeof(size_t);
            net::socket::fd_t* fds = (net::socket::fd_t*)data;
            for (size_t i = 0; i < n; ++i) {
                sockets.push_back(*fds++);
            }
            return sockets;
        }
        std::vector<net::socket::fd_t> sockets = init_sockets();
    }
}
