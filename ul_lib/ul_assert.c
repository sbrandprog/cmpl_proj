#include "ul_assert.h"

void ul_assert_func(const wchar_t * expr_str, const wchar_t * func_name) {
	fwprintf(stderr, L"assertion failure of [%s] in [%s]\n", expr_str, func_name);

	abort();
}
