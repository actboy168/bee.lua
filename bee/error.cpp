#include <bee/error.h>
#if defined(_WIN32)
#include <Windows.h>
#include <bee/utility/unicode_win.h>
#include <sstream>
#include <Windows.h>
#else
#include <errno.h>
#endif

namespace bee {
#if defined(_WIN32)
    struct errormsg : public std::wstring_view {
        typedef std::wstring_view mybase;
        errormsg(wchar_t* str) : mybase(str) { }
        ~errormsg() { ::LocalFree(reinterpret_cast<HLOCAL>(const_cast<wchar_t*>(mybase::data()))); }
    };

    static std::wstring error_message(int error_code) {
        wchar_t* message = 0;
        unsigned long result = ::FormatMessageW(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            error_code,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPWSTR>(&message),
            0,
            NULL);

        if ((result == 0) || !message) {
            std::wostringstream os;
            os << L"Unable to get an error message for error code: " << error_code << ".";
            return os.str();
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
            return std::move(w2u(error_message(error_code)));
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

    static const std::error_category& windows_category() noexcept {
        return g_windows_category;
    }
#endif

    int last_crterror() {
        return errno;
    }

    int last_syserror() {
#if defined(_WIN32)
        return ::GetLastError();
#else
        return errno;
#endif
    }

    int last_neterror() {
#if defined(_WIN32)
        return ::WSAGetLastError();
#else
        return errno;
#endif
    }

    static std::error_code make_error_code(int err) {
#if defined(_WIN32)
        return std::error_code(err, windows_category());
#else
        return std::error_code(err, std::generic_category());
#endif
    }

    std::system_error make_error(int err, const char* message) {
        return std::system_error(make_error_code(err), message ? message : "");
    }

    std::system_error make_crterror(const char* message) {
        return std::system_error(
            std::error_code(last_crterror(), std::generic_category()), 
            message ? message : ""
        );
    }

    std::system_error make_syserror(const char* message) {
        return make_error(last_syserror(), message);
    }

    std::system_error make_neterror(const char* message) {
        return make_error(last_neterror(), message);
    }
}
