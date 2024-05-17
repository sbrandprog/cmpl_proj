#pragma once
#include "pla_tok.h"

#define PLA_PRSR_EC_GROUP 2

typedef bool pla_prsr_get_tok_proc_t(void * src_data, pla_tok_t * out);

typedef struct pla_prsr {
	pla_ec_fmtr_t * ec_fmtr;

	void * src_data;
	pla_prsr_get_tok_proc_t * get_tok_proc;

	pla_tok_t tok;
} pla_psrs_t;

void pla_prsr_init(pla_prsr_t * prsr, pla_ec_fmtr_t * ec_fmtr);
void pla_prsr_cleanup(pla_prsr_t * prsr);

void pla_prsr_set_src(pla_prsr_t * prsr, void * src_data, pla_prsr_get_tok_proc_t * get_tok_proc);
void pla_prsr_reset_src(pla_prsr_t * prsr);

bool pla_prsr_parse_cn(pla_prsr_t * prsr, pla_punc_t delim, pla_cn_t ** out);
bool pla_prsr_parse_expr(pla_prsr_t * prsr, pla_expr_t ** out);
bool pla_prsr_parse_stmt(pla_prsr_t * prsr, pla_stmt_t ** out);
bool pla_prsr_parse_dclr(pla_prsr_t * prsr, pla_dclr_t ** out);
bool pla_prsr_parse_tu(pla_prsr_t * prsr, pla_tu_t * tu);
