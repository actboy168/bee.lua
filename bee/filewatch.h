#pragma once

#if defined(_WIN32)
#include <bee/filewatch/filewatch_win.h>
namespace bee { namespace fsevent = win::fsevent; }
#endif
