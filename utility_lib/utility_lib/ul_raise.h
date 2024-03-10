#pragma once
#include "ul.h"

#define ul_raise(exc_code) RaiseException(exc_code, EXCEPTION_NONCONTINUABLE, 0, NULL)

#define ul_raise_no_mem() ul_raise(STATUS_NO_MEMORY)

#define ul_raise_assert_wc(expr, exc_code) do { if (!(expr)) { ul_raise(exc_code); } } while (false)

#define ul_raise_assert(expr) ul_raise_assert_wc((expr), STATUS_ASSERTION_FAILURE)
#define ul_raise_check_mem_alloc(mem) ul_raise_assert_wc((mem != NULL), STATUS_NO_MEMORY)
