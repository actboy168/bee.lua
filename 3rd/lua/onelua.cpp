extern "C" {
    #include "lprefix.h"
}
#include "lua.hpp"
#define l_likely(x) luai_likely(x)
#define l_unlikely(x) luai_unlikely(x)
#include "onelua.c"
