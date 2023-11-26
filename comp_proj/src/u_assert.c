#include "pch.h"
#include "u_assert.h"

_Noreturn void u_assert_func(const wchar_t * func_name, const wchar_t * expr_str) {
	fwprintf(stderr, L"execution failed an [%s] assertion in [%s] function.", expr_str, func_name);

#ifdef _DEBUG
	__debugbreak();
#endif

	abort();
}