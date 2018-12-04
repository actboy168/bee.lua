#pragma once

#include <stdint.h>

namespace bee::platform {
	enum class WinVer {
		PreXP = 0,
		XP,
		Server2003,
		Vista,
		Win7,
		Win8,
		Win8_1,
		Win10,
		Win10Later,
	};
	struct version {
		WinVer   ver;
		uint32_t build;
	};
	version get_version();
}
