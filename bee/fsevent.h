#pragma once

#if defined(_WIN32)
#include <bee/fsevent/fsevent_win.h>
namespace bee { namespace fsevent = win::fsevent; }
#elif defined(__APPLE__)
#include <bee/fsevent/fsevent_osx.h>
namespace bee { namespace fsevent = osx::fsevent; }
#else
#include <bee/fsevent/fsevent_linux.h>
namespace bee { namespace fsevent = linux::fsevent; }
#endif
