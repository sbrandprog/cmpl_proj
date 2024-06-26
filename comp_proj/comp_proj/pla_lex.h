#pragma once
#include "pla_tok.h"

#define PLA_LEX_EC_GROUP 1

struct pla_lex_src {
	ul_hs_t * name;
	void * data;
	pla_lex_src_get_ch_proc_t * get_ch_proc;
	size_t line_num;
	size_t line_ch;
};
struct pla_lex {
	ul_hst_t * hst;
	pla_ec_fmtr_t * ec_fmtr;

	ul_hs_t * emp_hs;

	const pla_lex_src_t * src;

	wchar_t ch;
	bool ch_succ;

	bool line_switch;
	size_t line_num;
	size_t line_ch;

	size_t str_size;
	wchar_t * str;
	size_t str_cap;
	ul_hs_hash_t str_hash;

	pla_tok_t tok;

	bool is_rptd;
};

void pla_lex_init(pla_lex_t * lex, ul_hst_t * hst, pla_ec_fmtr_t * ec_fmtr);
void pla_lex_cleanup(pla_lex_t * lex);

void pla_lex_set_src(pla_lex_t * lex, const pla_lex_src_t * src);

bool pla_lex_get_tok(pla_lex_t * lex);
