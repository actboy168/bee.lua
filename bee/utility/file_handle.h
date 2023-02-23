#pragma once

#include <string>
#if defined(_WIN32)
#include <Windows.h>
#endif
#include <bee/nonstd/filesystem.h>
#include <optional>

namespace bee {
    class file_handle {
    public:
#if defined(_WIN32)
        using value_type = HANDLE;
#else
        using value_type = int;
#endif
        enum class mode {
            read,
            write,
        };

        file_handle();
        file_handle(value_type v);
        explicit operator bool() const;
        bool valid() const;
        value_type value() const;
        value_type* operator&();
        bool operator==(const file_handle& other) const;
        bool operator!=(const file_handle& other) const;
        FILE* to_file(mode mode) const;
        std::optional<fs::path> path() const;
        void close();
        static file_handle from_file(FILE* f);
        static file_handle dup(FILE* f);
        static file_handle lock(const fs::path& filename);
        static file_handle open_link(const fs::path& filename);
    private:
        value_type h;
    };
}
