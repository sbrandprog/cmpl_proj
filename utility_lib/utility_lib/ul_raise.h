#pragma once
#include "ul.h"

inline _Noreturn void ul_raise_func(DWORD exc_code) {
	RaiseException(exc_code, EXCEPTION_NONCONTINUABLE, 0, NULL);
	
	__fastfail(-1);
}

#define ul_raise(exc_code) ul_raise_func(exc_code)

#define ul_raise_no_mem() ul_raise(STATUS_NO_MEMORY)
#define ul_raise_unreachable() ul_raise(STATUS_ASSERTION_FAILURE)

#define ul_raise_assert_wc(expr, exc_code) do { if (!(expr)) { ul_raise(exc_code); } } while (false)

#define ul_raise_assert(expr) ul_raise_assert_wc((expr), STATUS_ASSERTION_FAILURE)
#define ul_raise_check_mem_alloc(mem) ul_raise_assert_wc((mem != NULL), STATUS_NO_MEMORY)
