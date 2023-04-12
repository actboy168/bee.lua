#pragma once

#if defined(_WIN32)
#    include <cstdint>
#endif
#include <bee/nonstd/filesystem.h>

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
        file_handle(value_type v) noexcept;
        explicit operator bool() const noexcept;
        bool valid() const noexcept;
        value_type value() const noexcept;
        value_type* operator&() noexcept;
        bool operator==(const file_handle& other) const noexcept;
        bool operator!=(const file_handle& other) const noexcept;
        FILE* to_file(mode mode) const noexcept;
        std::optional<fs::path> path() const;
        void close() noexcept;
        static file_handle from_file(FILE* f) noexcept;
        static file_handle dup(FILE* f) noexcept;
        static file_handle lock(const fs::path& filename) noexcept;
        static file_handle open_link(const fs::path& filename) noexcept;

    private:
        value_type h;
    };
}
