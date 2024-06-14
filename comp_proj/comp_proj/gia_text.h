#pragma once
#include "pla_ec_sndr.h"
#include "pla_ec_fmtr.h"
#include "pla_lex.h"
#include "pla_prsr.h"
#include "gia.h"

struct gia_text_ch_pos {
	size_t line;
	size_t ch;
};
struct gia_text_line {
	size_t cap;
	wchar_t * str;
	size_t size;
};
struct gia_text_tok {
	pla_tok_t base;
};
struct gia_text_tok_line {
	size_t cap;
	gia_text_tok_t * toks;
	size_t size;
};
struct gia_text_anlzr {
	ul_hst_t hst;
	pla_ec_sndr_t ec_sndr;
	pla_ec_fmtr_t ec_fmtr;

	pla_lex_t lex;
	size_t tok_lines_cap;
	gia_text_tok_line_t * tok_lines;
	size_t tok_lines_size;

	pla_prsr_t prsr;
	pla_tu_t * tu;
};
struct gia_text {
	CRITICAL_SECTION lock;

	size_t lines_cap;
	gia_text_line_t * lines;
	size_t lines_size;

	gia_text_anlzr_t anlzr;
};

void gia_text_init(gia_text_t * text, ul_es_ctx_t * es_ctx);
void gia_text_cleanup(gia_text_t * text);

void gia_text_insert_str_nl(gia_text_t * text, size_t line_pos, size_t ins_pos, size_t str_size, wchar_t * str);
inline void gia_text_insert_ch_nl(gia_text_t * text, size_t line_pos, size_t ins_pos, wchar_t ch) {
	gia_text_insert_str_nl(text, line_pos, ins_pos, 1, &ch);
}
void gia_text_remove_str_nl(gia_text_t * text, size_t start_line_pos, size_t start_ch_pos, size_t end_line_pos, size_t end_ch_pos);
inline void gia_text_remove_ch_nl(gia_text_t * text, size_t line_pos, size_t rem_pos) {
	gia_text_remove_str_nl(text, line_pos, rem_pos, line_pos, rem_pos + 1);
}

void gia_text_from_tus_nl(gia_text_t * text, gia_tus_t * tus);
void gia_text_to_tus_nl(gia_text_t * text, gia_tus_t * tus);
