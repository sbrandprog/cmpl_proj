#include "ul_hsb.h"
#include "ul_arr.h"

void ul_hsb_init(ul_hsb_t * hsb) {
	memset(hsb, 0, sizeof(*hsb));

	InitializeCriticalSection(&hsb->lock);
}
void ul_hsb_cleanup(ul_hsb_t * hsb) {
	DeleteCriticalSection(&hsb->lock);

	free(hsb->buf);

	memset(hsb, 0, sizeof(*hsb));
}

size_t ul_hsb_format_nl_va(ul_hsb_t * hsb, const wchar_t * fmt, va_list args) {
	size_t str_size;

	{
		va_list new_args;

		va_copy(new_args, args);

		int res = _vscwprintf(fmt, new_args);

		va_end(new_args);

		ul_assert(res >= 0);

		str_size = (size_t)res;
	}

	size_t str_size_null = str_size + 1;

	if (str_size_null > hsb->cap) {
		ul_arr_grow(&hsb->cap, &hsb->buf, sizeof(*hsb->buf), str_size_null - hsb->cap);
	}

	{
		va_list new_args;

		va_copy(new_args, args);

		int res = vswprintf_s(hsb->buf, hsb->cap, fmt, new_args);

		va_end(new_args);

		ul_assert(res >= 0);

		return (size_t)res;
	}
}
size_t ul_hsb_format_nl(ul_hsb_t * hsb, const wchar_t * fmt, ...) {
	va_list args;

	va_start(args, fmt);

	size_t res = ul_hsb_format_nl_va(hsb, fmt, args);

	va_end(args);

	return res;
}

static ul_hs_t * ul_hsb_formatadd_nl_va(ul_hsb_t * hsb, ul_hst_t * hst, const wchar_t * fmt, va_list args) {
	size_t str_size = ul_hsb_format_nl_va(hsb, fmt, args);

	return ul_hst_hashadd(hst, str_size, hsb->buf);
}
ul_hs_t * ul_hsb_formatadd_va(ul_hsb_t * hsb, ul_hst_t * hst, const wchar_t * fmt, va_list args) {
	EnterCriticalSection(&hsb->lock);

	ul_hs_t * res = ul_hsb_formatadd_nl_va(hsb, hst, fmt, args);
	
	LeaveCriticalSection(&hsb->lock);

	return res;
}
ul_hs_t * ul_hsb_formatadd(ul_hsb_t * hsb, ul_hst_t * hst, const wchar_t * fmt, ...) {
	va_list args;

	va_start(args, fmt);

	ul_hs_t * res = ul_hsb_formatadd_va(hsb, hst, fmt, args);

	va_end(args);

	return res;
}
