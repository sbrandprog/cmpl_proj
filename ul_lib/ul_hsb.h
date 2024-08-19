#pragma once
#include "ul_hst.h"

struct ul_hsb {
	CRITICAL_SECTION lock;
	wchar_t * buf;
	size_t cap;
};

UL_API void ul_hsb_init(ul_hsb_t * hsb);
UL_API void ul_hsb_cleanup(ul_hsb_t * hsb);

UL_API size_t ul_hsb_format_nl_va(ul_hsb_t * hsb, const wchar_t * fmt, va_list args);
UL_API size_t ul_hsb_format_nl(ul_hsb_t * hsb, const wchar_t * fmt, ...);

UL_API ul_hs_t * ul_hsb_formatadd_va(ul_hsb_t * hsb, ul_hst_t * hst, const wchar_t * fmt, va_list args);
UL_API ul_hs_t * ul_hsb_formatadd(ul_hsb_t * hsb, ul_hst_t * hst, const wchar_t * fmt, ...);

inline ul_hs_t * ul_hsb_cat_hs_delim(ul_hsb_t * hsb, ul_hst_t * hst, ul_hs_t * first, wchar_t delim, ul_hs_t * second) {
	return ul_hsb_formatadd(hsb, hst, L"%s%c%s", first->str, delim, second->str);
}
