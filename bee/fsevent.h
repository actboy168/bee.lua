#pragma once

#if defined(_WIN32)
#include <bee/fsevent/fsevent_win.h>
namespace bee { namespace fsevent = win::fsevent; }
#elif defined(__MAC_OS_X_VERSION_MIN_REQUIRED)
#include <bee/fsevent/fsevent_osx.h>
namespace bee { namespace fsevent = osx::fsevent; }
#endif
