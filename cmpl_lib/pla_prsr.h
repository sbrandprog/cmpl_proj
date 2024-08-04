#pragma once
#include "pla_tok.h"

#define PLA_PRSR_EC_GROUP 2

struct pla_prsr_src {
	ul_hs_t * name;
	void * data;
	pla_prsr_src_get_tok_proc_t * get_tok_proc;
};
struct pla_prsr_rse {
	pla_prsr_rse_t * prev;
	bool is_rptd;
	bool set_next;
	bool get_next;
};
struct pla_prsr {
	pla_ec_fmtr_t * ec_fmtr;

	const pla_prsr_src_t * src;

	pla_tok_t tok;
	size_t tok_ind;
	pla_ec_pos_t prev_tok_pos_start;
	pla_ec_pos_t prev_tok_pos_end;

	bool is_rptd;
	pla_prsr_rse_t * rse;
};

PLA_API void pla_prsr_init(pla_prsr_t * prsr, pla_ec_fmtr_t * ec_fmtr);
PLA_API void pla_prsr_cleanup(pla_prsr_t * prsr);

PLA_API void pla_prsr_set_src(pla_prsr_t * prsr, const pla_prsr_src_t * src);

PLA_API bool pla_prsr_parse_tu(pla_prsr_t * prsr, pla_tu_t * tu);
