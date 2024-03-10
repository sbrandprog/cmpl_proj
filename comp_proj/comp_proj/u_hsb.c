#include "pch.h"
#include "u_hsb.h"
#include "u_misc.h"

void u_hsb_cleanup(u_hsb_t * hsb) {
	free(hsb->buf);

	memset(hsb, 0, sizeof(*hsb));
}

static size_t format_vargs(u_hsb_t * hsb, const wchar_t * fmt, va_list vargs) {
	size_t str_size;

	{
		va_list args;

		va_copy(args, vargs);

		int res = _vscwprintf(fmt, args);

		u_assert(res >= 0);

		va_end(args);

		str_size = (size_t)res;
	}

	size_t str_size_null = str_size + 1;

	if (str_size_null > hsb->cap) {
		u_grow_arr(&hsb->cap, &hsb->buf, sizeof(*hsb->buf), str_size_null - hsb->cap);
	}

	{
		va_list args;

		va_copy(args, vargs);

		int res = vswprintf_s(hsb->buf, hsb->cap, fmt, args);

		u_assert(res >= 0 && res < (int)str_size_null);

		va_end(args);

		return (size_t)res;
	}
}

size_t u_hsb_format(u_hsb_t * hsb, const wchar_t * fmt, ...) {
	va_list args;

	va_start(args, fmt);
	
	size_t res = format_vargs(hsb, fmt, args);

	va_end(args);

	return res;
}
u_hs_t * u_hsb_formatadd(u_hsb_t * hsb, u_hst_t * hst, const wchar_t * fmt, ...) {
	va_list args;

	va_start(args, fmt);

	size_t res = format_vargs(hsb, fmt, args);

	va_end(args);

	return u_hst_hashadd(hst, res, hsb->buf);
}
