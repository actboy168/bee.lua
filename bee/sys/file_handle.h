#pragma once

#include <bee/nonstd/filesystem.h>
#include <bee/utility/path_view.h>

#include <optional>
#include <string>

namespace bee {
    class file_handle {
    public:
#if defined(_WIN32)
        using value_type = void*;
#else
        using value_type = int;
#endif
        enum class mode {
            read,
            write,
        };

        file_handle() noexcept;
        explicit operator bool() const noexcept;
        bool valid() const noexcept;
        value_type value() const noexcept;
        value_type* operator&() noexcept;
        bool operator==(const file_handle& other) const noexcept;
        bool operator!=(const file_handle& other) const noexcept;
        FILE* to_file(mode mode) const noexcept;
        std::optional<fs::path> path() const;
        void close() noexcept;
        static inline file_handle from_native(value_type v) noexcept {
            file_handle handle;
            handle.h = v;
            return handle;
        }
        static file_handle from_file(FILE* f) noexcept;
        static file_handle dup(FILE* f) noexcept;
        static file_handle lock(path_view filename) noexcept;
        static file_handle open_link(path_view filename) noexcept;

    private:
        value_type h;
    };
}
