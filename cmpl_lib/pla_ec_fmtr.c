#include "pla_ec_fmtr.h"

void pla_ec_fmtr_init(pla_ec_fmtr_t * fmtr, pla_ec_t * ec, ul_hst_t * hst) {
	*fmtr = (pla_ec_fmtr_t){ .ec = ec, .hst = hst };

	ul_hsb_init(&fmtr->hsb);
}
void pla_ec_fmtr_cleanup(pla_ec_fmtr_t * fmtr) {
	ul_hsb_cleanup(&fmtr->hsb);

	memset(fmtr, 0, sizeof(*fmtr));
}

void pla_ec_fmtr_formatpost_va(pla_ec_fmtr_t * fmtr, size_t group, ul_hs_t * src_name, pla_ec_pos_t pos_start, pla_ec_pos_t pos_end, const wchar_t * fmt, va_list args) {
	ul_hs_t * msg = ul_hsb_formatadd_va(&fmtr->hsb, fmtr->hst, fmt, args);

	pla_ec_post(fmtr->ec, group, src_name, pos_start, pos_end, msg);
}
void pla_ec_fmtr_formatpost(pla_ec_fmtr_t * fmtr, size_t group, ul_hs_t * src_name, pla_ec_pos_t pos_start, pla_ec_pos_t pos_end, const wchar_t * fmt, ...) {
	va_list args;

	va_start(args, fmt);

	pla_ec_fmtr_formatpost_va(fmtr, group, src_name, pos_start, pos_end, fmt, args);

	va_end(args);
}
