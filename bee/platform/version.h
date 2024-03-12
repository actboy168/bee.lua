#pragma once

#include <cstdint>

namespace bee {
    struct version {
        uint32_t major;
        uint32_t minor;
        uint32_t revision;
        bool operator<(const version& r) const noexcept {
            const version& l = *this;
            if (l.major < r.major) {
                return true;
            }
            if (l.minor < r.minor) {
                return true;
            }
            if (l.revision < r.revision) {
                return true;
            }
            return false;
        }
    };

    version os_version() noexcept;
}
