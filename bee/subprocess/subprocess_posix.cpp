#include <bee/subprocess.h>
#include <bee/nonstd/format.h>
#include <bee/net/socket.h>
#include <bee/nonstd/unreachable.h>
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

namespace bee::subprocess {

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


    void envbuilder::set(const std::string& key, const std::string& value) {
        set_env_[key] = value;
    }

    void envbuilder::del(const std::string& key) {
        del_env_.insert(key);
    }

    static void env_append(std::vector<char*>& envs, const std::string& k, const std::string& v) {
        size_t n = k.size() + v.size() + 2;
        char* str = new char[n];
        memcpy(str, k.data(), k.size());
        str[k.size()] = '=';
        memcpy(str+k.size()+1, v.data(), v.size());
        str[n-1] = '\0';
        envs.emplace_back(str);
    }

    static std::unique_ptr<char*[]> env_release(std::vector<char*>& envs) {
        envs.emplace_back(nullptr);
        std::unique_ptr<char*[]> r(new char*[envs.size()]);
        memcpy(r.get(), envs.data(), envs.size() * sizeof(char*));
        return r;
    }

    environment envbuilder::release() {
        char** es = environ;
        if (es == 0) {
            return nullptr;
        }
        std::vector<char*> envs;
        for (; *es; ++es) {
            std::string str = *es;
            std::string::size_type pos = str.find(L'=');
            std::string key = str.substr(0, pos);
            if (del_env_.find(key) != del_env_.end()) {
                continue;
            }
            std::string val = str.substr(pos + 1, str.length());
            auto it = set_env_.find(key);
            if (it != set_env_.end()) {
                val = it->second;
                set_env_.erase(it);
            }
            env_append(envs, key, val);
        }
        for (auto& e : set_env_) {
            const std::string& key = e.first;
            const std::string& val = e.second;
            if (del_env_.find(key) != del_env_.end()) {
                continue;
            }
            env_append(envs, key, val);
        }
        return env_release(envs);
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

    void spawn::redirect(stdio type, file_handle h) { 
        switch (type) {
        case stdio::eInput:
            fds_[0] = h.value();
            break;
        case stdio::eOutput:
            fds_[1] = h.value();
            break;
        case stdio::eError:
            fds_[2] = h.value();
            break;
        default:
            std::unreachable();
        }
    }

    void spawn::env(environment&& env) {
        env_ = std::move(env);
    }

#if defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 101500
// for posix_spawn_file_actions_addchdir_np
#define USE_POSIX_SPAWN 1
#endif
#if defined(__GLIBC__) && __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 29
// for posix_spawn_file_actions_addchdir_np
#define USE_POSIX_SPAWN 1
#endif

    bool spawn::exec(args_t& args, const char* cwd) {
        if (args.size() == 0) {
            return false;
        }
        args.push(nullptr);
#if defined(USE_POSIX_SPAWN)
        char** arguments = args.data();
        if (cwd) {
            posix_spawn_file_actions_addchdir_np(&spawnfile_, cwd);
        }
        pid_t pid;
        for (int i = 0; i < 3; ++i) {
            if (fds_[i] > 0) {
                if (posix_spawn_file_actions_adddup2(&spawnfile_, fds_[i], i)) {
                    return false;
                }
            }
        }
        int ec = posix_spawnp(&pid, arguments[0], &spawnfile_, &spawnattr_, arguments, env_);
        if (ec != 0) {
            errno = ec;
            return false;
        }
        pid_ = pid;
        for (int i = 0; i < 3; ++i) {
            if (fds_[i] > 0) {
                close(fds_[i]);
            }
        }
        return true;
#else
        pid_t pid = fork();
        if (pid == -1) {
            return false;
        }
        if (pid == 0) {
            //if (detached_) {
            //    setsid();
            //}
            for (int i = 0; i < 3; ++i) {
                if (fds_[i] > 0) {
                    if (dup2(fds_[i], i) == -1) {
                        _exit(127);
                    }
                }
            }
            if (env_) {
                environ = env_;
            }
            if (cwd && chdir(cwd)) {
                _exit(127);
            }
            //if (suspended_) {
            //    ::kill(getpid(), SIGSTOP);
            //}
            execvp(args[0], args.data());
            _exit(127);
        }
        pid_ = pid;
        for (int i = 0; i < 3; ++i) {
            if (fds_[i] > 0) {
                close(fds_[i]);
            }
        }
        return true;
#endif
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
        open_result open() {
            int fds[2];
            if (!net::socket::blockpair(fds)) {
                return { {}, {} };
            }
            return { {fds[0]}, {fds[1]} };
        }
        int peek(FILE* f) {
            char tmp[256];
            int rc = recv(file_handle::from_file(f).value(), tmp, sizeof(tmp), MSG_PEEK | MSG_DONTWAIT);
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
