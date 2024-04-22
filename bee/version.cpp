#include <bee/version.h>

#if defined(__APPLE__)
#    include <objc/message.h>
#else
#    if defined(_WIN32)
#        include <Windows.h>
#        include <bee/win/module_version.h>
#    else
#        include <sys/utsname.h>

#        include <cstdio>
#    endif
#    include <bee/nonstd/charconv.h>

#    include <string_view>
#    include <tuple>
#endif

namespace bee {

#if !defined(__APPLE__)
    static uint32_t toint(std::string_view str, uint32_t def = 0) noexcept {
        uint32_t res = def;
        auto first   = str.data();
        auto last    = str.data() + str.size();
        if (auto [p, ec] = std::from_chars(first, last, res); ec != std::errc()) {
            std::ignore = p;
            return def;
        }
        return res;
    }

#    if defined(_WIN32)
    static uint32_t toint(std::wstring_view wstr, uint32_t def = 0) noexcept {
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
#    endif

    template <typename CharT>
    static version to_version(std::basic_string_view<CharT> verstr) noexcept {
        constexpr auto npos = std::basic_string_view<CharT>::npos;
        version v { 0, 0, 0 };
        size_t pos  = 0;
        size_t next = verstr.find(CharT { '.' }, pos);
        v.major     = toint(verstr.substr(pos, (next == npos) ? npos : (next - pos)));
        if (next == npos) {
            return v;
        }
        pos     = next + 1;
        next    = verstr.find(CharT { '.' }, pos);
        v.minor = toint(verstr.substr(pos, (next == npos) ? npos : (next - pos)));
        if (next == npos) {
            return v;
        }
        pos        = next + 1;
        next       = verstr.find(CharT { '.' }, pos);
        v.revision = toint(verstr.substr(pos, (next == npos) ? npos : (next - pos)));
        return v;
    }
#endif

    version os_version() noexcept {
#if defined(__APPLE__)
        // id processInfo = [NSProcessInfo processInfo]
        id processInfo = reinterpret_cast<id (*)(Class, SEL)>(objc_msgSend)(objc_getClass("NSProcessInfo"), sel_getUid("processInfo"));
        if (!processInfo) {
            return { 0, 0, 0 };
        }
        struct OSVersion {
            int64_t major_version;
            int64_t minor_version;
            int64_t patch_version;
        };
#    if defined(_M_ARM64) || defined(__aarch64__)
#        define msgSend objc_msgSend
#    else
#        define msgSend objc_msgSend_stret
#    endif
        // NSOperatingSystemVersion version = [processInfo operatingSystemVersion]
        OSVersion version = reinterpret_cast<OSVersion (*)(id, SEL)>(msgSend)(processInfo, sel_getUid("operatingSystemVersion"));
        return {
            static_cast<uint32_t>(version.major_version),
            static_cast<uint32_t>(version.minor_version),
            static_cast<uint32_t>(version.patch_version)
        };
#elif defined(_WIN32)
        OSVERSIONINFOW osvi      = {};
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOW);
#    if defined(_MSC_VER)
#        pragma warning(push)
#        pragma warning(disable : 4996; disable : 28159)
#    endif
        const BOOL ok = ::GetVersionExW(&osvi);
#    if defined(_MSC_VER)
#        pragma warning(pop)
#    endif
        if (!ok) {
            return { 0, 0, 0 };
        }

        if (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 2 && osvi.dwBuildNumber == 9200) {
            // see
            //   http://msdn.microsoft.com/en-us/library/windows/desktop/ms724451(v=vs.85).aspx
            //   http://msdn.microsoft.com/en-us/library/windows/desktop/ms724429(v=vs.85).aspx
            win::module_version mv(L"kernel32.dll");
            const auto wstr = mv.get_value(L"ProductVersion");
            return to_version(wstr);
        }
        return {
            osvi.dwMajorVersion,
            osvi.dwMinorVersion,
            osvi.dwBuildNumber,
        };
#else
        struct utsname info;
        if (uname(&info) < 0) {
            return { 0, 0, 0 };
        }
        return to_version(std::string_view { info.release });
#endif
    }
}
