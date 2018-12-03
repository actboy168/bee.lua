#pragma once

#include  <bee/config.h>
#include  <bee/utility/dynarray.h>

namespace bee {
#pragma warning(push)
#pragma warning(disable:4251)
#pragma warning(disable:4275)
    class _BEE_API exception : public std::exception {
    public:
        exception();
        exception(const char* fmt, ...);
        exception(const wchar_t* fmt, ...);
        virtual ~exception();
        virtual const char* what() const;
    protected:
        std::dynarray<char> what_;
    };
#pragma warning(pop)
}
