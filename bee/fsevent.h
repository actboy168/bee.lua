#pragma once

#if defined(_WIN32)
#include <bee/fsevent/fsevent_win.h>
namespace bee { namespace fsevent = win::fsevent; }
#elif defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)
#include <bee/fsevent/fsevent_osx.h>
namespace bee { namespace fsevent = osx::fsevent; }
#endif
