#include <bee/subprocess.h>
#include <bee/format.h>
#include <bee/net/socket.h>
#include <memory.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <vector>

#if defined(__APPLE__)
# include <crt_externs.h>
# define environ (*_NSGetEnviron())
#else
extern char **environ;
#endif

namespace bee::posix::subprocess {

    args_t::~args_t() {
        for (size_t i = 0; i < size(); ++i) {
            delete[]((*this)[i]);
        }
    }

    void args_t::push(char* str) {
        push_back(str);
    }

    void args_t::push(const std::string& str) {
        std::unique_ptr<char[]> tmp(new char[str.size() + 1]);
        memcpy(tmp.get(), str.data(), str.size() + 1);
        push(tmp.release());
    }

    static void sigalrm_handler (int) {
        // Nothing to do
    }
    static bool wait_with_timeout(pid_t pid, int *status, int timeout) {
        assert(pid > 0);
        assert(timeout >= -1);
        if (timeout <= 0) {
            if (timeout == -1) {
                ::waitpid(pid, status, 0);
            }
            return 0 != ::waitpid(pid, status, WNOHANG);
        }
        pid_t err;
        struct sigaction sa, old_sa;
        sa.sa_handler = sigalrm_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        ::sigaction(SIGALRM, &sa, &old_sa);
        ::alarm(timeout);
        err = ::waitpid(pid, status, 0);
        ::alarm(0);
        ::sigaction(SIGALRM, &old_sa, NULL);
        return err == pid;
    }


    struct envbuilder {
        struct env {
            env(char** v)
                : v(v)
            {}
            ~env() {
                if (!v) {
                    return;
                }
                for (char** p = v; *p; ++p) {
                    delete[] (*p);
                }
                delete[] v;
            }
            operator char**() const {
                return v;
            }
            char** v;
        };
    
        std::vector<char*> data;
        env release() {
            data.emplace_back(nullptr);
            char** r = new char*[data.size()];
            memcpy(r, data.data(), data.size() * sizeof(char*));
            return r;
        }
        void append(const std::string& k, const std::string& v) {
            size_t n = k.size() + v.size() + 2;
            char* str = new char[n];
            memcpy(str, k.data(), k.size());
            str[k.size()] = '=';
            memcpy(str+k.size()+1, v.data(), v.size());
            str[n-1] = '\0';
            data.emplace_back(str);
        }
    };


    static envbuilder::env make_env(std::map<std::string, std::string>& set, std::set<std::string>& del) {
        char** es = environ;
        if (es == 0) {
            return nullptr;
        }
        envbuilder envs;
        for (; *es; ++es) {
            std::string str = *es;
            std::string::size_type pos = str.find(L'=');
            std::string key = str.substr(0, pos);
            if (del.find(key) != del.end()) {
                continue;
            }
            std::string val = str.substr(pos + 1, str.length());
            auto it = set.find(key);
            if (it != set.end()) {
                val = it->second;
                set.erase(it);
            }
            envs.append(key, val);
        }
        for (auto& e : set) {
            const std::string& key = e.first;
            const std::string& val = e.second;
            if (del.find(key) != del.end()) {
                continue;
            }
            envs.append(key, val);
        }
        return envs.release();
    }

    spawn::spawn() {
        fds_[0] = -1;
        fds_[1] = -1;
        fds_[2] = -1;
        int r = posix_spawnattr_init(&spawnattr_);
        (void)r; assert(r == 0);
        r = posix_spawn_file_actions_init(&spawnfile_);
        (void)r; assert(r == 0);
    }

    spawn::~spawn() {
        posix_spawnattr_destroy(&spawnattr_);
        posix_spawn_file_actions_destroy(&spawnfile_);
    }

    void spawn::suspended() {
#if defined(POSIX_SPAWN_START_SUSPENDED)
        // apple extension
        short flags = 0;
        posix_spawnattr_getflags(&spawnattr_, &flags);
        posix_spawnattr_setflags(&spawnattr_, flags | POSIX_SPAWN_START_SUSPENDED);
#endif
    }

    void spawn::detached() {
#if defined(POSIX_SPAWN_SETSID)
        // since glibc 2.26
        short flags = 0;
        posix_spawnattr_getflags(&spawnattr_, &flags);
        posix_spawnattr_setflags(&spawnattr_, flags | POSIX_SPAWN_SETSID);
#endif
    }

    void spawn::redirect(stdio type, file::handle h) { 
        switch (type) {
        case stdio::eInput:
            fds_[0] = h;
            break;
        case stdio::eOutput:
            fds_[1] = h;
            break;
        case stdio::eError:
            fds_[2] = h;
            break;
        default:
            break;
        }
    }

    void spawn::env_set(const std::string& key, const std::string& value) {
        set_env_[key] = value;
    }

    void spawn::env_del(const std::string& key) {
        del_env_.insert(key);
    }

#if defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 101500
#define SUPPORT_CWD 1
#endif
#if defined(__GLIBC__) && __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 29
#define SUPPORT_CWD 1
#endif

    bool support_cwd() {
#if defined(SUPPORT_CWD)
        return true;
#else
        return false;
#endif
    }

    bool spawn::exec(args_t& args, const char* cwd) {
        if (args.size() == 0) {
            return false;
        }
        args.push(nullptr);
        char** arguments = args.data();
#if defined(SUPPORT_CWD)
        if (cwd) {
            posix_spawn_file_actions_addchdir_np(&spawnfile_, cwd);
        }
#endif
        pid_t pid;
        for (int i = 0; i < 3; ++i) {
            if (fds_[i] > 0) {
                if (posix_spawn_file_actions_adddup2(&spawnfile_, fds_[i], i)) {
                    return false;
                }
            }
        }
        auto env = make_env(set_env_, del_env_);
        if (posix_spawnp(&pid, arguments[0], &spawnfile_, &spawnattr_, arguments, env)) {
            return false;
        }
        pid_ = pid;
        for (int i = 0; i < 3; ++i) {
            if (fds_[i] > 0) {
                close(fds_[i]);
            }
        }
        return true;
    }

    process::process(spawn& spawn)
        : pid(spawn.pid_)
    { }

    bool     process::is_running() {
        return (0 == ::waitpid(pid, 0, WNOHANG));
    }

    bool     process::kill(int signum) {
        if (0 == ::kill(pid, signum)) {
            if (signum == 0) {
                return true;
            }
            return wait_with_timeout(pid, &status, 5);
        }
        return false;
    }

    uint32_t process::wait() {
        if (!wait_with_timeout(pid, &status, -1)) {
            return 0;
        }
        int exit_status = WIFEXITED(status)? WEXITSTATUS(status) : 0;
        int term_signal = WIFSIGNALED(status)? WTERMSIG(status) : 0;
        return (term_signal << 8) | exit_status;
    }

    bool process::resume() {
        return kill(SIGCONT);
    }

    uint32_t process::get_id() const {
        return pid;
    }

    uintptr_t process::native_handle() {
        return pid;
    }

    namespace pipe {
        FILE* open_result::open_read() {
            return file::open_read(rd);
        }
        FILE* open_result::open_write() {
            return file::open_write(wr);
        }
        open_result open() {
            int fds[2];
            if (!net::socket::blockpair(fds)) {
                return { file::handle::invalid(), file::handle::invalid() };
            }
            return { file::handle(fds[0]), file::handle(fds[1]) };
        }
        int peek(FILE* f) {
            char tmp[256];
            int rc = recv(file::get_handle(f), tmp, sizeof(tmp), MSG_PEEK | MSG_DONTWAIT);
            if (rc == 0) {
                return -1;
            }
            else if (rc < 0) {
                if (errno == EAGAIN || errno == EINTR) {
                    return 0;
                }
                return -1;
            }
            return rc;
        }
    }
}
