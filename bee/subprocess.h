#pragma once

#include <bee/config.h>

#if defined(_WIN32)
#   if defined(_MSC_VER)
#   pragma warning(push)
#   pragma warning(disable:4251)
#   include <bee/subprocess/subprocess_win.h>
#   pragma warning(pop)
#else
#   include <bee/subprocess/subprocess_win.h>
#endif
namespace bee { namespace subprocess = win::subprocess; }
#else
#include <bee/subprocess/subprocess_posix.h>
namespace bee { namespace subprocess = posix::subprocess; }
#endif
