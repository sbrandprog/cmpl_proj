#include "pch.h"
#include "gia_text.h"

#define LINE_INIT_STR_SIZE 64

static void init_line(gia_text_line_t * line) {
	*line = (gia_text_line_t){ 0 };

	line->str = malloc(LINE_INIT_STR_SIZE * sizeof(*line->str));

	ul_raise_check_mem_alloc(line->str);

	line->cap = LINE_INIT_STR_SIZE;
}
static void cleanup_line(gia_text_line_t * line) {
	free(line->str);

	memset(line, 0, sizeof(*line));
}


void gia_text_init(gia_text_t * text) {
	*text = (gia_text_t){ 0 };

	InitializeCriticalSection(&text->lock);

	ul_arr_grow(&text->lines_cap, &text->lines, sizeof(*text->lines), 1);

	init_line(&text->lines[text->lines_size++]);
}
void gia_text_cleanup(gia_text_t * text) {
	DeleteCriticalSection(&text->lock);

	for (gia_text_line_t * line = text->lines, *line_end = line + text->lines_size; line != line_end; ++line) {
		cleanup_line(line);
	}

	free(text->lines);

	memset(text, 0, sizeof(*text));
}

void gia_text_insert_ch_nl(gia_text_t * text, size_t line_pos, size_t ins_pos, wchar_t ch) {
	ul_raise_assert(line_pos < text->lines_size);
	
	gia_text_line_t * line = &text->lines[line_pos];

	ul_raise_assert(ins_pos <= line->size);

	if (line->size + 1 > line->cap) {
		ul_arr_grow(&line->cap, &line->str, sizeof(*line->str), 1);
	}

	wmemmove_s(line->str + ins_pos + 1, line->cap - ins_pos, line->str + ins_pos, line->size - ins_pos);
	line->str[ins_pos] = ch;

	++line->size;
}
void gia_text_insert_ch(gia_text_t * text, size_t line_pos, size_t ins_pos, wchar_t ch) {
	EnterCriticalSection(&text->lock);

	__try {
		gia_text_insert_ch_nl(text, line_pos, ins_pos, ch);
	}
	__finally {
		LeaveCriticalSection(&text->lock);
	}
}
void gia_text_remove_ch_nl(gia_text_t * text, size_t line_pos, size_t rem_pos) {
	ul_raise_assert(line_pos < text->lines_size);

	gia_text_line_t * line = &text->lines[line_pos];

	ul_raise_assert(line->size > 0 && rem_pos < line->size);

	wmemmove_s(line->str + rem_pos, line->cap - rem_pos, line->str + rem_pos + 1, line->size - rem_pos);

	--line->size;
}
void gia_text_remove_ch(gia_text_t * text, size_t line_pos, size_t rem_pos) {
	EnterCriticalSection(&text->lock);

	__try {
		gia_text_remove_ch_nl(text, line_pos, rem_pos);
	}
	__finally {
		LeaveCriticalSection(&text->lock);
	}
}

void gia_text_insert_line_nl(gia_text_t * text, size_t ins_pos) {
	ul_raise_assert(ins_pos <= text->lines_size);

	if (text->lines_size + 1 > text->lines_cap) {
		ul_arr_grow(&text->lines_cap, &text->lines, sizeof(*text->lines), 1);
	}

	gia_text_line_t * ins_it = text->lines + ins_pos;

	memmove_s(ins_it + 1, (text->lines_cap - ins_pos) * sizeof(*ins_it), ins_it, (text->lines_size - ins_pos) * sizeof(*ins_it));

	++text->lines_size;

	init_line(ins_it);
}
void gia_text_insert_line(gia_text_t * text, size_t ins_pos) {
	EnterCriticalSection(&text->lock);

	__try {
		gia_text_insert_line_nl(text, ins_pos);
	}
	__finally {
		LeaveCriticalSection(&text->lock);
	}
}
void gia_text_remove_line_nl(gia_text_t * text, size_t rem_pos) {
	ul_raise_assert(rem_pos < text->lines_size);

	cleanup_line(&text->lines[rem_pos]);

	gia_text_line_t * rem_it = text->lines + rem_pos;

	memmove_s(rem_it, (text->lines_cap - rem_pos) * sizeof(*rem_it), rem_it + 1, (text->lines_size - rem_pos) * sizeof(*rem_it));

	--text->lines_size;
}
void gia_text_remove_line(gia_text_t * text, size_t rem_pos) {
	EnterCriticalSection(&text->lock);

	__try {
		gia_text_remove_line_nl(text, rem_pos);
	}
	__finally {
		LeaveCriticalSection(&text->lock);
	}
}
