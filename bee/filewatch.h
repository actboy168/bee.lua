#pragma once

#if defined(_WIN32)
#include <bee/filewatch/filewatch_win.h>
namespace bee { namespace filewatch = win::filewatch; }
#elif defined(__APPLE__)
#include <bee/filewatch/filewatch_osx.h>
namespace bee { namespace filewatch = osx::filewatch; }
#elif defined(__linux__)
#include <bee/filewatch/filewatch_linux.h>
namespace bee { namespace filewatch = linux::filewatch; }
#endif
