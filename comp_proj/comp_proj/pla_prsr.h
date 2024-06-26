#pragma once
#include "pla_tok.h"

#define PLA_PRSR_EC_GROUP 2

typedef bool pla_prsr_get_tok_proc_t(void * src_data, pla_tok_t * out);

struct pla_prsr_rse {
	pla_prsr_rse_t * prev;
	bool is_rptd;
	bool set_next;
	bool get_next;
};
struct pla_prsr {
	pla_ec_fmtr_t * ec_fmtr;

	void * src_data;
	pla_prsr_get_tok_proc_t * get_tok_proc;

	pla_tok_t tok;
	size_t tok_ind;
	pla_ec_pos_t prev_tok_pos_start;
	pla_ec_pos_t prev_tok_pos_end;

	bool is_rptd;
	pla_prsr_rse_t * rse;
};

void pla_prsr_init(pla_prsr_t * prsr, pla_ec_fmtr_t * ec_fmtr);
void pla_prsr_cleanup(pla_prsr_t * prsr);

void pla_prsr_set_src(pla_prsr_t * prsr, void * src_data, pla_prsr_get_tok_proc_t * get_tok_proc);
void pla_prsr_reset_src(pla_prsr_t * prsr);

bool pla_prsr_parse_tu(pla_prsr_t * prsr, pla_tu_t * tu);
