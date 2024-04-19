#include "pch.h"
#include "pla_punc.h"
#include "pla_keyw.h"
#include "pla_lex.h"

typedef bool ch_pred_t(wchar_t ch);
typedef bool ch_proc_t(pla_lex_t * lex, wchar_t * out);

static bool get_src_ch_proc_dflt(void * src_data, wchar_t * out) {
	return false;
}

void pla_lex_init(pla_lex_t * lex, ul_hst_t * hst) {
	*lex = (pla_lex_t){ .hst = hst, .get_src_ch_proc = get_src_ch_proc_dflt };
}
void pla_lex_cleanup(pla_lex_t * lex) {
	free(lex->str);

	memset(lex, 0, sizeof(*lex));
}

static bool next_ch_fc(pla_lex_t * lex, bool first_ch) {
	lex->ch_succ = false;

	if (!lex->get_src_ch_proc(lex->src_data, &lex->ch)) {
		return false;
	}

	lex->ch_succ = true;

	if (!first_ch) {
		++lex->line_ch;
	}

	if (lex->line_switch) {
		lex->line_switch = false;
		++lex->line_num;
		lex->line_ch = 0;
	}

	if (lex->ch == L'\n') {
		lex->line_switch = true;
	}

	return true;
}
static bool next_ch(pla_lex_t * lex) {
	return next_ch_fc(lex, false);
}

void pla_lex_update_src(pla_lex_t * lex, pla_lex_get_src_ch_proc_t * get_src_ch_proc, void * src_data) {
	lex->get_src_ch_proc = get_src_ch_proc;
	lex->src_data = src_data;

	lex->ch = 0;
	lex->ch_succ = false;

	if (!next_ch_fc(lex, true)) {
		return;
	}
}
void pla_lex_set_src(pla_lex_t * lex, pla_lex_get_src_ch_proc_t * get_src_ch_proc, void * src_data, size_t line_num, size_t line_ch) {
	lex->line_switch = false;
	lex->line_num = line_num;
	lex->line_ch = line_ch;

	pla_lex_update_src(lex, get_src_ch_proc, src_data);
}

static pla_tok_pos_t get_tok_pos(pla_lex_t * lex) {
	return (pla_tok_pos_t) {
		.line_num = lex->line_num, .line_ch = lex->line_ch
	};
}
static void report_lex(pla_lex_t * lex, pla_lex_err_type_t err_type) {
	if (lex->err_stack_pos > PLA_LEX_ERR_STACK_SIZE) {
		return;
	}

	lex->err_stack[lex->err_stack_pos++] = (pla_lex_err_t){ .type = err_type, .pos = get_tok_pos(lex) };
}

static void clear_str(pla_lex_t * lex) {
	lex->str_size = 0;
	lex->str_hash = 0;
}
static void push_str_ch(pla_lex_t * lex, wchar_t ch) {
	if (lex->str_size + 1 > lex->str_cap) {
		ul_arr_grow(&lex->str_cap, &lex->str, sizeof(*lex->str), 1);
	}

	lex->str[lex->str_size++] = ch;
	lex->str_hash = ul_hs_hash_ch(lex->str_hash, ch);
}
static bool fetch_str(pla_lex_t * lex, ch_pred_t * pred, ch_proc_t * proc) {
	clear_str(lex);

	do {
		wchar_t str_ch = lex->ch;

		if (proc != NULL) {
			if (!proc(lex, &str_ch)) {
				return false;
			}
		}

		push_str_ch(lex, str_ch);
	} while (next_ch(lex) && pred(lex->ch));

	return true;
}
static ul_hs_t * hadd_str(pla_lex_t * lex) {
	return ul_hst_add(lex->hst, lex->str_size, lex->str, lex->str_hash);
}
static bool fadd_str(pla_lex_t * lex, ch_pred_t * pred, ch_proc_t * proc, ul_hs_t ** out) {
	if (!fetch_str(lex, pred, proc)) {
		return false;
	}

	*out = hadd_str(lex);

	return true;
}
static bool ch_str_ch_proc(pla_lex_t * lex, wchar_t * out) {
	if (lex->ch == L'\\') {
		if (!next_ch(lex)) {
			return false;
		}

		switch (lex->ch) {
			case L'a':
				*out = L'\a';
				break;
			case L'b':
				*out = L'\b';
				break;
			case L'f':
				*out = L'\f';
				break;
			case L'n':
				*out = L'\n';
				break;
			case L'r':
				*out = L'\r';
				break;
			case L't':
				*out = L'\t';
				break;
			case L'v':
				*out = L'\v';
				break;
			case L'\\':
				*out = L'\\';
				break;
			case L'\'':
				*out = L'\'';
				break;
			case L'\"':
				*out = L'\"';
				break;
			default:
				report_lex(lex, PlaLexErrInvEscSeq);
				return false;
		}
	}

	return true;
}
static void get_tok_core(pla_lex_t * lex) {
	lex->tok.type = PlaTokNone;
	lex->err_stack_pos = 0;

	if (!lex->ch_succ) {
		report_lex(lex, PlaLexErrSrc);
		return;
	}

	while (true) {
		lex->tok.pos_start = get_tok_pos(lex);

		if (pla_tok_is_emp_ch(lex->ch)) {
			do {
				if (!next_ch(lex)) {
					report_lex(lex, PlaLexErrSrc);
					return;
				}
			} while (pla_tok_is_emp_ch(lex->ch));
		}
		else if (pla_tok_is_punc_ch(lex->ch)) {
			clear_str(lex);

			push_str_ch(lex, lex->ch);

			size_t punc_len;
			pla_punc_t punc = pla_punc_fetch_best(lex->str_size, lex->str, &punc_len);

			if (punc == PlaPuncNone) {
				next_ch(lex);
				report_lex(lex, PlaLexErrInvPunc);
				return;
			}

			size_t punc_len_old = punc_len;

			while (next_ch(lex) && pla_tok_is_punc_ch(lex->ch)) {
				push_str_ch(lex, lex->ch);

				punc = pla_punc_fetch_best(lex->str_size, lex->str, &punc_len);

				if (punc_len <= punc_len_old) {
					break;
				}

				punc_len_old = punc_len;
			}

			lex->tok.type = PlaTokPunc;
			lex->tok.punc = punc;

			return;
		}
		else if (pla_tok_is_first_ident_ch(lex->ch)) {
			bool res = fetch_str(lex, pla_tok_is_ident_ch, NULL);
			ul_assert(res);

			pla_keyw_t keyw = pla_keyw_fetch_exact(lex->str_size, lex->str);

			if (keyw != PlaKeywNone) {
				lex->tok.type = PlaTokKeyw;
				lex->tok.keyw = keyw;

				return;
			}

			ul_hs_t * str = hadd_str(lex);

			lex->tok.type = PlaTokIdent;
			lex->tok.ident = str;

			return;
		}
		else if (pla_tok_is_dqoute_ch(lex->ch)) {
			if (!next_ch(lex)) {
				report_lex(lex, PlaLexErrSrc);
				return;
			}

			ul_hs_t * data;

			if (!fadd_str(lex, pla_tok_is_ch_str_ch, ch_str_ch_proc, &data)) {
				return;
			}

			if (!pla_tok_is_dqoute_ch(lex->ch)) {
				report_lex(lex, PlaLexErrInvChStr);
				return;
			}
			
			if (!next_ch(lex)) {
				report_lex(lex, PlaLexErrSrc);
				return;
			}
			
			if (!pla_tok_is_tag_ch(lex->ch)) {
				report_lex(lex, PlaLexErrInvChStr);
				return;
			}

			ul_hs_t * tag;

			bool res = fadd_str(lex, pla_tok_is_tag_ch, NULL, &tag);
			ul_assert(res);

			lex->tok.type = PlaTokChStr;
			lex->tok.ch_str.data = data;
			lex->tok.ch_str.tag = tag;

			return;
		}
		else if (pla_tok_is_first_num_str_ch(lex->ch)) {
			ul_hs_t * data;

			bool res = fadd_str(lex, pla_tok_is_num_str_ch, NULL, &data);
			ul_assert(res);

			if (!pla_tok_is_num_str_tag_intro_ch(lex->ch)) {
				report_lex(lex, PlaLexErrInvNumStr);
				return;
			}

			if (!next_ch(lex)) {
				report_lex(lex, PlaLexErrSrc);
				return;
			}

			if (!pla_tok_is_tag_ch(lex->ch)) {
				report_lex(lex, PlaLexErrInvNumStr);
				return;
			}

			ul_hs_t * tag;

			res = fadd_str(lex, pla_tok_is_tag_ch, NULL, &tag);
			ul_assert(res);

			lex->tok.type = PlaTokNumStr;
			lex->tok.num_str.data = data;
			lex->tok.num_str.tag = tag;

			return;
		}
		else {
			report_lex(lex, PlaLexErrUnkCh);
			return;
		}
	}
}
bool pla_lex_get_tok(pla_lex_t * lex) {
	get_tok_core(lex);

	if (lex->err_stack_pos > 0) {
		return false;
	}

	ul_assert(lex->tok.type != PlaTokNone);

	lex->tok.pos_end = get_tok_pos(lex);

	return true;
}

const ul_ros_t pla_lex_err_strs[PlaLexErr_Count] = {
	[PlaLexErrSrc] = UL_ROS_MAKE(L"source error"),
	[PlaLexErrInvPunc] = UL_ROS_MAKE(L"invalid punctuator"),
	[PlaLexErrInvEscSeq] = UL_ROS_MAKE(L"invalid escape sequence"),
	[PlaLexErrInvChStr] = UL_ROS_MAKE(L"invalid character string"),
	[PlaLexErrInvNumStr] = UL_ROS_MAKE(L"invalid number string"),
	[PlaLexErrUnkCh] = UL_ROS_MAKE(L"unknown character")
};
