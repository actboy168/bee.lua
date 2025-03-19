#pragma once

#include <setjmp.h>

#if defined(_M_IX86)
int __fastcall fast_setjmp(jmp_buf);
_Noreturn void __fastcall fast_longjmp(jmp_buf, int);
#elif defined(_M_X64)
int fast_setjmp(jmp_buf);
_Noreturn void fast_longjmp(jmp_buf, int);
#endif
