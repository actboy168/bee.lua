#pragma once

#include <string>
#if defined(_WIN32)
#include <Windows.h>
#endif

namespace bee {
    class file_handle {
    public:
#if defined(_WIN32)
        typedef HANDLE value_type;
        typedef std::wstring string_type;
#else
        typedef int value_type;
        typedef std::string string_type;
#endif
        enum class mode {
            read,
            write,
        };

        file_handle();
        file_handle(value_type v);
        explicit operator bool() const;
        value_type value() const;
        value_type* operator&();
        bool operator==(const file_handle& other) const;
        bool operator!=(const file_handle& other) const;
        FILE* to_file(mode mode) const;
        static file_handle from_file(FILE* f);
        static file_handle dup(FILE* f);
        static file_handle lock(const string_type& filename);
    private:
        value_type h;
    };
}
