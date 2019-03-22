#pragma once

#if defined(_WIN32)

#include <bee/config.h>
#include <stdint.h>

namespace bee::platform {
    enum class WinVer : uint8_t {
        PreXP,
        XP,
        Server2003,
        Vista,
        Win7,
        Win8,
        Win8_1,
        Win10,
        Win10Later
    };
	struct version {
		WinVer   ver;
		uint32_t build;
	};
	_BEE_API version get_version();
}

#endif
