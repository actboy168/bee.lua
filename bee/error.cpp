#include <bee/error.h>
#if defined(_WIN32)
#    include <Windows.h>
#    include <bee/nonstd/format.h>
#    include <bee/platform/win/unicode.h>
#else
#    include <errno.h>
#endif

namespace bee {
#if defined(_WIN32)
    struct errormsg : public std::wstring_view {
        using mybase = std::wstring_view;
        errormsg(wchar_t* str)
            : mybase(str) {}
        ~errormsg() { ::LocalFree(reinterpret_cast<HLOCAL>(const_cast<wchar_t*>(mybase::data()))); }
    };

    static std::wstring error_message(int error_code) {
        wchar_t* message     = 0;
        unsigned long result = ::FormatMessageW(
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
        virtual const char* name() const noexcept {
            return "Windows";
        }
        virtual std::string message(int error_code) const noexcept {
            return win::w2u(error_message(error_code));
        }
        virtual std::error_condition default_error_condition(int error_code) const noexcept {
            const std::error_condition cond = std::system_category().default_error_condition(error_code);
            if (cond.category() == std::generic_category()) {
                return cond;
            }
            return std::error_condition(error_code, *this);
        }
    };

    static winCategory g_windows_category;
#endif

    static int last_crterror() {
        return errno;
    }

    static int last_syserror() {
#if defined(_WIN32)
        return ::GetLastError();
#else
        return errno;
#endif
    }

    static int last_neterror() {
#if defined(_WIN32)
        return ::WSAGetLastError();
#else
        return errno;
#endif
    }

    const std::error_category& get_error_category() {
#if defined(_WIN32)
        return g_windows_category;
#else
        return std::generic_category();
#endif
    }

    std::string make_error(std::error_code errcode, std::string_view errmsg) {
        std::string message(errmsg);
        if (!message.empty()) {
            message.append(": ");
        }
        message.append(errcode.message());
        return message;
    }

    std::string make_crterror(std::string_view errmsg) {
        return make_error(std::error_code(last_crterror(), std::generic_category()), errmsg);
    }

    std::string make_syserror(std::string_view errmsg) {
        return make_error(std::error_code(last_syserror(), get_error_category()), errmsg);
    }

    std::string make_neterror(std::string_view errmsg) {
        return make_error(std::error_code(last_neterror(), get_error_category()), errmsg);
    }
}
