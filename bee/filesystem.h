#pragma once

#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <bee/nonstd/filesystem.h>
namespace fs = ghc::filesystem;
#endif
