#pragma once
#include "pla_ec.h"

struct pla_ec_fmtr {
	pla_ec_t * ec;
	ul_hst_t * hst;

	ul_hsb_t hsb;
};

void pla_ec_fmtr_init(pla_ec_fmtr_t * fmtr, pla_ec_t * ec, ul_hst_t * hst);
void pla_ec_fmtr_cleanup(pla_ec_fmtr_t * fmtr);

void pla_ec_fmtr_formatpost_va(pla_ec_fmtr_t * fmtr, size_t group, ul_hs_t * src_name, pla_ec_pos_t pos_start, pla_ec_pos_t pos_end, const wchar_t * fmt, va_list args);
void pla_ec_fmtr_formatpost(pla_ec_fmtr_t * fmtr, size_t group, ul_hs_t * src_name, pla_ec_pos_t pos_start, pla_ec_pos_t pos_end, const wchar_t * fmt, ...);
