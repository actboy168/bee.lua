#pragma once

#include <stdint.h>
#include <string>
#include <stdio.h>
#include <map>
#include <set>
#include <vector>
#include <bee/net/socket.h>

namespace bee::posix::subprocess {
    enum class stdio {
        eInput,
        eOutput,
        eError,
    };

    namespace pipe {
        typedef int handle;
        enum class mode {
            eRead,
            eWrite,
        };
        struct open_result {
            handle rd;
            handle wr;
            FILE* open_file(mode m);
            operator bool() { return rd && wr; }
        };
        extern std::vector<net::socket::fd_t> sockets;
        handle      dup(FILE* f);
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
        enum class type {
            string,
            array,
        };
        type type = type::array;
        ~args_t();
    };

    class spawn {
        friend class process;
    public:
        spawn();
        ~spawn();
        void suspended();
        void redirect(stdio type, pipe::handle f);
        void duplicate(net::socket::fd_t fd);
        void env_set(const std::string& key, const std::string& value);
        void env_del(const std::string& key);
        bool exec(args_t& args, const char* cwd);
    private:
        bool raw_exec(char* const args[], const char* cwd);
        void do_duplicate();
        void do_duplicate_shutdown();
    private:
        std::map<std::string, std::string> set_env_;
        std::set<std::string>              del_env_;
        std::vector<net::socket::fd_t>     sockets_;
        int                                fds_[3];
        int                                pid_ = -1;
        bool                               suspended_ = false;
    };
}
