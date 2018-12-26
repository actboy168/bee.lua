//
// embed for msvc
// Based on http://wg21.link/p1040r2
//
// Copyright (c) 2018 actboy168
//
#pragma once

#include <bee/nonstd/span.h>

#if defined(_MSC_VER)

namespace nonstd::embed_detail {
    #include <bee/nonstd/embed_detail.h>
}
#define embed(NAME, FILENAME) byte_span(nonstd::embed_detail:: ## NAME)

#else

#include "incbin.h"
#define define_embed(NAME, FILENAME) INCBIN(NAME, FILENAME)
#define embed(NAME, FILENAME) span<const std::byte>((const std::byte*)g##NAME##Data, (size_t)g##NAME##Size)

#endif
