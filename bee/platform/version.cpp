#include <bee/platform/version.h>

#if defined(__APPLE__)
#   include <objc/message.h>
#else
#   if defined(_WIN32)
#       include <windows.h>
#       include <bee/platform/win/module_version.h>
#   else
#       include <sys/utsname.h>
#       include <stdio.h>
#   endif
#   include <vector>
#   include <string_view>
#   include <bee/nonstd/charconv.h>
#endif

namespace bee {
    
#if !defined(__APPLE__)
    static uint32_t toint(std::string_view const& str, uint32_t def = 0) {
        uint32_t res;
        auto first = str.data();
        auto last = str.data() + str.size();
        if (auto[p, ec] = std::from_chars(first, last, res); ec != std::errc()) {
            (void)p;
            return def;
        }
        return res;
    }

#if defined(_WIN32)
    static uint32_t toint(std::wstring_view const& wstr, uint32_t def = 0) {
        std::string str;
        str.resize(wstr.size());
        for (size_t i = 0; i < wstr.size(); ++i) {
            if (wstr[i] > 127) {
                return def;
            }
            str[i] = (char)wstr[i];
        }
        return toint(str, def);
    }
#endif

    template <typename CharT>
    static version to_version(const std::basic_string_view<CharT>& verstr) {
        std::basic_string_view<CharT> vers[3];
        size_t n = 0;
        for (size_t pos = 0;;) {
            if (n == sizeof(vers)/sizeof(vers[0])) {
                break;
            }
            size_t next = verstr.find(CharT('.'), pos);
            if (next == std::basic_string_view<CharT>::npos) {
                vers[n++] = verstr.substr(pos);
                break;
            }
            vers[n++] = verstr.substr(pos, next - pos);
            pos = next + 1;
        }
        return {
            (n > 0) ? toint(vers[0]) : 0,
            (n > 1) ? toint(vers[1]) : 0,
            (n > 2) ? toint(vers[2]) : 0,
        };
    }
#endif

    version os_version() {
#if defined(__APPLE__)
        // id processInfo = [NSProcessInfo processInfo]
        id processInfo = reinterpret_cast<id (*)(Class, SEL)>(objc_msgSend)(objc_getClass("NSProcessInfo"), sel_getUid("processInfo"));
        if (!processInfo) {
            return {0,0,0};
        }
        struct OSVersion {
            int64_t major_version;
            int64_t minor_version;
            int64_t patch_version;
        };
#if defined(_M_ARM64) || defined(__aarch64__)
#   define msgSend objc_msgSend
#else
#   define msgSend objc_msgSend_stret
#endif
        // NSOperatingSystemVersion version = [processInfo operatingSystemVersion]
        OSVersion version = reinterpret_cast<OSVersion (*)(id, SEL)>(msgSend)(processInfo, sel_getUid("operatingSystemVersion"));
        return {
            static_cast<uint32_t>(version.major_version),
            static_cast<uint32_t>(version.minor_version),
            static_cast<uint32_t>(version.patch_version)
        };
#elif defined(_WIN32)
        OSVERSIONINFOW osvi = {  };
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOW);
#if defined(_MSC_VER)
#pragma warning(suppress:4996; suppress:28159)
        BOOL ok = ::GetVersionExW(&osvi);
#else
        BOOL ok = ::GetVersionExW(&osvi);
#endif
        if (!ok) {
            return { 0, 0, 0 };
        }

        version v {
            osvi.dwMajorVersion,
            osvi.dwMinorVersion,
            osvi.dwBuildNumber,
        };
        if ((v.major > 6) || (v.major == 6 && v.minor >= 2)) {
            // see
            //   http://msdn.microsoft.com/en-us/library/windows/desktop/ms724451(v=vs.85).aspx
            //   http://msdn.microsoft.com/en-us/library/windows/desktop/ms724429(v=vs.85).aspx
            win::module_version mv(L"kernel32.dll");
            auto wstr = mv.get_value(L"ProductVersion");
            return to_version(wstr);
        }
        return v;
#else
        struct utsname info;
        if (uname(&info) < 0) {
            return { 0, 0, 0 };
        }
        return to_version(std::string_view {info.release});
#endif
    }
}
