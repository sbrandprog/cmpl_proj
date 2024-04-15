#pragma once
#include "pla_tok.h"

#define PLA_LEX_ERR_STACK_SIZE 4

typedef bool pla_lex_get_src_ch_proc_t(void * src_data, wchar_t * out);

struct pla_lex_tok_pos {
	size_t line_num;
	size_t line_ch;
};
enum pla_lex_err_type {
	PlaLexErrSrc,
	PlaLexErrInvPunc,
	PlaLexErrInvEscSeq,
	PlaLexErrInvChStr,
	PlaLexErrInvNumStr,
	PlaLexErrUnkCh,
	PlaLexErr_Count
};
struct pla_lex_err {
	pla_lex_err_type_t type;
	pla_lex_tok_pos_t pos;
};
struct pla_lex {
	ul_hst_t * hst;
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
	pla_lex_tok_pos_t tok_start;
	pla_lex_tok_pos_t tok_end;

	pla_lex_err_t err_stack[PLA_LEX_ERR_STACK_SIZE];
	size_t err_stack_pos;
};

void pla_lex_init(pla_lex_t * lex, ul_hst_t * hst);
void pla_lex_cleanup(pla_lex_t * lex);

void pla_lex_update_src(pla_lex_t * lex, pla_lex_get_src_ch_proc_t * get_src_ch_proc, void * src_data);
void pla_lex_set_src(pla_lex_t * lex, pla_lex_get_src_ch_proc_t * get_src_ch_proc, void * src_data);

bool pla_lex_get_tok(pla_lex_t * lex);

extern const ul_ros_t pla_lex_err_strs[PlaLexErr_Count];
