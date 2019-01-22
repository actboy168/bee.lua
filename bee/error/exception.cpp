#include <bee/error/exception.h>
#include <stdarg.h>

#if defined(_WIN32)

#include <bee/utility/unicode_win.h>

#define VPRINTF(fmt, va) ::_vscprintf((fmt), (va))
#define VSPRINTF(str, sz, fmt, va) ::_vsnprintf_s((str), (sz) + 1, (sz), (fmt), (va));
#else
#define VPRINTF(fmt, va) ::vprintf((fmt), (va))
#define VSPRINTF(str, sz, fmt, va) ::vsnprintf((str), (sz), (fmt), (va));
#endif

namespace bee {
    exception::exception()
        : what_(0)
    { }

    exception::exception(const char* fmt, ...)
        : exception()
    {
        va_list va;
        va_start(va, fmt);
        size_t sz = VPRINTF(fmt, va);
        std::dynarray<char> str(sz + 1);
        int n = VSPRINTF(str.data(), sz, fmt, va);
        if (n > 0) {
            what_ = std::move(str);
        }
        va_end(va);
    }

#if defined(_WIN32)
    exception::exception(const wchar_t* fmt, ...)
        : exception()
    {
        va_list va;
        va_start(va, fmt);
        size_t sz = ::_vscwprintf(fmt, va);
        std::dynarray<wchar_t> str(sz + 1);
        int n = ::_vsnwprintf_s(str.data(), sz + 1, sz, fmt, va);
        if (n > 0) {
            std::string ustr = w2u(std::wstring_view(str.data(), n));
            what_ = std::dynarray<char>(ustr.size() + 1);
            memcpy(what_.data(), ustr.data(), ustr.size() + 1);
        }
        va_end(va);
    }
#endif

    exception::~exception()
    { }

    const char* exception::what() const noexcept {
        return what_.data() ? what_.data() : "unknown bee::exception";
    }
}
