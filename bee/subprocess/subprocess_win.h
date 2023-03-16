#pragma once

#include <Windows.h>
#include <map>
#include <set>
#include <vector>
#include <string>
#include <bee/subprocess/common.h>
#include <bee/utility/zstring_view.h>

namespace bee::subprocess {
    namespace ignore_case {
        template <class T>
        struct less;
        template <>
        struct less<wchar_t> {
            bool operator()(const wchar_t& lft, const wchar_t& rht) const {
                return (towlower(static_cast<wint_t>(lft)) < towlower(static_cast<wint_t>(rht)));
            }
        };
        template <>
        struct less<std::wstring> {
            bool operator()(const std::wstring& lft, const std::wstring& rht) const {
                return std::lexicographical_compare(lft.begin(), lft.end(), rht.begin(), rht.end(), less<wchar_t>());
            }
        };
    }

    enum class console {
        eInherit,
        eDisable,
        eNew,
        eDetached,
        eHide,
    };
    class envbuilder {
    public:
        void set(const std::wstring& key, const std::wstring& value);
        void del(const std::wstring& key);
        environment release();

    private:
        using less = ignore_case::less<std::wstring>;
        std::map<std::wstring, std::wstring, less> set_env_;
        std::set<std::wstring, less> del_env_;
    };

    class spawn;
    class process {
    public:
        process(spawn& spawn);
        process(PROCESS_INFORMATION&& pi) { pi_ = std::move(pi); }
        ~process();
        void close();
        bool is_running();
        bool kill(int signum);
        uint32_t wait();
        uint32_t get_id() const;
        bool resume();
        uintptr_t native_handle();
        PROCESS_INFORMATION const& info() const { return pi_; }

    private:
        bool wait(uint32_t timeout);
        uint32_t exit_code();

    private:
        PROCESS_INFORMATION pi_;
    };

    struct args_t {
        void push(zstring_view v);
        void push(std::wstring&& v);
        std::wstring& operator[](size_t i) {
            return data_[i];
        }
        std::wstring const& operator[](size_t i) const {
            return data_[i];
        }
        size_t size() const {
            return data_.size();
        }

    private:
        std::vector<std::wstring> data_;
    };

    class spawn {
        friend class process;

    public:
        spawn();
        ~spawn();
        void search_path();
        void set_console(console type);
        bool hide_window();
        void suspended();
        void detached();
        void redirect(stdio type, file_handle h);
        void env(environment&& env);
        bool exec(const args_t& args, const wchar_t* cwd);

    private:
        void do_duplicate_shutdown();

    private:
        environment env_ = nullptr;
        STARTUPINFOW si_;
        PROCESS_INFORMATION pi_;
        DWORD flags_         = 0;
        console console_     = console::eInherit;
        bool inherit_handle_ = false;
        bool search_path_    = false;
        bool detached_       = false;
    };
}
