#include "pch.h"
#include "pla_tu.h"
#include "gia_tus.h"
#include "gia_text.h"

#define LINE_INIT_STR_SIZE 64

typedef struct gia_text_line_lex_ctx {
	wchar_t * ch;
	wchar_t * ch_end;
} line_lex_ctx_t;
typedef struct gia_text_prsr_ctx {
	gia_text_tok_line_t * line;
	gia_text_tok_line_t * line_end;

	gia_text_tok_t * tok;
	gia_text_tok_t * tok_end;
} text_prsr_ctx_t;

typedef enum gia_text_actn_type {
	ActnInsStr,
	ActnRemStr,
	Actn_Count
} actn_type_t;
typedef struct gia_text_actn {
	actn_type_t type;
	size_t line_pos;
	size_t ch_pos;
	union {
		struct {
			size_t str_size;
			wchar_t * str;
		} ins_str;
		struct {
			size_t line_pos_end;
			size_t ch_pos_end;
		} rem_str;
		gia_tus_t * tus;
	};
} actn_t;

static void init_line(gia_text_line_t * line) {
	*line = (gia_text_line_t){ 0 };

	line->str = malloc(LINE_INIT_STR_SIZE * sizeof(*line->str));

	ul_assert(line->str != NULL);

	line->cap = LINE_INIT_STR_SIZE;
}
static void cleanup_line(gia_text_line_t * line) {
	free(line->str);

	memset(line, 0, sizeof(*line));
}

static void insert_line_str(gia_text_line_t * line, size_t pos, size_t str_size, wchar_t * str) {
	ul_assert(pos <= line->size);

	if (line->size + str_size > line->cap) {
		ul_arr_grow(&line->cap, &line->str, sizeof(*line->str), line->size + str_size - line->cap);
	}

	wmemmove_s(line->str + pos + str_size, line->cap - pos, line->str + pos, line->size - pos);
	wmemcpy_s(line->str + pos, line->cap - pos, str, str_size);

	line->size += str_size;
}
static void remove_line_str(gia_text_line_t * line, size_t pos_start, size_t pos_end) {
	ul_assert(pos_start <= pos_end && pos_end <= line->size);

	wmemmove_s(line->str + pos_start, line->cap - pos_start, line->str + pos_end, line->size - pos_end);

	line->size -= pos_end - pos_start;
}


static void init_tok_line(gia_text_tok_line_t * tok_line) {
	*tok_line = (gia_text_tok_line_t){ 0 };
}
static void cleanup_tok_line(gia_text_tok_line_t * tok_line) {
	free(tok_line->toks);

	memset(tok_line, 0, sizeof(*tok_line));
}

static void push_tok_line_tok(gia_text_tok_line_t * line, pla_tok_t * tok) {
	if (line->size + 1 > line->cap) {
		ul_arr_grow(&line->cap, &line->toks, sizeof(*line->toks), 1);
	}

	line->toks[line->size++] = (gia_text_tok_t){ .base = *tok };
}


static void init_anlzr(gia_text_anlzr_t * anlzr, ul_es_ctx_t * es_ctx) {
	*anlzr = (gia_text_anlzr_t){ 0 };

	ul_arr_grow(&anlzr->tok_lines_cap, &anlzr->tok_lines, sizeof(*anlzr->tok_lines), 1);

	init_tok_line(&anlzr->tok_lines[anlzr->tok_lines_size++]);

	ul_hst_init(&anlzr->hst);

	pla_ec_sndr_init(&anlzr->ec_sndr, es_ctx);

	pla_ec_fmtr_init(&anlzr->ec_fmtr, &anlzr->ec_sndr.ec, &anlzr->hst);

	pla_lex_init(&anlzr->lex, &anlzr->hst, &anlzr->ec_fmtr);

	pla_prsr_init(&anlzr->prsr, &anlzr->ec_fmtr);
}
static void cleanup_anlzr(gia_text_anlzr_t * anlzr) {
	pla_tu_destroy(anlzr->tu);

	pla_prsr_cleanup(&anlzr->prsr);

	for (gia_text_tok_line_t * tok_line = anlzr->tok_lines, *tok_line_end = tok_line + anlzr->tok_lines_size; tok_line != tok_line_end; ++tok_line) {
		cleanup_tok_line(tok_line);
	}

	free(anlzr->tok_lines);

	pla_lex_cleanup(&anlzr->lex);

	pla_ec_fmtr_cleanup(&anlzr->ec_fmtr);

	pla_ec_sndr_cleanup(&anlzr->ec_sndr);

	ul_hst_cleanup(&anlzr->hst);

	memset(anlzr, 0, sizeof(*anlzr));
}

static bool get_line_ch(void * src_data, wchar_t * out) {
	line_lex_ctx_t * line_lex_ctx = src_data;

	if (line_lex_ctx->ch == line_lex_ctx->ch_end) {
		return false;
	}

	*out = *line_lex_ctx->ch++;

	return true;
}
static void update_anlzr_tok_line(gia_text_t * text, size_t line_pos) {
	gia_text_anlzr_t * const anlzr = &text->anlzr;

	ul_assert(line_pos < text->lines_size && line_pos < text->anlzr.tok_lines_size);

	pla_ec_clear_line(&anlzr->ec_sndr.ec, PLA_LEX_EC_GROUP, line_pos);

	gia_text_line_t * line = &text->lines[line_pos];
	gia_text_tok_line_t * tok_line = &text->anlzr.tok_lines[line_pos];

	line_lex_ctx_t line_lex_ctx = { .ch = line->str, .ch_end = line->str + line->size };

	pla_lex_set_src(&anlzr->lex, &line_lex_ctx, get_line_ch, line_pos, 0);

	tok_line->size = 0;

	while (anlzr->lex.ch_succ) {
		pla_lex_get_tok(&anlzr->lex);

		if (anlzr->lex.tok.type != PlaTokNone) {
			push_tok_line_tok(tok_line, &anlzr->lex.tok);
		}
	}

	pla_lex_reset_src(&anlzr->lex);
}

static void shift_lines_toks(gia_text_t * text, size_t start_pos, size_t shift_size, bool shift_rev) {
	ul_assert(start_pos <= text->lines_size);

	for (gia_text_tok_line_t * tok_line = text->anlzr.tok_lines + start_pos, *tok_line_end = text->anlzr.tok_lines + text->anlzr.tok_lines_size; tok_line != tok_line_end; ++tok_line) {
		for (gia_text_tok_t * tok = tok_line->toks, *tok_end = tok + tok_line->size; tok != tok_end; ++tok) {
			if (shift_rev) {
				tok->base.pos_start.line_num -= shift_size;
				tok->base.pos_end.line_num -= shift_size;
			}
			else {
				tok->base.pos_start.line_num += shift_size;
				tok->base.pos_end.line_num += shift_size;
			}
		}
	}

	pla_ec_shift(&text->anlzr.ec_sndr.ec, PLA_LEX_EC_GROUP, start_pos, shift_size, shift_rev);
}

static bool get_text_tok(void * src_data, pla_tok_t * out) {
	text_prsr_ctx_t * ctx = src_data;

	while (true) {
		if (ctx->tok != ctx->tok_end) {
			*out = (ctx->tok++)->base;
			break;
		}
		else if (ctx->line != ctx->line_end) {
			gia_text_tok_line_t * line = ctx->line++;

			ctx->tok = line->toks;
			ctx->tok_end = line->toks + line->size;
		}
		else {
			return false;
		}
	}

	return true;
}
static void update_anlzr_tu_nl(gia_text_t * text) {
	gia_text_anlzr_t * const anlzr = &text->anlzr;

	pla_ec_clear_group(&anlzr->ec_sndr.ec, PLA_PRSR_EC_GROUP);

	if (anlzr->tu != NULL) {
		pla_tu_destroy(anlzr->tu);
	}

	anlzr->tu = pla_tu_create(ul_hst_hashadd(&anlzr->hst, 0, L""));

	text_prsr_ctx_t text_prsr_ctx = { .line = text->anlzr.tok_lines, .line_end = text->anlzr.tok_lines + text->anlzr.tok_lines_size };

	pla_prsr_set_src(&anlzr->prsr, &text_prsr_ctx, get_text_tok);

	pla_prsr_parse_tu(&anlzr->prsr, anlzr->tu);

	pla_prsr_reset_src(&anlzr->prsr);
}

static void process_anlzr_actn_ins_str_nl(gia_text_t * text, actn_t * actn) {
	const size_t line_pos = actn->line_pos;
	gia_text_anlzr_t * const anlzr = &text->anlzr;

	if (text->lines_size > anlzr->tok_lines_size) {
		size_t ins_size = text->lines_size - anlzr->tok_lines_size;

		if (anlzr->tok_lines_size + ins_size > anlzr->tok_lines_cap) {
			ul_arr_grow(&anlzr->tok_lines_cap, &anlzr->tok_lines, sizeof(*anlzr->tok_lines), anlzr->tok_lines_size + ins_size - anlzr->tok_lines_cap);
		}

		memmove_s(anlzr->tok_lines + line_pos + 1 + ins_size, (anlzr->tok_lines_cap - line_pos - 1 - ins_size) * sizeof(*anlzr->tok_lines), anlzr->tok_lines + line_pos + 1, (anlzr->tok_lines_size - line_pos - 1) * sizeof(*anlzr->tok_lines));

		for (size_t tok_line_i = line_pos + 1, tok_line_i_end = tok_line_i + ins_size; tok_line_i < tok_line_i_end; ++tok_line_i) {
			init_tok_line(&anlzr->tok_lines[tok_line_i]);

			++anlzr->tok_lines_size;

			update_anlzr_tok_line(text, tok_line_i);
		}
	}

	ul_assert(text->lines_size == text->anlzr.tok_lines_size);

	update_anlzr_tok_line(text, actn->line_pos);

	update_anlzr_tu_nl(text);
}
static void process_anlzr_actn_rem_str_nl(gia_text_t * text, actn_t * actn) {
	const size_t start_line_pos = actn->line_pos, end_line_pos = actn->rem_str.line_pos_end;
	gia_text_anlzr_t * const anlzr = &text->anlzr;

	if (start_line_pos != end_line_pos) {
		size_t line_i_start = start_line_pos + 1, line_i_end = min(end_line_pos + 1, anlzr->tok_lines_size);

		for (size_t line_i = line_i_start; line_i < line_i_end; ++line_i) {
			cleanup_tok_line(&anlzr->tok_lines[line_i]);
		}

		memmove_s(anlzr->tok_lines + line_i_start, (anlzr->tok_lines_cap - line_i_start) * sizeof(*anlzr->tok_lines), anlzr->tok_lines + line_i_end, (anlzr->tok_lines_size - line_i_end) * sizeof(*anlzr->tok_lines));

		size_t lines_count = line_i_end - line_i_start;

		anlzr->tok_lines_size -= lines_count;

		shift_lines_toks(text, start_line_pos + 1, lines_count, true);
	}

	ul_assert(text->lines_size == text->anlzr.tok_lines_size);

	update_anlzr_tok_line(text, start_line_pos);

	update_anlzr_tu_nl(text);
}
static void process_anlzr_actn_nl(gia_text_t * text, actn_t * actn) {
	typedef void do_actn_proc_t(gia_text_t * text, actn_t * actn);

	static do_actn_proc_t * const procs[Actn_Count] = {
		[ActnInsStr] = process_anlzr_actn_ins_str_nl,
		[ActnRemStr] = process_anlzr_actn_rem_str_nl
	};

	do_actn_proc_t * proc = procs[actn->type];

	ul_assert(proc != NULL);

	proc(text, actn);
}


void gia_text_init(gia_text_t * text, ul_es_ctx_t * es_ctx) {
	*text = (gia_text_t){ 0 };

	ul_arr_grow(&text->lines_cap, &text->lines, sizeof(*text->lines), 1);

	init_line(&text->lines[text->lines_size++]);

	init_anlzr(&text->anlzr, es_ctx);

	InitializeCriticalSection(&text->lock);
}
void gia_text_cleanup(gia_text_t * text) {
	DeleteCriticalSection(&text->lock);
	
	cleanup_anlzr(&text->anlzr);

	for (gia_text_line_t * line = text->lines, *line_end = line + text->lines_size; line != line_end; ++line) {
		cleanup_line(line);
	}

	free(text->lines);

	memset(text, 0, sizeof(*text));
}

static void process_actn_ins_str_nl(gia_text_t * text, actn_t * actn) {
	const size_t line_pos = actn->line_pos, ins_pos = actn->ch_pos, str_size = actn->ins_str.str_size;
	wchar_t * const str = actn->ins_str.str;

	ul_assert(line_pos < text->lines_size);
	
	size_t line_i = line_pos, line_i_ins = ins_pos;
	wchar_t * cur = str, * cur_end = cur + str_size;

	while (true) {
		wchar_t * cur_nl = wmemchr(cur, L'\n', cur_end - cur);

		if (cur_nl == NULL) {
			cur_nl = cur_end;
		}

		size_t cur_size = cur_nl - cur;

		{
			gia_text_line_t * line = &text->lines[line_i++];

			line_i_ins = min(line_i_ins, line->size);

			insert_line_str(line, line_i_ins, cur_size, cur);
		}

		if (cur_nl == cur_end) {
			break;
		}

		if (text->lines_size + 1 > text->lines_cap) {
			ul_arr_grow(&text->lines_cap, &text->lines, sizeof(*text->lines), 1);
		}

		memmove_s(text->lines + line_i + 1, (text->lines_cap - line_i) * sizeof(*text->lines), text->lines + line_i, (text->lines_size - line_i) * sizeof(*text->lines));

		++text->lines_size;

		{		
			gia_text_line_t * prev_line = &text->lines[line_i - 1];
			gia_text_line_t * line = &text->lines[line_i];

			init_line(&text->lines[line_i]);

			insert_line_str(line, 0, prev_line->size - cur_size - line_i_ins, prev_line->str + cur_size + line_i_ins);
			remove_line_str(prev_line, cur_size + line_i_ins, prev_line->size);

			line_i_ins = 0;
		}

		if (cur_nl + 1 == cur_end) {
			break;
		}

		cur = cur_nl + 1;
	}
}
static void process_actn_rem_str_nl(gia_text_t * text, actn_t * actn) {
	const size_t start_line_pos = actn->line_pos, start_ch_pos = actn->ch_pos, end_line_pos = actn->rem_str.line_pos_end, end_ch_pos = actn->rem_str.ch_pos_end;

	ul_assert(start_line_pos < end_line_pos || start_line_pos == end_line_pos && start_ch_pos <= end_ch_pos);

	if (start_line_pos >= text->lines_size) {
		return;
	}

	if (start_line_pos == end_line_pos) {
		gia_text_line_t * line = &text->lines[start_line_pos];

		remove_line_str(line, min(start_ch_pos, line->size), min(end_ch_pos, line->size));
	}
	else {
		gia_text_line_t * line = &text->lines[start_line_pos];

		remove_line_str(line, min(start_ch_pos, line->size), line->size);

		if (end_line_pos < text->lines_size) {
			gia_text_line_t * last_line = &text->lines[end_line_pos];

			size_t ins_size = min(end_ch_pos, last_line->size);

			insert_line_str(line, line->size, last_line->size - ins_size, last_line->str + ins_size);
		}

		size_t line_i_start = start_line_pos + 1, line_i_end = min(end_line_pos + 1, text->lines_size);
		
		for (size_t line_i = line_i_start; line_i < line_i_end; ++line_i) {
			cleanup_line(&text->lines[line_i]);
		}

		memmove_s(text->lines + line_i_start, (text->lines_cap - line_i_start) * sizeof(*text->lines), text->lines + line_i_end, (text->lines_size - line_i_end) * sizeof(*text->lines));

		text->lines_size -= line_i_end - line_i_start;
	}
}
static void process_actn_read_tus_nl(gia_text_t * text, actn_t * actn) {
	gia_tus_t * const tus = actn->tus;

	size_t line_i = 0;

	wchar_t * ch = tus->src, * ch_end = ch + tus->src_size;

	while (true) {
		wchar_t * nl_ch = ch;

		while (nl_ch != ch_end && *nl_ch != L'\n') {
			++nl_ch;
		}

		while (line_i >= text->lines_size) {
			if (text->lines_size + 1 > text->lines_cap) {
				ul_arr_grow(&text->lines_cap, &text->lines, sizeof(*text->lines), 1);
			}

			init_line(&text->lines[text->lines_size++]);
		}

		gia_text_line_t * line = &text->lines[line_i];

		line->size = 0;

		size_t ins_size = nl_ch - ch;

		if (ins_size > line->cap) {
			ul_arr_grow(&line->cap, &line->str, sizeof(*line->str), ins_size - line->cap);
		}

		wmemcpy_s(line->str, line->cap, ch, ins_size);

		line->size = ins_size;

		if (nl_ch == ch_end || nl_ch + 1 == ch_end) {
			break;
		}

		ch = nl_ch + 1;
		++line_i;
	}

	++line_i;

	while (line_i < text->lines_size) {
		cleanup_line(&text->lines[--text->lines_size]);
	}
}
static void process_actn_nl(gia_text_t * text, actn_t * actn) {
	typedef void do_actn_proc_t(gia_text_t * text, actn_t * actn);

	static do_actn_proc_t * const procs[Actn_Count] = {
		[ActnInsStr] = process_actn_ins_str_nl,
		[ActnRemStr] = process_actn_rem_str_nl
	};

	do_actn_proc_t * proc = procs[actn->type];

	ul_assert(proc != NULL);

	proc(text, actn);

	process_anlzr_actn_nl(text, actn);
}

void gia_text_insert_str_nl(gia_text_t * text, size_t line_pos, size_t ins_pos, size_t str_size, wchar_t * str) {
	actn_t actn = { .type = ActnInsStr, .line_pos = line_pos, .ch_pos = ins_pos, .ins_str = { .str_size = str_size, .str = str } };

	process_actn_nl(text, &actn);
}
void gia_text_remove_str_nl(gia_text_t * text, size_t start_line_pos, size_t start_ch_pos, size_t end_line_pos, size_t end_ch_pos) {
	actn_t actn = { .type = ActnRemStr, .line_pos = start_line_pos, .ch_pos = start_ch_pos, .rem_str = { .line_pos_end = end_line_pos, .ch_pos_end = end_ch_pos } };

	process_actn_nl(text, &actn);
}

void gia_text_from_tus_nl(gia_text_t * text, gia_tus_t * tus) {
	gia_text_remove_str_nl(text, 0, 0, text->lines_size, SIZE_MAX);
	gia_text_insert_str_nl(text, 0, 0, tus->src_size, tus->src);
}
void gia_text_to_tus_nl(gia_text_t * text, gia_tus_t * tus) {
	tus->src_size = 0;

	wchar_t nl_ch = L'\n';

	for (gia_text_line_t * line = text->lines, *line_end = line + text->lines_size; line != line_end; ++line) {
		gia_tus_insert_str_nl(tus, tus->src_size, line->size, line->str);
		gia_tus_insert_str_nl(tus, tus->src_size, 1, &nl_ch);
	}
}
