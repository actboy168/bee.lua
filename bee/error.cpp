#include <bee/error.h>
#include <bee/nonstd/format.h>

#if defined(_WIN32)
#    include <Windows.h>
#    include <bee/platform/win/wtf8.h>
#else
#    include <errno.h>
#endif

namespace bee::error {
#if defined(_WIN32)
    struct errormsg : public std::wstring_view {
        using mybase = std::wstring_view;
        errormsg(wchar_t* str) noexcept
            : mybase(str) {}
        ~errormsg() noexcept {
            ::LocalFree(reinterpret_cast<HLOCAL>(const_cast<wchar_t*>(mybase::data())));
        }
    };

    static std::wstring error_message(int error_code) {
        wchar_t* message           = 0;
        const unsigned long result = ::FormatMessageW(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            error_code,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPWSTR>(&message),
            0,
            NULL
        );

        if ((result == 0) || !message) {
            return std::format(L"Unable to get an error message for error code: {}.", error_code);
        }
        errormsg str(message);
        while (str.size() && ((str.back() == L'\n') || (str.back() == L'\r'))) {
            str.remove_suffix(1);
        }
        return std::wstring(str);
    }

    class winCategory : public std::error_category {
    public:
        const char* name() const noexcept override {
            return "Windows";
        }
        std::string message(int error_code) const override {
            return wtf8::w2u(error_message(error_code));
        }
        std::error_condition default_error_condition(int error_code) const noexcept override {
            const std::error_condition cond = std::system_category().default_error_condition(error_code);
            if (cond.category() == std::generic_category()) {
                return cond;
            }
            return std::error_condition(error_code, *this);
        }
    };

    static winCategory g_windows_category;
#endif

    std::string errmsg(std::string_view msg, std::error_code ec) noexcept {
        return std::format("{}: ({}:{}){}", msg, ec.category().name(), ec.value(), ec.message());
    }

    std::string crt_errmsg(std::string_view msg, std::errc err) noexcept {
        return errmsg(msg, std::make_error_code(err));
    }

    std::string crt_errmsg(std::string_view msg) noexcept {
        return crt_errmsg(msg, (std::errc)errno);
    }

    std::string sys_errmsg(std::string_view msg) noexcept {
#if defined(_WIN32)
        return errmsg(msg, std::error_code(::WSAGetLastError(), g_windows_category));
#else
        return errmsg(msg, std::error_code(errno, std::system_category()));
#endif
    }

    std::string net_errmsg(std::string_view msg, int err) noexcept {
#if defined(_WIN32)
        return errmsg(msg, std::error_code(err, g_windows_category));
#else
        return errmsg(msg, std::error_code(err, std::system_category()));
#endif
    }

    std::string net_errmsg(std::string_view msg) noexcept {
#if defined(_WIN32)
        return net_errmsg(msg, ::GetLastError());
#else
        return net_errmsg(msg, errno);
#endif
    }
}
