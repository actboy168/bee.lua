#pragma once

#include <stdint.h>

namespace bee::win {
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

	version get_version();
}
