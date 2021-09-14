#pragma once

#include <stdint.h>
#include <string>
#include <stdio.h>
#include <map>
#include <set>
#include <vector>
#include <bee/utility/file_helper.h>
#include <spawn.h>

namespace bee::posix::subprocess {
    enum class stdio {
        eInput,
        eOutput,
        eError,
    };

    namespace pipe {
        struct open_result {
            file::handle rd;
            file::handle wr;
            FILE*        open_read();
            FILE*        open_write();
            operator bool() { return rd && wr; }
        };
        open_result open();
        int         peek(FILE* f);
    }

    class spawn;
    class process {
    public:
        process(spawn& spawn);
        bool      is_running();
        bool      kill(int signum);
        uint32_t  wait();
        uint32_t  get_id() const;
        bool      resume();
        uintptr_t native_handle();

        int pid;
        int status = 0;
    };

    struct args_t : public std::vector<char*> {
        ~args_t();
        void push(char* str);
        void push(const std::string& str);
    };

    class spawn {
        friend class process;
    public:
        spawn();
        ~spawn();
        void suspended();
        void detached();
        void redirect(stdio type, file::handle f);
        void env_set(const std::string& key, const std::string& value);
        void env_del(const std::string& key);
        bool exec(args_t& args, const char* cwd);
    private:
        bool raw_exec(char* const args[], const char* cwd);
    private:
        std::map<std::string, std::string> set_env_;
        std::set<std::string>              del_env_;
        int                                fds_[3];
        int                                pid_ = -1;
        posix_spawnattr_t                  spawnattr_;
        posix_spawn_file_actions_t         spawnfile_;
    };
}
