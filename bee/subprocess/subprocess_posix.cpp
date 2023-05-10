#include <bee/net/socket.h>
#include <bee/nonstd/format.h>
#include <bee/nonstd/unreachable.h>
#include <bee/subprocess.h>
#include <bee/utility/dynarray.h>
#include <errno.h>
#include <memory.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cassert>

#if defined(__APPLE__)
#    include <crt_externs.h>
#    define environ (*_NSGetEnviron())
#else
extern char** environ;
#endif

namespace bee::subprocess {

    args_t::~args_t() {
        for (size_t i = 0; i < size(); ++i) {
            delete[] (data_[i]);
        }
    }

    void args_t::push(char* str) {
        data_.emplace_back(str);
    }

    void args_t::push(zstring_view str) {
        dynarray<char> tmp(str.data(), str.size() + 1);
        data_.emplace_back(tmp.release());
    }

    void envbuilder::set(const std::string& key, const std::string& value) {
        set_env_[key] = value;
    }

    void envbuilder::del(const std::string& key) {
        set_env_[key] = std::nullopt;
    }

    static void env_append(std::vector<char*>& envs, const std::string& k, const std::string& v) {
        size_t n = k.size() + v.size() + 2;
        dynarray<char> tmp(n);
        memcpy(tmp.data(), k.data(), k.size());
        tmp[k.size()] = '=';
        memcpy(tmp.data() + k.size() + 1, v.data(), v.size());
        tmp[n - 1] = '\0';
        envs.emplace_back(tmp.release());
    }

    static dynarray<char*> env_release(std::vector<char*>& envs) {
        envs.emplace_back(nullptr);
        return { envs };
    }

    environment envbuilder::release() {
        char** es = environ;
        if (es == 0) {
            return nullptr;
        }
        std::vector<char*> envs;
        for (; *es; ++es) {
            std::string str = *es;
            auto pos        = str.find(L'=');
            std::string key = str.substr(0, pos);
            std::string val = str.substr(pos + 1, str.length());
            auto it         = set_env_.find(key);
            if (it == set_env_.end()) {
                env_append(envs, key, val);
            }
            else {
                if (it->second.has_value()) {
                    env_append(envs, key, *it->second);
                }
                set_env_.erase(it);
            }
        }
        for (auto& e : set_env_) {
            const std::string& key = e.first;
            if (e.second.has_value()) {
                env_append(envs, key, *e.second);
            }
        }
        return env_release(envs);
    }

    spawn::spawn() {
        fds_[0] = -1;
        fds_[1] = -1;
        fds_[2] = -1;
    }

    void spawn::suspended() {
#if defined(POSIX_SPAWN_START_SUSPENDED)
        // apple extension
        spawnattr_ |= POSIX_SPAWN_START_SUSPENDED;
#endif
    }

    void spawn::detached() {
#if defined(POSIX_SPAWN_SETSID)
        // since glibc 2.26
        spawnattr_ |= POSIX_SPAWN_SETSID;
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
#    define USE_POSIX_SPAWN 1
#endif
#if defined(__GLIBC__) && __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 29
// for posix_spawn_file_actions_addchdir_np
#    define USE_POSIX_SPAWN 1
#endif

    bool spawn::exec(args_t& args, const char* cwd) {
        if (args.size() == 0) {
            return false;
        }
        args.push(nullptr);
#if defined(USE_POSIX_SPAWN)
        posix_spawn_file_actions_t actions;
        if (int err = posix_spawn_file_actions_init(&actions)) {
            errno = err;
            return false;
        }
        char** arguments = args.data();
        if (cwd) {
            if (int err = posix_spawn_file_actions_addchdir_np(&actions, cwd)) {
                posix_spawn_file_actions_destroy(&actions);
                errno = err;
                return false;
            }
        }
        pid_t pid;
        for (int i = 0; i < 3; ++i) {
            if (fds_[i] > 0) {
                if (int err = posix_spawn_file_actions_adddup2(&actions, fds_[i], i)) {
                    posix_spawn_file_actions_destroy(&actions);
                    errno = err;
                    return false;
                }
            }
        }
        posix_spawnattr_t attr;
        if (int err = posix_spawnattr_init(&attr)) {
            posix_spawn_file_actions_destroy(&actions);
            errno = err;
            return false;
        }
        if (int err = posix_spawnattr_setflags(&attr, spawnattr_)) {
            posix_spawn_file_actions_destroy(&actions);
            posix_spawnattr_destroy(&attr);
            errno = err;
            return false;
        }
        if (int err = posix_spawnp(&pid, arguments[0], &actions, &attr, arguments, env_)) {
            posix_spawn_file_actions_destroy(&actions);
            posix_spawnattr_destroy(&attr);
            errno = err;
            return false;
        }
        posix_spawn_file_actions_destroy(&actions);
        posix_spawnattr_destroy(&attr);
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
            // if (detached_) {
            //     setsid();
            // }
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
            // if (suspended_) {
            //     ::kill(getpid(), SIGSTOP);
            // }
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

    process::process(spawn& spawn) noexcept
        : pid(spawn.pid_) {}

    process::~process() noexcept {
        detach();
    }

    bool process::detach() noexcept {
        return !is_running();
    }

    process_id process::get_id() const noexcept {
        return pid;
    }

    process_handle process::native_handle() const noexcept {
        return pid;
    }

    bool process::kill(int signum) noexcept {
        return 0 == ::kill(pid, signum);
    }

    static uint32_t make_status(int status) {
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }
        if (WIFSIGNALED(status)) {
            return WTERMSIG(status) << 8;
        }
        return 0;
    }

    bool process::is_running() noexcept {
        if (status) {
            return false;
        }
        int stat;
        int r = ::waitpid(pid, &stat, WNOHANG);
        if (r == 0) {
            return true;
        }
        if (r == -1) {
            return false;
        }
        status = make_status(stat);
        return false;
    }

    std::optional<uint32_t> process::wait() noexcept {
        if (status) {
            return status;
        }
        int r, stat;
        do
            r = ::waitpid(pid, &stat, 0);
        while (r == -1 && errno == EINTR);
        if (r == -1) {
            return std::nullopt;
        }
        status = make_status(stat);
        return status;
    }

    bool process::resume() noexcept {
        return kill(SIGCONT);
    }

    namespace pipe {
        open_result open() noexcept {
            net::fd_t fds[2];
            if (!net::socket::pair(fds, net::socket::fd_flags::none)) {
                return { {}, {} };
            }
            return { { (bee::file_handle::value_type)fds[0] }, { (bee::file_handle::value_type)fds[1] } };
        }
        int peek(FILE* f) noexcept {
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
