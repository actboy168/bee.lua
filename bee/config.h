#pragma once

#if defined(BEE_INLINE)
#    define _BEE_API
#else
#    if defined(BEE_EXPORTS)
#        define _BEE_API __declspec(dllexport)
#    else
#        define _BEE_API __declspec(dllimport)
#    endif
#endif
