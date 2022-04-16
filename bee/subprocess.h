#pragma once

#if defined(_WIN32)
#   include <bee/subprocess/subprocess_win.h>
#else
#   include <bee/subprocess/subprocess_posix.h>
#endif
