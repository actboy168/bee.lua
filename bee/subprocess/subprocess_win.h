#pragma once

#include <bee/subprocess/common.h>
#include <bee/utility/zstring_view.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace bee::subprocess {
    namespace ignore_case {
        template <class T>
        struct less;
        template <>
        struct less<wchar_t> {
            bool operator()(const wchar_t& lft, const wchar_t& rht) const noexcept {
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
        void set(const std::wstring& key, const std::wstring& value) noexcept;
        void del(const std::wstring& key) noexcept;
        environment release() noexcept;

    private:
        using less = ignore_case::less<std::wstring>;
        std::map<std::wstring, std::optional<std::wstring>, less> set_env_;
    };

    using os_handle      = void*;
    using process_id     = uint32_t;
    using process_handle = os_handle;

    class spawn;
    class process {
    public:
        process() noexcept;
        process(process&& o) noexcept;
        process(spawn& spawn) noexcept;
        ~process() noexcept;
        bool detach() noexcept;
        std::optional<process> dup() noexcept;
        bool is_running() noexcept;
        bool kill(int signum) noexcept;
        std::optional<uint32_t> wait() noexcept;
        bool resume() noexcept;
        process_id get_id() const noexcept {
            return dwProcessId;
        }
        process_handle native_handle() const noexcept {
            return hProcess;
        }
        os_handle thread_handle() const noexcept {
            return hThread;
        }

    private:
        process_handle hProcess;
        os_handle hThread;
        process_id dwProcessId;
        process_id dwThreadId;
    };

    struct args_t {
        void push(zstring_view v) noexcept;
        void push(const std::wstring& v) noexcept;
        std::wstring& operator[](size_t i) noexcept {
            return data_[i];
        }
        const std::wstring& operator[](size_t i) const noexcept {
            return data_[i];
        }
        size_t size() const noexcept {
            return data_.size();
        }

    private:
        std::vector<std::wstring> data_;
    };

    class spawn {
        friend class process;

    public:
        spawn() noexcept;
        ~spawn() noexcept;
        void search_path() noexcept;
        void set_console(console type) noexcept;
        bool hide_window() noexcept;
        void suspended() noexcept;
        void detached() noexcept;
        void redirect(stdio type, file_handle h) noexcept;
        void env(environment&& env) noexcept;
        bool exec(const args_t& args, const wchar_t* cwd) noexcept;

    private:
        environment env_ = nullptr;
        process pi_;
        os_handle fds_[3];
        uint32_t flags_      = 0;
        console console_     = console::eInherit;
        bool inherit_handle_ = false;
        bool search_path_    = false;
        bool detached_       = false;
        bool hide_window_    = false;
    };
}
