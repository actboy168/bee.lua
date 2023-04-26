#include <bee/error.h>
#include <bee/utility/dynarray.h>
#include <bee/utility/path_helper.h>

#if defined(_WIN32)

#    include <Windows.h>
#    include <shlobj.h>

// http://blogs.msdn.com/oldnewthing/archive/2004/10/25/247180.aspx
extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace bee::path_helper {
    static path_expected dll_path(HMODULE module_handle) {
        wchar_t buffer[MAX_PATH];
        DWORD path_len = ::GetModuleFileNameW(module_handle, buffer, _countof(buffer));
        if (path_len == 0) {
            return unexpected<std::string>(make_syserror("GetModuleFileNameW"));
        }
        if (path_len < _countof(buffer)) {
            return fs::path(buffer, buffer + path_len);
        }
        for (DWORD buf_len = 0x200; buf_len <= 0x10000; buf_len <<= 1) {
            dynarray<wchar_t> buf(buf_len);
            path_len = ::GetModuleFileNameW(module_handle, buf.data(), buf_len);
            if (path_len == 0) {
                return unexpected<std::string>(make_syserror("GetModuleFileNameW"));
            }
            if (path_len < _countof(buffer)) {
                return fs::path(buf.data(), buf.data() + path_len);
            }
        }
        return unexpected<std::string>("::GetModuleFileNameW return too long.");
    }

    path_expected exe_path() {
        return dll_path(NULL);
    }

    path_expected dll_path() {
        return dll_path(reinterpret_cast<HMODULE>(&__ImageBase));
    }
}

#else

#    if defined(__APPLE__)

#        include <mach-o/dyld.h>

namespace bee::path_helper {
    path_expected exe_path() {
        uint32_t path_len = 0;
        _NSGetExecutablePath(0, &path_len);
        if (path_len <= 1) {
            return unexpected<std::string>("_NSGetExecutablePath failed.");
        }
        dynarray<char> buf(path_len);
        int rv = _NSGetExecutablePath(buf.data(), &path_len);
        if (rv != 0) {
            return unexpected<std::string>("_NSGetExecutablePath failed.");
        }
        return fs::path(buf.data(), buf.data() + path_len - 1);
    }
}

#    else

#        include <unistd.h>

#        if defined(__FreeBSD__)
#            include <sys/param.h>
#            include <sys/sysctl.h>
#        elif defined(__OpenBSD__)
#            include <sys/sysctl.h>
#            include <unistd.h>
#        endif

namespace bee::path_helper {
    path_expected exe_path() {
#        if defined(__FreeBSD__)
        int name[] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
        char exe[1024];
        size_t length = 1024;
        int error     = sysctl(name, 4, exe, &length, NULL, 0);
        if (error < 0 || length <= 1) {
            return unexpected<std::string>(make_syserror("exe_path"));
        }
        return fs::path(exe, exe + length - 1);
#        elif defined(__OpenBSD__)
        int name[] = { CTL_KERN, KERN_PROC_ARGS, getpid(), KERN_PROC_ARGV };
        size_t argc;
        if (sysctl(name, 4, NULL, &argc, NULL, 0) < 0) {
            return unexpected<std::string>(make_syserror("exe_path"));
        }
        const char** argv = (const char**)malloc(argc);
        if (!argv) {
            return unexpected<std::string>(make_syserror("exe_path"));
        }
        if (sysctl(name, 4, argv, &argc, NULL, 0) < 0) {
            return unexpected<std::string>(make_syserror("exe_path"));
        }
        return fs::path(argv[0]);
#        else
        std::error_code ec;
        auto res = fs::read_symlink("/proc/self/exe", ec);
        if (ec) {
            return unexpected<std::string>(make_syserror("exe_path"));
        }
        return res;
#        endif
    }
}

#    endif

#    if defined(BEE_DISABLE_DLOPEN)

namespace bee::path_helper {
    static path_expected dll_path(void* module_handle) {
        return unexpected<std::string>("disable dl.");
    }
    path_expected dll_path() {
        return dll_path(nullptr);
    }
}

#    else

#        include <dlfcn.h>

namespace bee::path_helper {
    static path_expected dll_path(void* module_handle) {
        ::Dl_info dl_info;
        dl_info.dli_fname = 0;
        const int ret     = ::dladdr(module_handle, &dl_info);
        if (0 != ret && dl_info.dli_fname != NULL) {
            return fs::absolute(dl_info.dli_fname).lexically_normal();
        }
        return unexpected<std::string>("::dladdr failed.");
    }

    path_expected dll_path() {
        return dll_path((void*)&exe_path);
    }
}

#    endif

#endif

namespace bee::path_helper {
#if defined(_WIN32)
    bool equal(const fs::path& lhs, const fs::path& rhs) {
        fs::path lpath = lhs.lexically_normal();
        fs::path rpath = rhs.lexically_normal();
        const fs::path::value_type* l(lpath.c_str());
        const fs::path::value_type* r(rpath.c_str());
        while ((towlower(*l) == towlower(*r) || (*l == L'\\' && *r == L'/') || (*l == L'/' && *r == L'\\')) && *l) {
            ++l;
            ++r;
        }
        return *l == *r;
    }
#elif defined(__APPLE__)
    bool equal(const fs::path& lhs, const fs::path& rhs) {
        fs::path lpath = lhs.lexically_normal();
        fs::path rpath = rhs.lexically_normal();
        const fs::path::value_type* l(lpath.c_str());
        const fs::path::value_type* r(rpath.c_str());
        while (towlower(*l) == towlower(*r) && *l) {
            ++l;
            ++r;
        }
        return *l == *r;
    }
#else
    bool equal(const fs::path& lhs, const fs::path& rhs) {
        return lhs.lexically_normal() == rhs.lexically_normal();
    }
#endif
}
