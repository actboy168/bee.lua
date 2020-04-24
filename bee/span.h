#pragma once

#if __has_include(<span>)
#	include <span>
namespace nonstd {
	using std::span;
}
#else
#	include <bee/nonstd/span.h>
#endif
