#include "pch.h"
#include "gia_tus.h"
#include "gia_text.h"

#define LINE_INIT_STR_SIZE 64

typedef struct gia_text_line_lex_ctx {
	wchar_t * ch;
	wchar_t * ch_end;
} line_lex_ctx_t;

static void init_line(gia_text_line_t * line) {
	*line = (gia_text_line_t){ 0 };

	line->str = malloc(LINE_INIT_STR_SIZE * sizeof(*line->str));

	ul_assert(line->str != NULL);

	line->cap = LINE_INIT_STR_SIZE;
}
static void cleanup_line(gia_text_line_t * line) {
	free(line->toks);
	free(line->str);

	memset(line, 0, sizeof(*line));
}

static void insert_line_tok(gia_text_line_t * line, pla_tok_t * tok) {
	if (line->toks_size + 1 > line->toks_cap) {
		ul_arr_grow(&line->toks_cap, &line->toks, sizeof(*line->toks), 1);
	}

	line->toks[line->toks_size++] = *tok;
}


void gia_text_init(gia_text_t * text) {
	*text = (gia_text_t){ 0 };

	ul_arr_grow(&text->lines_cap, &text->lines, sizeof(*text->lines), 1);

	init_line(&text->lines[text->lines_size++]);

	InitializeCriticalSection(&text->lock);

	ul_hst_init(&text->lex_hst);

	pla_lex_init(&text->lex, &text->lex_hst);
}
void gia_text_cleanup(gia_text_t * text) {
	pla_lex_cleanup(&text->lex);

	ul_hst_cleanup(&text->lex_hst);
	
	DeleteCriticalSection(&text->lock);

	for (gia_text_line_t * line = text->lines, *line_end = line + text->lines_size; line != line_end; ++line) {
		cleanup_line(line);
	}

	free(text->lines);

	memset(text, 0, sizeof(*text));
}

static bool get_line_ch(void * user_data, wchar_t * out) {
	line_lex_ctx_t * line_lex_ctx = user_data;

	if (line_lex_ctx->ch == line_lex_ctx->ch_end) {
		return false;
	}

	*out = *line_lex_ctx->ch++;

	return true;
}
static void update_line_toks_nl(gia_text_t * text, size_t line_pos) {
	ul_assert(line_pos < text->lines_size);

	gia_text_line_t * line = &text->lines[line_pos];

	line_lex_ctx_t line_lex_ctx = { .ch = line->str, .ch_end = line->str + line->size };

	pla_lex_set_src(&text->lex, get_line_ch, &line_lex_ctx, line_pos, 0);

	line->toks_size = 0;

	while (line_lex_ctx.ch != line_lex_ctx.ch_end) {
		if (pla_lex_get_tok(&text->lex)) {
			insert_line_tok(line, &text->lex.tok);
		}
	}
}

void gia_text_insert_str_nl(gia_text_t * text, size_t line_pos, size_t ins_pos, size_t str_size, wchar_t * str) {
	ul_assert(line_pos < text->lines_size);

	gia_text_line_t * line = &text->lines[line_pos];

	ul_assert(ins_pos <= line->size);

	if (line->size + str_size > line->cap) {
		ul_arr_grow(&line->cap, &line->str, sizeof(*line->str), line->size + str_size - line->cap);
	}

	wmemmove_s(line->str + ins_pos + str_size, line->cap - ins_pos, line->str + ins_pos, line->size - ins_pos);
	wmemcpy_s(line->str + ins_pos, line->cap - ins_pos, str, str_size);

	line->size += str_size;

	update_line_toks_nl(text, line_pos);
}
void gia_text_remove_str_nl(gia_text_t * text, size_t line_pos, size_t rem_pos_start, size_t rem_pos_end) {
	ul_assert(line_pos < text->lines_size);

	gia_text_line_t * line = &text->lines[line_pos];

	ul_assert(rem_pos_start <= rem_pos_end && rem_pos_end <= line->size);

	wmemmove_s(line->str + rem_pos_start, line->cap - rem_pos_start, line->str + rem_pos_end, line->size - rem_pos_end);

	line->size -= rem_pos_end - rem_pos_start;

	update_line_toks_nl(text, line_pos);
}

void gia_text_insert_line_nl(gia_text_t * text, size_t ins_pos) {
	ul_assert(ins_pos <= text->lines_size);

	if (text->lines_size + 1 > text->lines_cap) {
		ul_arr_grow(&text->lines_cap, &text->lines, sizeof(*text->lines), 1);
	}

	gia_text_line_t * ins_it = text->lines + ins_pos;

	memmove_s(ins_it + 1, (text->lines_cap - ins_pos) * sizeof(*ins_it), ins_it, (text->lines_size - ins_pos) * sizeof(*ins_it));

	++text->lines_size;

	init_line(ins_it);
}
void gia_text_remove_line_nl(gia_text_t * text, size_t rem_pos) {
	ul_assert(rem_pos < text->lines_size);

	cleanup_line(&text->lines[rem_pos]);

	gia_text_line_t * rem_it = text->lines + rem_pos;

	memmove_s(rem_it, (text->lines_cap - rem_pos) * sizeof(*rem_it), rem_it + 1, (text->lines_size - rem_pos) * sizeof(*rem_it));

	--text->lines_size;
}

void gia_text_from_tus_nl(gia_text_t * text, gia_tus_t * tus) {
	size_t line = 0;

	wchar_t * ch = tus->src, * ch_end = ch + tus->src_size;

	while (true) {
		wchar_t * nl_ch = ch;

		while (nl_ch != ch_end && *nl_ch != L'\n') {
			++nl_ch;
		}

		while (line >= text->lines_size) {
			gia_text_insert_line_nl(text, text->lines_size);
		}

		gia_text_remove_str_nl(text, line, 0, text->lines[line].size);
		gia_text_insert_str_nl(text, line, 0, nl_ch - ch, ch);

		if (nl_ch == ch_end || nl_ch + 1 == ch_end) {
			break;
		}
		else {
			ch = nl_ch + 1;
			++line;
		}
	}
}
void gia_text_to_tus_nl(gia_text_t * text, gia_tus_t * tus) {
	tus->src_size = 0;

	wchar_t nl_ch = L'\n';

	for (gia_text_line_t * line = text->lines, *line_end = line + text->lines_size; line != line_end; ++line) {
		gia_tus_insert_str_nl(tus, tus->src_size, line->size, line->str);
		gia_tus_insert_str_nl(tus, tus->src_size, 1, &nl_ch);
	}
}
