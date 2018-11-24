//
// embed for msvc
// Based on http://wg21.link/p1040r2
//
// Copyright (c) 2018 actboy168
//
#pragma once

#include <bee/nonstd/span.h>

namespace nonstd::embed_detail {
	#include <bee/nonstd/embed_detail.h>
}

#define embed(NAME, FILENAME) byte_span(nonstd::embed_detail:: ## NAME)
