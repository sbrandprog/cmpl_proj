#include "pch.h"
#include "pla_ec_fmtr.h"
#include "pla_punc.h"
#include "pla_keyw.h"
#include "pla_lex.h"

typedef bool ch_pred_t(wchar_t ch);
typedef void ch_proc_t(pla_lex_t * lex, wchar_t * out);

void pla_lex_init(pla_lex_t * lex, ul_hst_t * hst, pla_ec_fmtr_t * ec_fmtr) {
	*lex = (pla_lex_t){ .hst = hst, .ec_fmtr = ec_fmtr };

	static const ul_ros_t emp_str_raw = UL_ROS_MAKE(L"");

	lex->emp_hs = ul_hst_hashadd(hst, emp_str_raw.size, emp_str_raw.str);
}
void pla_lex_cleanup(pla_lex_t * lex) {
	free(lex->str);

	memset(lex, 0, sizeof(*lex));
}

static bool next_ch(pla_lex_t * lex) {
	if (lex->ch_succ) {
		++lex->line_ch;
	}

	lex->ch_succ = false;

	if (lex->src == NULL || !lex->src->get_ch_proc(lex->src->data, &lex->ch)) {
		return false;
	}

	lex->ch_succ = true;

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

static void update_src(pla_lex_t * lex, const pla_lex_src_t * src) {
	lex->src = src;

	lex->ch = 0;
	lex->ch_succ = false;

	if (!next_ch(lex)) {
		return;
	}
}
void pla_lex_set_src(pla_lex_t * lex, const pla_lex_src_t * src) {
	lex->line_switch = false;
	if (src != NULL) {
		lex->line_num = src->line_num;
		lex->line_ch = src->line_ch;
	}
	else {
		lex->line_num = 0;
		lex->line_ch = 0;
	}

	update_src(lex, src);
}

static pla_ec_pos_t get_ec_pos(pla_lex_t * lex) {
	return (pla_ec_pos_t) {
		.line_num = lex->line_num, .line_ch = lex->line_ch
	};
}
static void report_lex(pla_lex_t * lex, const wchar_t * fmt, ...) {
	lex->is_rptd = true;
	
	va_list args;

	va_start(args, fmt);

	pla_ec_fmtr_formatpost_va(lex->ec_fmtr, PLA_LEX_EC_GROUP, (lex->src != NULL ? lex->src->name : NULL), lex->tok.pos_start, get_ec_pos(lex), fmt, args);

	va_end(args);
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
static void fetch_str(pla_lex_t * lex, ch_pred_t * pred, ch_proc_t * proc) {
	clear_str(lex);

	do {
		wchar_t str_ch = lex->ch;

		if (proc != NULL) {
			proc(lex, &str_ch);
		}

		push_str_ch(lex, str_ch);
	} while (next_ch(lex) && pred(lex->ch));
}
static ul_hs_t * hadd_str(pla_lex_t * lex) {
	return ul_hst_add(lex->hst, lex->str_size, lex->str, lex->str_hash);
}
static void fadd_str(pla_lex_t * lex, ch_pred_t * pred, ch_proc_t * proc, ul_hs_t ** out) {
	fetch_str(lex, pred, proc);

	*out = hadd_str(lex);
}
static void ch_str_ch_proc(pla_lex_t * lex, wchar_t * out) {
	if (lex->ch == L'\\') {
		if (!next_ch(lex)) {
			report_lex(lex, L"invalid escape sequence: no character", lex->ch);
			return;
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
				report_lex(lex, L"invalid escape sequence: \\%c", lex->ch);
				break;
		}
	}
}
static void get_tok_core(pla_lex_t * lex) {
	if (!lex->ch_succ) {
		return;
	}

	while (true) {
		lex->tok.pos_start = get_ec_pos(lex);

		if (pla_tok_is_emp_ch(lex->ch)) {
			do {
				if (!next_ch(lex)) {
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
				report_lex(lex, L"invalid punctuator: undefined punctuator");
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
			fetch_str(lex, pla_tok_is_ident_ch, NULL);

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
			ul_hs_t * data = lex->emp_hs, * tag = lex->emp_hs;

			do {
				if (!next_ch(lex)) {
					report_lex(lex, L"invalid character string: no string characters");
					break;
				}

				if (lex->ch_succ && !pla_tok_is_dqoute_ch(lex->ch)) {
					fadd_str(lex, pla_tok_is_ch_str_ch, ch_str_ch_proc, &data);
				}

				if (!lex->ch_succ || !pla_tok_is_dqoute_ch(lex->ch)) {
					report_lex(lex, L"invalid character string: no enclosing double-quote");
					break;
				}

				if (!next_ch(lex) || !pla_tok_is_tag_ch(lex->ch)) {
					report_lex(lex, L"invalid character string: no type-tag");
					break;
				}

				fadd_str(lex, pla_tok_is_tag_ch, NULL, &tag);

			} while (false);

			lex->tok.type = PlaTokChStr;
			lex->tok.ch_str.data = data;
			lex->tok.ch_str.tag = tag;

			return;
		}
		else if (pla_tok_is_first_num_str_ch(lex->ch)) {
			ul_hs_t * data = lex->emp_hs, * tag = lex->emp_hs;

			do {
				fadd_str(lex, pla_tok_is_num_str_ch, NULL, &data);

				if (!lex->ch_succ || !pla_tok_is_num_str_tag_intro_ch(lex->ch) || !next_ch(lex) || !pla_tok_is_tag_ch(lex->ch)) {
					report_lex(lex, L"invalid number string: no type-tag");
					break;
				}

				fadd_str(lex, pla_tok_is_tag_ch, NULL, &tag);

			} while (false);

			lex->tok.type = PlaTokNumStr;
			lex->tok.num_str.data = data;
			lex->tok.num_str.tag = tag;

			return;
		}
		else {
			report_lex(lex, L"undefined character: %c", lex->ch);
			next_ch(lex);
			return;
		}
	}
}
bool pla_lex_get_tok(pla_lex_t * lex) {
	lex->is_rptd = false;
	lex->tok.type = PlaTokNone;
	lex->tok.pos_start = get_ec_pos(lex);

	get_tok_core(lex);

	lex->tok.pos_end = get_ec_pos(lex);

	return lex->tok.type != PlaTokNone;
}
