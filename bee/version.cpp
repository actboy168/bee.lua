#include <bee/version.h>

#if defined(__APPLE__)
#    include <objc/message.h>
#elif defined(_WIN32)
#    include <Windows.h>
extern "C" {
void WINAPI RtlGetNtVersionNumbers(LPDWORD, LPDWORD, LPDWORD);
}
#else
#    include <bee/nonstd/charconv.h>
#    include <sys/utsname.h>

#    include <cstdio>
#    include <string_view>
#    include <tuple>
#endif

namespace bee {

#if defined(__APPLE__)
    version os_version() noexcept {
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
        // NSOperatingSystemVersion version = [processInfo operatingSystemVersion]
#    if defined(_M_ARM64) || defined(__aarch64__)
        OSVersion version = reinterpret_cast<OSVersion (*)(id, SEL)>(objc_msgSend)(processInfo, sel_getUid("operatingSystemVersion"));
#    else
        OSVersion version = reinterpret_cast<OSVersion (*)(id, SEL)>(objc_msgSend_stret)(processInfo, sel_getUid("operatingSystemVersion"));
#    endif
        return {
            static_cast<uint32_t>(version.major_version),
            static_cast<uint32_t>(version.minor_version),
            static_cast<uint32_t>(version.patch_version)
        };
    }
#elif defined(_WIN32)
    version os_version() noexcept {
        DWORD major, minor, build;
        RtlGetNtVersionNumbers(&major, &minor, &build);
        return {
            major,
            minor,
            build & (~0xF0000000),
        };
    }
#else
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
    version os_version() noexcept {
        struct utsname info;
        if (uname(&info) < 0) {
            return { 0, 0, 0 };
        }
        return to_version(std::string_view { info.release });
    }
#endif
}
