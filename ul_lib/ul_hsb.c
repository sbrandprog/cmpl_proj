#include "ul_hsb.h"
#include "ul_arr.h"

void ul_hsb_init(ul_hsb_t * hsb) {
	*hsb = (ul_hsb_t){ 0 };
}
void ul_hsb_cleanup(ul_hsb_t * hsb) {
	free(hsb->buf);

	memset(hsb, 0, sizeof(*hsb));
}

size_t ul_hsb_format_va(ul_hsb_t * hsb, const wchar_t * fmt, va_list args) {
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

	ul_arr_grow(str_size_null, &hsb->cap, &hsb->buf, sizeof(*hsb->buf));

	{
		va_list new_args;

		va_copy(new_args, args);

		int res = vswprintf_s(hsb->buf, hsb->cap, fmt, new_args);

		va_end(new_args);

		ul_assert(res >= 0);

		return (size_t)res;
	}
}
size_t ul_hsb_format(ul_hsb_t * hsb, const wchar_t * fmt, ...) {
	va_list args;

	va_start(args, fmt);

	size_t res = ul_hsb_format_va(hsb, fmt, args);

	va_end(args);

	return res;
}

ul_hs_t * ul_hsb_formatadd_va(ul_hsb_t * hsb, ul_hst_t * hst, const wchar_t * fmt, va_list args) {
	size_t str_size = ul_hsb_format_va(hsb, fmt, args);

	return ul_hst_hashadd(hst, str_size, hsb->buf);
}
