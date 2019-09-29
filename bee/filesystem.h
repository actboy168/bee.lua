#pragma once

#if __has_include(<filesystem>)
#   if defined(__MINGW32__)
#   elif defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)
#       if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 101500
#           define BEE_ENABLE_FILESYSTEM 1
#       endif
#   else
#       define BEE_ENABLE_FILESYSTEM 1
#   endif
#endif

#if defined(BEE_ENABLE_FILESYSTEM)
#undef BEE_ENABLE_FILESYSTEM
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <bee/nonstd/filesystem.h>
namespace fs = ghc::filesystem;
#endif
