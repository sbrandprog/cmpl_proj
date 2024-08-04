#pragma once
#include "ul.h"

inline _Noreturn void ul_assert_func(const wchar_t * expr_str, const wchar_t * func_name) {
	fwprintf(stderr, L"assertion failure of [%s] in [%s]\n", expr_str, func_name);

	abort();
}

#define ul_assert(expr) do { if (!(expr)) { __debugbreak(); ul_assert_func(L ## #expr, __FUNCTIONW__); } } while(false)
#define ul_assert_unreachable() ul_assert(false && L"unreachable")
