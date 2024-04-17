#pragma once
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
struct gia_text {
	CRITICAL_SECTION lock;

	size_t lines_cap;
	gia_text_line_t * lines;
	size_t lines_size;
};

void gia_text_init(gia_text_t * text);
void gia_text_cleanup(gia_text_t * text);

void gia_text_insert_str_nl(gia_text_t * text, size_t line_pos, size_t ins_pos, size_t str_size, wchar_t * str);
void gia_text_insert_ch_nl(gia_text_t * text, size_t line_pos, size_t ins_pos, wchar_t ch);
void gia_text_remove_str_nl(gia_text_t * text, size_t line_pos, size_t rem_pos_start, size_t rem_pos_end);
void gia_text_remove_ch_nl(gia_text_t * text, size_t line_pos, size_t rem_pos);

void gia_text_insert_line_nl(gia_text_t * text, size_t ins_pos);
void gia_text_remove_line_nl(gia_text_t * text, size_t rem_pos);
