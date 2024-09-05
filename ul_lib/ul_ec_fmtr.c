#include "ul_assert.h"
#include "ul_ec_fmtr.h"

void ul_ec_fmtr_init(ul_ec_fmtr_t * fmtr, ul_ec_t * ec) {
	*fmtr = (ul_ec_fmtr_t){ .ec = ec };
}
void ul_ec_fmtr_cleanup(ul_ec_fmtr_t * fmtr) {
	memset(fmtr, 0, sizeof(*fmtr));
}

void ul_ec_fmtr_format_va(ul_ec_fmtr_t * fmtr, const wchar_t * fmt, va_list args) {
	if (fmtr->strs_pos < _countof(fmtr->rec.strs) - 1) {
		errno = 0;

		int res = _vsnwprintf_s(fmtr->rec.strs + fmtr->strs_pos, _countof(fmtr->rec.strs) - fmtr->strs_pos, _TRUNCATE, fmt, args);

		ul_assert(errno == 0);

		fmtr->strs_pos = res == -1 ? _countof(fmtr->rec.strs) : (fmtr->strs_pos + (size_t)res + 1);

		++fmtr->rec.strs_size;
	}
	else {
		fmtr->strs_pos = _countof(fmtr->rec.strs);
	}
}
void ul_ec_fmtr_format(ul_ec_fmtr_t * fmtr, const wchar_t * fmt, ...) {
	va_list args;

	va_start(args, fmt);

	ul_ec_fmtr_format_va(fmtr, fmt, args);

	va_end(args);
}

void ul_ec_fmtr_post(ul_ec_fmtr_t * fmtr, const wchar_t * type, const wchar_t * mod_name) {
	fmtr->rec.type = type;
	fmtr->rec.mod_name = mod_name;

	ul_ec_post(fmtr->ec, &fmtr->rec);

	fmtr->rec.strs_size = 0;
	fmtr->strs_pos = 0;
}
