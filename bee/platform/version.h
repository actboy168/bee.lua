#pragma once

#include <stdint.h>
#include <bee/nonstd/enum.h>

namespace bee::platform {
    BETTER_ENUM(WinVer, uint8_t,
        PreXP,
        XP,
        Server2003,
        Vista,
        Win7,
        Win8,
        Win8_1,
        Win10,
        Win10Later
    );
	struct version {
		WinVer   ver;
		uint32_t build;
	};
	version get_version();
}