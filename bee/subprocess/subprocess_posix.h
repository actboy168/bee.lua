#pragma once

#include <bee/subprocess/common.h>
#include <bee/utility/zstring_view.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace bee::subprocess {
    class envbuilder {
    public:
        void set(const std::string& key, const std::string& value) noexcept;
        void del(const std::string& key) noexcept;
        environment release() noexcept;

    private:
        std::map<std::string, std::optional<std::string>> set_env_;
    };

    using process_id     = pid_t;
    using process_handle = pid_t;

    class spawn;
    class process {
    public:
        process(spawn& spawn) noexcept;
        ~process() noexcept;
        bool detach() noexcept;
        process_id get_id() const noexcept;
        process_handle native_handle() const noexcept;
        bool kill(int signum) noexcept;
        bool is_running() noexcept;
        std::optional<uint32_t> wait() noexcept;
        bool resume() noexcept;
        pid_t pid;
        std::optional<uint32_t> status;
    };

    struct args_t {
        ~args_t() noexcept;
        void push(char* str) noexcept;
        void push(zstring_view str) noexcept;
        char*& operator[](size_t i) noexcept {
            return data_[i];
        }
        char* const& operator[](size_t i) const noexcept {
            return data_[i];
        }
        char* const* data() const noexcept {
            return data_.data();
        }
        char** data() noexcept {
            return data_.data();
        }
        size_t size() const noexcept {
            return data_.size();
        }

    private:
        std::vector<char*> data_;
    };

    class spawn {
        friend class process;

    public:
        spawn() noexcept;
        void suspended() noexcept;
        void detached() noexcept;
        void redirect(stdio type, file_handle f) noexcept;
        void env(environment&& env) noexcept;
        bool exec(args_t& args, const char* cwd) noexcept;

    private:
        environment env_ = nullptr;
        int fds_[3];
        pid_t pid_       = -1;
        short spawnattr_ = 0;
    };
}
