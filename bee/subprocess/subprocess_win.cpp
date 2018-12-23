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
    
    bool spawn::redirect(stdio type, pipe::handle h) {
        si_.dwFlags |= STARTF_USESTDHANDLES;
        inherit_handle_ = true;
        pipe::handle newh; 
        if (!::SetHandleInformation(h, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
            return false;
        }
        if (!::DuplicateHandle(::GetCurrentProcess(), h, ::GetCurrentProcess(), &newh, 0, TRUE, DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
            return false;
        }
        switch (type) {
        case stdio::eInput:
            si_.hStdInput = newh;
            break;
        case stdio::eOutput:
            si_.hStdOutput = newh;
            break;
        case stdio::eError:
            si_.hStdError = newh;
            break;
        default:
            break;
        }
        return true;
    }

    bool spawn::duplicate(const std::string& name, net::socket::fd_t fd) {
        if (sockets_.find(name) != sockets_.end()) {
            return false;
        }
        net::socket::fd_t newfd = net::socket::dup(fd);
        if (newfd == net::socket::retired_fd) {
            return false;
        }
        inherit_handle_ = true;
        sockets_[name] = newfd;
        return true;
    }

    void spawn::do_duplicate() {
        size_t size = sizeof(size_t);
        size_t n = 0;
        for (auto pair : sockets_) {
            n++;
            size += sizeof(HANDLE) + pair.first.size() + 1;
        }
        sh_.reset(new sharedmemory(create_only, format(L"bee-subprocess-dup-sockets-%d", pi_.dwProcessId).c_str(), size));
        if (!sh_->ok()) {
            return;
        }
        std::byte* data = sh_->data();
        *(size_t*)data = n;
        data += sizeof(size_t);
        for (auto pair : sockets_) {
            memcpy(data, pair.first.data(), pair.first.size() + 1);
            data += pair.first.size() + 1;
            *(net::socket::fd_t*)data = pair.second;
            data += sizeof(net::socket::fd_t);
            net::socket::close(pair.second);
        }
    }

    bool spawn::execute(const wchar_t* application, wchar_t* commandline, const wchar_t* cwd) {
        std::unique_ptr<wchar_t[]> command_line(commandline);
        std::unique_ptr<wchar_t[]> environment;
        if (!set_env_.empty() || !del_env_.empty()) {
            environment.reset(make_env(set_env_, del_env_));
            flags_ |= CREATE_UNICODE_ENVIRONMENT;
        }
        bool resume = false;
        if (!sockets_.empty()) {
            resume = !(flags_ & CREATE_SUSPENDED);
            flags_ |= CREATE_SUSPENDED;
        }
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
            return false;
        }
        ::CloseHandle(si_.hStdInput);
        ::CloseHandle(si_.hStdOutput);
        ::CloseHandle(si_.hStdError);
        if (!sockets_.empty()) {
            do_duplicate();
        }
        if (resume) {
            ::ResumeThread(pi_.hThread);
        }
        return true;
    }
   
    bool spawn::exec(const std::vector<std::wstring>& args, const wchar_t* cwd) {
        return execute(args[0].c_str(), make_args(args), cwd);
    }

    bool spawn::exec(const std::wstring& app, const std::wstring& cmd, const wchar_t* cwd) {
        return execute(app.c_str(), make_args(app, cmd), cwd);
    }

    void spawn::env_set(const std::wstring& key, const std::wstring& value) {
        set_env_[key] = value;
    }

    void spawn::env_del(const std::wstring& key) {
        del_env_.insert(key);
    }

    process::process(spawn& spawn)
        : pi_(spawn.pi_)
        , sh_(spawn.sh_.release())
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

        static std::map<std::string, net::socket::fd_t> init_sockets() {
            std::map<std::string, net::socket::fd_t> sockets;
            sharedmemory sh(open_only, format(L"bee-subprocess-dup-sockets-%d", ::GetCurrentProcessId()).c_str());
            if (!sh.ok()) {
                return  std::move(sockets);
            }
            bee::net::socket::initialize();
            std::byte* data = sh.data();
            size_t n = *(size_t*)data;
            data += sizeof(size_t);
            for (size_t i = 0; i < n; ++i) {
                std::string name((const char*)data);
                data += name.size() + 1;
                auto fd = *(net::socket::fd_t*)(data);
                data += sizeof(net::socket::fd_t);
                sockets.emplace(name, fd);
            }
            return  std::move(sockets);
        }
        std::map<std::string, net::socket::fd_t> sockets = init_sockets();
    }
}
