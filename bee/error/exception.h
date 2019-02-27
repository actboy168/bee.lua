#pragma once

#include  <bee/config.h>
#include  <bee/nonstd/dynarray.h>

namespace bee {
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4251)
#pragma warning(disable:4275)
#endif
    class _BEE_API exception : public std::exception {
    public:
        exception();
        exception(const char* fmt, ...);
#if defined(_WIN32)
        exception(const wchar_t* fmt, ...);
#endif
        virtual ~exception();
        virtual const char* what() const noexcept;
    protected:
        std::dynarray<char> what_;
    };
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
}
