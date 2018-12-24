#pragma once

#include <Windows.h>
#include <map>
#include <set>
#include <vector>
#include <string>
#include <memory>
#include <bee/net/socket.h>

namespace bee::win::subprocess {
    namespace ignore_case {
        template <class T> struct less;
        template <> struct less<wchar_t> {
            bool operator()(const wchar_t& lft, const wchar_t& rht) const
            {
                return (towlower(static_cast<wint_t>(lft)) < towlower(static_cast<wint_t>(rht)));
            }
        };
        template <> struct less<std::wstring> {
            bool operator()(const std::wstring& lft, const std::wstring& rht) const
            {
                return std::lexicographical_compare(lft.begin(), lft.end(), rht.begin(), rht.end(), less<wchar_t>());
            }
        };
    }

    enum class console {
        eInherit,
        eDisable,
        eNew,
    };
    enum class stdio {
        eInput,
        eOutput,
        eError,
    };

    namespace pipe {
        typedef HANDLE handle;
        enum class mode {
            eRead,
            eWrite,
        };
        struct open_result {
            handle rd;
            handle wr;
            FILE*  open_file(mode m);
            operator bool() { return rd && wr; }
        };
        extern std::vector<net::socket::fd_t> sockets;
        handle dup(FILE* f);
        _BEE_API open_result open();
        _BEE_API int         peek(FILE* f);
    }

    class sharedmemory;
    class spawn;
    class _BEE_API process {
    public:
        process(spawn& spawn);
        process(PROCESS_INFORMATION&& pi) { pi_ = std::move(pi); }
        ~process();
        bool      is_running();
        bool      kill(int signum);
        uint32_t  wait();
        uint32_t  get_id() const;
        bool      resume();
        uintptr_t native_handle();
        PROCESS_INFORMATION const& info() const { return pi_; }

    private:
        bool     wait(uint32_t timeout);
        uint32_t exit_code();

    private:
        PROCESS_INFORMATION           pi_;
    };

    struct args_t : public std::vector<std::wstring> {
        enum class type {
            string,
            array,
        };
        type type;
    };

    class _BEE_API spawn {
        friend class process;
    public:
        spawn();
        ~spawn();
        bool set_console(console type);
        bool hide_window();
        void suspended();
        void redirect(stdio type, pipe::handle h);
        void duplicate(net::socket::fd_t fd);
        void env_set(const std::wstring& key, const std::wstring& value);
        void env_del(const std::wstring& key);
        bool exec(const args_t& args, const wchar_t* cwd);

    private:
        bool raw_exec(const wchar_t* application, wchar_t* commandline, const wchar_t* cwd);
        void do_duplicate_start(bool& resume);
        void do_duplicate_shutdown();
        void do_duplicate_finish();

    private:
        std::map<std::wstring, std::wstring, ignore_case::less<std::wstring>> set_env_;
        std::set<std::wstring, ignore_case::less<std::wstring>>               del_env_;
        std::vector<net::socket::fd_t>                                        sockets_;
        STARTUPINFOW            si_;
        PROCESS_INFORMATION     pi_;
        bool                    inherit_handle_;
        DWORD                   flags_;
    };
}
