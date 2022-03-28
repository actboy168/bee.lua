#pragma once

#include <bee/config.h>

#if defined(_WIN32)
#   if defined(_MSC_VER)
#   include <bee/subprocess/subprocess_win.h>
#else
#   include <bee/subprocess/subprocess_win.h>
#endif
#else
#include <bee/subprocess/subprocess_posix.h>
#endif
