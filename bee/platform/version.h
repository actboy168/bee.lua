#pragma once

#if defined(_WIN32)

#include <bee/config.h>
#include <stdint.h>

namespace bee::platform {
	struct version {
		uint32_t major;
		uint32_t minor;
		uint32_t revision;
		uint32_t build;
        bool operator<(const version& r) const {
            const version& l = *this;
            if (l.major < r.major) { return true; }
            if (l.minor < r.minor) { return true; }
            if (l.revision < r.revision) { return true; }
            if (l.build < r.build) { return true; }
            return false;
        }
	};

    _BEE_API version get_version();
}

#endif
