#pragma once
#include "ul_ec.h"

struct ul_ec_fmtr {
	ul_ec_t * ec;
	ul_ec_rec_t rec;
	size_t strs_pos;
};

UL_API void ul_ec_fmtr_init(ul_ec_fmtr_t * fmtr, ul_ec_t * ec);
UL_API void ul_ec_fmtr_cleanup(ul_ec_fmtr_t * fmtr);

UL_API void ul_ec_fmtr_format_va(ul_ec_fmtr_t * fmtr, const wchar_t * fmt, va_list args);
inline void ul_ec_fmtr_format(ul_ec_fmtr_t * fmtr, const wchar_t * fmt, ...) {
	va_list args;

	va_start(args, fmt);

	ul_ec_fmtr_format_va(fmtr, fmt, args);

	va_end(args);
}

UL_API void ul_ec_fmtr_post(ul_ec_fmtr_t * fmtr, const wchar_t * type, const wchar_t * mod_name);
