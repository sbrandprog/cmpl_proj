#pragma once
#include "pla_tok.h"

#define PLA_LEX_EC_GROUP 1

typedef bool pla_lex_get_src_ch_proc_t(void * src_data, wchar_t * out);

struct pla_lex {
	ul_hst_t * hst;
	pla_ec_t * ec;

	ul_hs_t * emp_hs;

	ul_hsb_t hsb;

	pla_lex_get_src_ch_proc_t * get_src_ch_proc;
	void * src_data;

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

void pla_lex_init(pla_lex_t * lex, ul_hst_t * hst, pla_ec_t * ec);
void pla_lex_cleanup(pla_lex_t * lex);

void pla_lex_update_src(pla_lex_t * lex, pla_lex_get_src_ch_proc_t * get_src_ch_proc, void * src_data);
void pla_lex_set_src(pla_lex_t * lex, pla_lex_get_src_ch_proc_t * get_src_ch_proc, void * src_data, size_t line_num, size_t line_ch);

bool pla_lex_get_tok(pla_lex_t * lex);
