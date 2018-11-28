#pragma once

#if defined(_WIN32)
#include <bee/fsevent/fsevent_win.h>
namespace bee { namespace fsevent = win::fsevent; }
#endif
