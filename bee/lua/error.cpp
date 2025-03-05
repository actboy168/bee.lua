#include <bee/lua/error.h>
#include <bee/nonstd/format.h>

#if defined(_WIN32)
#    include <Windows.h>
#    include <bee/win/cwtf8.h>
#else
#    include <errno.h>
#endif

namespace bee::lua {
#if defined(_WIN32)
    class sys_category : public std::error_category {
    public:
        struct errormsg : public std::wstring_view {
            using mybase = std::wstring_view;
            errormsg(wchar_t* str) noexcept
                : mybase(str) {}
            ~errormsg() noexcept {
                ::LocalFree(reinterpret_cast<HLOCAL>(const_cast<wchar_t*>(mybase::data())));
            }
        };
        const char* name() const noexcept override {
            return "Windows";
        }
        std::string message(int error_code) const override {
            wchar_t* message           = nullptr;
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
                return std::format("Unable to get an error message for error code: {}.", error_code);
            }
            errormsg wstr(message);
            while (wstr.size() && ((wstr.back() == L'\n') || (wstr.back() == L'\r'))) {
                wstr.remove_suffix(1);
            }
            size_t len = wtf8_from_utf16_length(wstr.data(), wstr.size());
            std::string ret(len, '\0');
            wtf8_from_utf16(wstr.data(), wstr.size(), ret.data(), len);
            return ret;
        }
        std::error_condition default_error_condition(int error_code) const noexcept override {
            const std::error_condition cond = std::system_category().default_error_condition(error_code);
            if (cond.category() == std::generic_category()) {
                return cond;
            }
            return std::error_condition(error_code, *this);
        }
    };
    static int last_sys_error() {
        return ::GetLastError();
    }
    static int last_net_error() {
        return ::WSAGetLastError();
    }
#else
    static const std::error_category& sys_category() {
        return std::generic_category();
    }
    static int last_sys_error() {
        return errno;
    }
    static int last_net_error() {
        return errno;
    }
#endif
    static void push_error(lua_State* L, std::string_view msg, std::string_view name, const std::error_category& cat, int val) {
        lua_pushfstring(L, "%s: (%s:%d)%s", msg.data(), name.data(), val, cat.message(val).c_str());
    }
    void push_sys_error(lua_State* L, std::string_view msg, int err) {
        push_error(L, msg, "sys", sys_category(), err);
    }
    void push_sys_error(lua_State* L, std::string_view msg) {
        push_error(L, msg, "sys", sys_category(), last_sys_error());
    }
    void push_net_error(lua_State* L, std::string_view msg) {
        push_error(L, msg, "net", sys_category(), last_net_error());
    }
    int return_error(lua_State* L, std::string_view msg) {
        lua_pushnil(L);
        lua_pushlstring(L, msg.data(), msg.size());
        return 2;
    }
    int return_crt_error(lua_State* L, std::string_view msg) {
        lua_pushnil(L);
        push_error(L, msg, "crt", std::generic_category(), errno);
        return 2;
    }
    int return_sys_error(lua_State* L, std::string_view msg) {
        lua_pushnil(L);
        push_sys_error(L, msg);
        return 2;
    }
    int return_net_error(lua_State* L, std::string_view msg, int err) {
        lua_pushnil(L);
        push_error(L, msg, "net", sys_category(), err);
        return 2;
    }
    int return_net_error(lua_State* L, std::string_view msg) {
        lua_pushnil(L);
        push_net_error(L, msg);
        return 2;
    }
}
