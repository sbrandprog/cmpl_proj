#include "pch.h"
#include "wa_lib/wa_accel.h"
#include "wa_lib/wa_ctx.h"
#include "wa_lib/wa_wnd.h"
#include "wa_lib/wa_ctl.h"
#include "gia_tus.h"
#include "gia_repo.h"
#include "gia_bs.h"
#include "gia_edit.h"

#define TEXT_LINE_INIT_STR_SIZE 64

#define TAB_SIZE 4
#define VIS_SPACE 3
#define VIS_WHEEL_MUL 2

#define SEL_SCROLL_TIMER_ID 1
#define SEL_SCROLL_TIMER_ELPS 10

#define GKS_PRS_CHECK 0x8000

typedef enum gia_edit_cmd_type {
	CmdExit,
	CmdRunExe,
	CmdBuildExe,
	CmdClearCmdWin,
	CmdSaveTus,
	CmdCutSel,
	CmdCopySel,
	CmdPasteData,
	CmdSelectAll,
	Cmd_Count
} cmd_type_t;

typedef struct gia_edit_style {
	wa_style_col_t cols[GiaEditCol_Count];
	wa_style_font_t font;
	float err_line_size;
} style_t;

typedef struct gia_edit_ch_pos {
	size_t line;
	size_t ch;
} text_ch_pos_t;
typedef struct gia_edit_text_line {
	size_t cap;
	wchar_t * str;
	size_t size;
} text_line_t;
typedef enum gia_edit_text_actn_type {
	TextActnInsStr,
	TextActnRemStr,
	TextActn_Count
} text_actn_type_t;
typedef struct gia_edit_text_actn {
	text_actn_type_t type;
	text_ch_pos_t pos;
	union {
		struct {
			size_t str_size;
			wchar_t * str;
			text_ch_pos_t pos_end_out;
		} ins_str;
		struct {
			text_ch_pos_t pos_end;
		} rem_str;
		gia_tus_t * tus;
	};
} text_actn_t;
typedef struct gia_edit_text {
	CRITICAL_SECTION lock;

	size_t lines_size;
	text_line_t * lines;
	size_t lines_cap;
} text_t;

typedef struct gia_edit_data {
	HWND hw;
	wa_ctx_t * ctx;
	wa_ctl_data_t ctl_data;
	style_t style;

	const wchar_t * exe_name;
	gia_repo_t * repo;
	gia_tus_t * tus;

	text_t text;

	size_t vis_line;
	size_t vis_col;

	size_t caret_line;
	size_t caret_col;

	bool is_redraw_req;
	bool is_caret_redraw_req;
	bool is_cpt;
	bool is_fcs;
	HWND prev_fcs_hw;

	int wheel_x;
	int wheel_y;

	bool is_sel;
	size_t sel_start_line;
	size_t sel_start_col;

	PTP_WORK run_exe_work;
	PTP_WORK build_work;
} wnd_data_t;


static const ACCEL accel_table[] = {
	{ .fVirt = FVIRTKEY, .key = VK_ESCAPE, .cmd = CmdExit },
	{ .fVirt = FVIRTKEY, .key = VK_F5, .cmd = CmdRunExe },
	{ .fVirt = FVIRTKEY, .key = VK_F6, .cmd = CmdBuildExe },
	{ .fVirt = FVIRTKEY, .key = VK_F7, .cmd = CmdClearCmdWin },
	{ .fVirt = FCONTROL | FVIRTKEY, .key = 'S', .cmd = CmdSaveTus },
	{ .fVirt = FCONTROL | FVIRTKEY, .key = 'X', .cmd = CmdCutSel },
	{ .fVirt = FCONTROL | FVIRTKEY, .key = 'C', .cmd = CmdCopySel },
	{ .fVirt = FCONTROL | FVIRTKEY, .key = 'V', .cmd = CmdPasteData },
	{ .fVirt = FCONTROL | FVIRTKEY, .key = 'A', .cmd = CmdSelectAll }
};


static bool init_style(style_t * style, gia_edit_style_desc_t * desc) {
	for (gia_edit_col_type_t col = 0; col < GiaEditCol_Count; ++col) {
		if (!wa_style_col_set(&style->cols[col], desc->cols[col])) {
			return false;
		}
	}

	if (!wa_style_font_set(&style->font, &desc->font)) {
		return false;
	}

	style->err_line_size = desc->err_line_size;

	return true;
}
static void cleanup_style(style_t * style) {
	for (gia_edit_col_type_t col = 0; col < GiaEditCol_Count; ++col) {
		wa_style_col_cleanup(&style->cols[col]);
	}

	wa_style_font_cleanup(&style->font);

	memset(style, 0, sizeof(*style));
}


static void init_line(text_line_t * line) {
	*line = (text_line_t){ 0 };

	line->str = malloc(TEXT_LINE_INIT_STR_SIZE * sizeof(*line->str));

	ul_assert(line->str != NULL);

	line->cap = TEXT_LINE_INIT_STR_SIZE;
}
static void cleanup_line(text_line_t * line) {
	free(line->str);

	memset(line, 0, sizeof(*line));
}

static void insert_line_str(text_line_t * line, size_t pos, size_t str_size, wchar_t * str) {
	ul_assert(pos <= line->size);

	if (line->size + str_size > line->cap) {
		ul_arr_grow(&line->cap, &line->str, sizeof(*line->str), line->size + str_size - line->cap);
	}

	wmemmove_s(line->str + pos + str_size, line->cap - pos, line->str + pos, line->size - pos);
	wmemcpy_s(line->str + pos, line->cap - pos, str, str_size);

	line->size += str_size;
}
static void remove_line_str(text_line_t * line, size_t pos_start, size_t pos_end) {
	ul_assert(pos_start <= pos_end && pos_end <= line->size);

	wmemmove_s(line->str + pos_start, line->cap - pos_start, line->str + pos_end, line->size - pos_end);

	line->size -= pos_end - pos_start;
}


static void text_init(text_t * text, ul_es_ctx_t * es_ctx) {
	*text = (text_t){ 0 };

	ul_arr_grow(&text->lines_cap, &text->lines, sizeof(*text->lines), 1);

	init_line(&text->lines[text->lines_size++]);

	InitializeCriticalSection(&text->lock);
}
static void text_cleanup(text_t * text) {
	DeleteCriticalSection(&text->lock);

	for (text_line_t * line = text->lines, *line_end = line + text->lines_size; line != line_end; ++line) {
		cleanup_line(line);
	}

	free(text->lines);

	memset(text, 0, sizeof(*text));
}

static void insert_text_line_nl(text_t * text, size_t ins_pos) {
	if (text->lines_size + 1 > text->lines_cap) {
		ul_arr_grow(&text->lines_cap, &text->lines, sizeof(*text->lines), 1);
	}

	memmove_s(text->lines + ins_pos + 1, (text->lines_cap - ins_pos) * sizeof(*text->lines), text->lines + ins_pos, (text->lines_size - ins_pos) * sizeof(*text->lines));

	++text->lines_size;

	init_line(&text->lines[ins_pos]);
}

static void process_text_actn_ins_str_nl(text_t * text, text_actn_t * actn) {
	const size_t line_pos = actn->pos.line, ins_pos = actn->pos.ch, str_size = actn->ins_str.str_size;
	wchar_t * const str = actn->ins_str.str;

	ul_assert(line_pos < text->lines_size);

	size_t line_i = line_pos, line_i_ch_ins = ins_pos;
	wchar_t * cur = str, * cur_end = cur + str_size;

	while (true) {
		wchar_t * cur_nl = cur;

		while (cur_nl != cur_end && *cur_nl != L'\r' && *cur_nl != L'\n') {
			++cur_nl;
		}

		size_t cur_size = cur_nl - cur;

		{
			text_line_t * line = &text->lines[line_i];

			line_i_ch_ins = min(line_i_ch_ins, line->size);

			insert_line_str(line, line_i_ch_ins, cur_size, cur);

			actn->ins_str.pos_end_out = (text_ch_pos_t){ .line = line_i, .ch = line_i_ch_ins + cur_size };
		}

		if (cur_nl == cur_end) {
			break;
		}

		insert_text_line_nl(text, line_i + 1);

		{
			text_line_t * line = &text->lines[line_i];
			text_line_t * next_line = &text->lines[line_i + 1];

			insert_line_str(next_line, 0, line->size - cur_size - line_i_ch_ins, line->str + cur_size + line_i_ch_ins);
			remove_line_str(line, cur_size + line_i_ch_ins, line->size);

			++line_i;
			line_i_ch_ins = 0;

			actn->ins_str.pos_end_out = (text_ch_pos_t){ .line = line_i, .ch = line_i_ch_ins };
		}

		bool is_cr = *cur_nl == L'\r';

		if (++cur_nl == cur_end) {
			break;
		}

		if (is_cr && *cur_nl == L'\n') {
			++cur_nl;

			if (cur_nl == cur_end) {
				break;
			}
		}

		cur = cur_nl;
	}
}
static void process_text_actn_rem_str_nl(text_t * text, text_actn_t * actn) {
	const size_t start_line_pos = actn->pos.line, start_ch_pos = actn->pos.ch, end_line_pos = actn->rem_str.pos_end.line, end_ch_pos = actn->rem_str.pos_end.ch;

	ul_assert(start_line_pos < end_line_pos || start_line_pos == end_line_pos && start_ch_pos <= end_ch_pos);

	if (start_line_pos >= text->lines_size) {
		return;
	}

	if (start_line_pos == end_line_pos) {
		text_line_t * line = &text->lines[start_line_pos];

		remove_line_str(line, min(start_ch_pos, line->size), min(end_ch_pos, line->size));
	}
	else {
		text_line_t * line = &text->lines[start_line_pos];

		remove_line_str(line, min(start_ch_pos, line->size), line->size);

		if (end_line_pos < text->lines_size) {
			text_line_t * last_line = &text->lines[end_line_pos];

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
static void process_text_actn_read_tus_nl(text_t * text, text_actn_t * actn) {
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

		text_line_t * line = &text->lines[line_i];

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
static void process_text_actn_nl(text_t * text, text_actn_t * actn) {
	typedef void do_actn_proc_t(text_t * text, text_actn_t * actn);

	static do_actn_proc_t * const procs[TextActn_Count] = {
		[TextActnInsStr] = process_text_actn_ins_str_nl,
		[TextActnRemStr] = process_text_actn_rem_str_nl
	};

	do_actn_proc_t * proc = procs[actn->type];

	ul_assert(proc != NULL);

	proc(text, actn);
}


static size_t get_ch_col_size(wnd_data_t * data, size_t line_col, wchar_t ch) {
	if (ch == L'\t') {
		return TAB_SIZE - line_col % TAB_SIZE;
	}

	return 1;
}
static size_t convert_line_ch_to_col(wnd_data_t * data, text_line_t * line, size_t line_ch) {
	ul_assert(line_ch <= line->size);

	size_t line_col = 0;

	for (wchar_t * ch = line->str, *ch_end = ch + line_ch; ch != ch_end; ++ch) {
		line_col += get_ch_col_size(data, line_col, *ch);
	}

	return line_col;
}
static size_t convert_line_col_to_ch(wnd_data_t * data, text_line_t * line, size_t line_col) {
	size_t line_ch = 0, line_col_calc = 0;

	for (; line_ch < line->size; ++line_ch) {
		if (line_col_calc >= line_col) {
			return line_ch;
		}

		line_col_calc += get_ch_col_size(data, line_col_calc, line->str[line_ch]);
	}

	return line_ch;
}

static text_ch_pos_t get_ch_pos(wnd_data_t * data, size_t line, size_t col) {
	ul_assert(data->text.lines_size > 0);

	line = min(line, data->text.lines_size - 1);

	size_t ch = convert_line_col_to_ch(data, &data->text.lines[line], col);

	return (text_ch_pos_t) {
		.line = line, .ch = ch
	};
}
static text_ch_pos_t get_caret_ch_pos(wnd_data_t * data) {
	return get_ch_pos(data, data->caret_line, data->caret_col);
}

static void get_sel_pos(wnd_data_t * data, text_ch_pos_t * out_start, text_ch_pos_t * out_end) {
	text_ch_pos_t sel_pos0 = get_ch_pos(data, data->sel_start_line, data->sel_start_col),
		sel_pos1 = get_caret_ch_pos(data);

	if (sel_pos1.line < sel_pos0.line
		|| sel_pos1.line == sel_pos0.line && sel_pos1.ch < sel_pos0.ch) {
		*out_start = sel_pos1;
		*out_end = sel_pos0;
	}
	else {
		*out_start = sel_pos0;
		*out_end = sel_pos1;
	}
}


static void redraw_caret_nl(wnd_data_t * data) {
	if (!data->is_fcs) {
		return;
	}

	RECT rect;
	
	GetClientRect(data->hw, &rect);

	size_t line_h = data->style.font.f_h, ch_w = data->style.font.f_w;

	size_t vis_line_max = data->vis_line + rect.bottom / line_h,
		vis_col_max = data->vis_col + rect.right / ch_w;

	text_ch_pos_t pos = get_caret_ch_pos(data);

	size_t col = convert_line_ch_to_col(data, &data->text.lines[data->caret_line], pos.ch);

	if (data->vis_line <= pos.line && pos.line <= vis_line_max
		&& data->vis_col <= col && col <= vis_col_max) {

		int caret_w = 1, caret_h = (int)line_h;

		int x = (int)((col - data->vis_col) * ch_w);
		int y = (int)((pos.line - data->vis_line) * line_h);
		
		caret_w -= max(0, x + caret_w - rect.right);
		caret_h -= max(0, y + caret_h - rect.bottom);

		CreateCaret(data->hw, NULL, caret_w, caret_h);

		SetCaretPos(x, y);

		ShowCaret(data->hw);
	}
	else {
		DestroyCaret();
	}
}
static void redraw_caret(wnd_data_t * data) {
	EnterCriticalSection(&data->text.lock);

	redraw_caret_nl(data);

	LeaveCriticalSection(&data->text.lock);
}
static size_t clamp_line_ch_to_vis_col(wnd_data_t * data, text_line_t * line, size_t line_ch) {
	size_t line_col = convert_line_ch_to_col(data, line, line_ch);
	return line_col - min(line_col, data->vis_col);
}
static void draw_line(wnd_data_t * data, HDC hdc, size_t line_pos, size_t line_max_w) {
	text_line_t * line = &data->text.lines[line_pos];

	int line_h = (int)((line_pos - data->vis_line) * data->style.font.f_h);
	size_t line_col_max = line_max_w + data->vis_col;

	for (size_t line_ch = 0, line_col = 0; line_ch < line->size && line_col < line_col_max; ) {
		wchar_t ch = line->str[line_ch];

		if (ch == L'\t') {
			line_col += TAB_SIZE - line_col % TAB_SIZE;
			++line_ch;
		}
		else {
			size_t batch_size = 1;

			while (line_ch + batch_size < line->size
				&& line->str[line_ch + batch_size] != L'\t'
				&& line_col + batch_size < line_col_max) {
				++batch_size;
			}

			SetTextColor(hdc, data->style.cols[GiaEditColPlain].cr);

			if (line_col >= data->vis_col) {
				TextOutW(hdc, (int)((line_col - data->vis_col) * data->style.font.f_w), line_h, line->str + line_ch, (int)batch_size);
			}
			else if (line_col + batch_size >= data->vis_col) {
				size_t offset = data->vis_col - line_col;
				TextOutW(hdc, 0, line_h, line->str + line_ch + offset, (int)(batch_size - offset));
			}

			line_col += batch_size;
			line_ch += batch_size;
		}
	}
}
static void redraw_lines_nl(wnd_data_t * data, HDC hdc, RECT * rect) {
	FillRect(hdc, rect, data->ctx->style.cols[WaStyleColBg].hb);

	size_t line_h = data->style.font.f_h, ch_w = data->style.font.f_w;

	size_t max_w = ul_align_to((size_t)rect->right, ch_w),
		max_h = ul_align_to((size_t)rect->bottom, line_h);

	size_t line_i_max = min(data->text.lines_size, data->vis_line + max_h);

	SelectFont(hdc, data->style.font.hf);
	SetBkMode(hdc, TRANSPARENT);

	{
		text_ch_pos_t sel_start, sel_end;
		get_sel_pos(data, &sel_start, &sel_end);

		RECT line_rect = { .top = 0, .bottom = (LONG)line_h };

		for (size_t line_i = data->vis_line; line_i < line_i_max; ++line_i) {
			text_line_t * line = &data->text.lines[line_i];

			if (data->is_sel && line_i >= sel_start.line && line_i <= sel_end.line) {
				RECT sel_rect = {
					.left = (LONG)(clamp_line_ch_to_vis_col(data, line, line_i == sel_start.line ? sel_start.ch : 0) * ch_w),
					.top = line_rect.top,
					.right = (LONG)(clamp_line_ch_to_vis_col(data, line, line_i == sel_end.line ? sel_end.ch : line->size) * ch_w),
					.bottom = line_rect.bottom
				};

				if (line_i != sel_end.line) {
					sel_rect.right += (LONG)ch_w;
				}

				FillRect(hdc, &sel_rect, data->style.cols[GiaEditColSel].hb);
			}

			draw_line(data, hdc, line_i, max_w);

			line_rect.top += (LONG)line_h;
			line_rect.bottom += (LONG)line_h;
		}
	}
}
static void redraw_lines(void * user_data, HDC hdc, RECT * rect) {
	wnd_data_t * data = user_data;

	EnterCriticalSection(&data->text.lock);

	redraw_lines_nl(data, hdc, rect);

	LeaveCriticalSection(&data->text.lock);
}


static void set_vis_pos(wnd_data_t * data, size_t vis_line, size_t vis_col) {
	if (data->vis_line != vis_line
		|| data->vis_col != vis_col) {
		data->vis_line = vis_line;
		data->vis_col = vis_col;

		data->is_redraw_req = true;
		data->is_caret_redraw_req = true;
	}
}

static void set_caret_pos_col(wnd_data_t * data, size_t caret_line, size_t caret_col) {
	if (data->caret_line != caret_line
		|| data->caret_col != caret_col) {
		data->caret_line = caret_line;
		data->caret_col = caret_col;

		data->is_caret_redraw_req = true;
	}

	if (data->is_sel) {
		data->is_redraw_req = true;
	}
}
static void set_caret_pos_ch(wnd_data_t * data, size_t caret_line, size_t caret_ch) {
	ul_assert(data->text.lines_size > 0);

	caret_line = min(caret_line, data->text.lines_size - 1);

	caret_ch = min(caret_ch, data->text.lines[caret_line].size);

	size_t caret_col = convert_line_ch_to_col(data, &data->text.lines[caret_line], caret_ch);

	set_caret_pos_col(data, caret_line, caret_col);
}
static size_t calc_vis_val(wnd_data_t * data, size_t caret_val, size_t vis_val, size_t vis_max) {
	if (caret_val <= vis_val + VIS_SPACE) {
		vis_val = max(caret_val, VIS_SPACE) - VIS_SPACE;
	}
	else if (caret_val + VIS_SPACE >= vis_val + vis_max) {
		vis_val = caret_val - max(vis_max, VIS_SPACE) + VIS_SPACE;
	}

	return vis_val;
}
static void update_vis_to_caret(wnd_data_t * data) {
	size_t vis_line = data->vis_line, vis_col = data->vis_col;

	RECT rect;
	GetClientRect(data->hw, &rect);

	size_t max_w = (size_t)rect.right / data->style.font.f_w;
	size_t max_h = (size_t)rect.bottom / data->style.font.f_h;

	vis_line = calc_vis_val(data, data->caret_line, vis_line, max_h);
	vis_col = calc_vis_val(data, data->caret_col, vis_col, max_w);

	set_vis_pos(data, vis_line, vis_col);
}


static void insert_text_str_nl(wnd_data_t * data, size_t line_pos, size_t ins_pos, size_t str_size, wchar_t * str, bool set_caret) {
	ul_assert(line_pos < data->text.lines_size && ins_pos <= data->text.lines[line_pos].size);
	
	text_actn_t actn = { .type = TextActnInsStr, .pos = { .line = line_pos, .ch = ins_pos }, .ins_str = { .str_size = str_size, .str = str } };

	process_text_actn_nl(&data->text, &actn);

	data->is_redraw_req = true;

	if (set_caret) {
		set_caret_pos_ch(data, actn.ins_str.pos_end_out.line, actn.ins_str.pos_end_out.ch);
	}
}
static void insert_text_ch_nl(wnd_data_t * data, size_t line_pos, size_t ins_pos, wchar_t ch, bool set_caret) {
	insert_text_str_nl(data, line_pos, ins_pos, 1, &ch, set_caret);
}
static void remove_text_str_nl(wnd_data_t * data, size_t start_line_pos, size_t start_ch_pos, size_t end_line_pos, size_t end_ch_pos, bool set_caret) {
	ul_assert(data->text.lines_size > 0);

	text_actn_t actn = { .type = TextActnRemStr, .pos = { .line = start_line_pos, .ch = start_ch_pos }, .rem_str = { .pos_end = { .line = end_line_pos, .ch = end_ch_pos } } };

	process_text_actn_nl(&data->text, &actn);

	data->is_redraw_req = true;

	if (set_caret) {
		size_t caret_line = min(start_line_pos, data->text.lines_size - 1);

		text_line_t * line = &data->text.lines[caret_line];

		size_t caret_ch = caret_line == start_line_pos ? min(start_ch_pos, line->size) : line->size;

		set_caret_pos_ch(data, caret_line, caret_ch);
	}
}
static void remove_text_ch_nl(wnd_data_t * data, size_t line_pos, size_t rem_pos, bool set_caret) {
	remove_text_str_nl(data, line_pos, rem_pos, line_pos, rem_pos + 1, set_caret);
}

static void read_text_from_tus(wnd_data_t * data) {
	EnterCriticalSection(&data->text.lock);
	EnterCriticalSection(&data->tus->lock);

	remove_text_str_nl(data, 0, 0, data->text.lines_size, SIZE_MAX, true);
	insert_text_str_nl(data, 0, 0, data->tus->src_size, data->tus->src, false);

	LeaveCriticalSection(&data->text.lock);
	LeaveCriticalSection(&data->tus->lock);
}
static void write_text_to_tus(wnd_data_t * data) {
	EnterCriticalSection(&data->text.lock);
	EnterCriticalSection(&data->tus->lock);

	data->tus->src_size = 0;

	wchar_t nl_ch = L'\n';

	for (text_line_t * line = data->text.lines, *line_end = line + data->text.lines_size; line != line_end; ++line) {
		gia_tus_insert_str_nl(data->tus, data->tus->src_size, line->size, line->str);
		gia_tus_insert_str_nl(data->tus, data->tus->src_size, 1, &nl_ch);
	}

	LeaveCriticalSection(&data->text.lock);
	LeaveCriticalSection(&data->tus->lock);
}

static void remove_text_sel_nl(wnd_data_t * data) {
	text_ch_pos_t sel_start, sel_end;

	get_sel_pos(data, &sel_start, &sel_end);

	remove_text_str_nl(data, sel_start.line, sel_start.ch, sel_end.line, sel_end.ch, true);

	data->is_sel = false;
}


static void process_caret_keyd_wp_nl_back(wnd_data_t * data) {
	text_ch_pos_t pos = get_caret_ch_pos(data);

	if (data->is_sel) {
		remove_text_sel_nl(data);
	}
	else {
		if (pos.ch > 0) {
			remove_text_ch_nl(data, pos.line, pos.ch - 1, true);
		}
		else if (pos.line > 0) {
			size_t new_caret_ch = data->text.lines[pos.line - 1].size;

			remove_text_str_nl(data, pos.line - 1, new_caret_ch, pos.line, 0, true);
		}
	}
}
static void process_caret_keyd_wp_nl_ret(wnd_data_t * data) {
	if (data->is_sel) {
		remove_text_sel_nl(data);
	}

	text_ch_pos_t pos = get_caret_ch_pos(data);

	insert_text_ch_nl(data, pos.line, pos.ch, L'\n', true);
}
static void process_caret_keyd_wp_nl_cret(wnd_data_t * data) {
	if (data->is_sel) {
		remove_text_sel_nl(data);
	}

	text_ch_pos_t pos = get_caret_ch_pos(data);

	insert_text_ch_nl(data, pos.line, 0, L'\n', false);

	set_caret_pos_ch(data, pos.line, pos.ch);
}
static void process_caret_keyd_wp_nl_nav(wnd_data_t * data, WPARAM wp) {
	ul_assert(data->text.lines_size > 0);

	if (data->is_sel) {
		text_ch_pos_t sel_start, sel_end;

		get_sel_pos(data, &sel_start, &sel_end);

		switch (wp) {
			case VK_END:
			case VK_RIGHT:
			case VK_DOWN:
				set_caret_pos_ch(data, sel_end.line, sel_end.ch);
				break;
			case VK_HOME:
			case VK_LEFT:
			case VK_UP:
				set_caret_pos_ch(data, sel_start.line, sel_start.ch);
				break;
			default:
				ul_assert_unreachable();
		}

		data->is_sel = false;
	}
	else {
		text_ch_pos_t caret_pos = get_caret_ch_pos(data);

		size_t caret_line = caret_pos.line, caret_col = data->caret_col, caret_ch = caret_pos.ch;

		switch (wp) {
			case VK_END:
				set_caret_pos_ch(data, caret_line, data->text.lines[caret_line].size);
				break;
			case VK_HOME:
				set_caret_pos_ch(data, caret_line, 0);
				break;
			case VK_LEFT:
				if (caret_ch > 0) {
					--caret_ch;
				}
				else if (caret_line > 0) {
					--caret_line;
					caret_ch = data->text.lines[caret_line].size;
				}

				set_caret_pos_ch(data, caret_line, caret_ch);
				break;
			case VK_UP:
				if (caret_line > 0) {
					--caret_line;
				}
				else if (caret_col > 0) {
					caret_col = 0;
				}

				set_caret_pos_col(data, caret_line, caret_col);
				break;
			case VK_RIGHT:
				if (caret_ch < data->text.lines[caret_line].size) {
					++caret_ch;
				}
				else if (caret_line < data->text.lines_size - 1) {
					++caret_line;
					caret_ch = 0;
				}

				set_caret_pos_ch(data, caret_line, caret_ch);
				break;
			case VK_DOWN:
				if (caret_line < data->text.lines_size - 1) {
					++caret_line;
				}
				else if (caret_col < data->text.lines[caret_line].size) {
					caret_col = data->text.lines[caret_line].size;
				}

				set_caret_pos_col(data, caret_line, caret_col);
				break;
			default:
				ul_assert_unreachable();
		}
	}
}
static void process_caret_keyd_wp_nl_del(wnd_data_t * data) {
	if (data->is_sel) {
		remove_text_sel_nl(data);
	}
	else {
		text_ch_pos_t pos = get_caret_ch_pos(data);

		text_line_t * line = &data->text.lines[pos.line];

		if (pos.ch < line->size) {
			remove_text_ch_nl(data, pos.line, pos.ch, false);
		}
		else if (pos.line + 1 < data->text.lines_size) {
			remove_text_str_nl(data, pos.line, SIZE_MAX, pos.line + 1, 0, false);
		}
	}
}
static void process_caret_keyd_wp_nl_vis(wnd_data_t * data) {
	update_vis_to_caret(data);
}
static void process_caret_keyd_wp_nl(wnd_data_t * data, WPARAM wp) {
	bool vis_update = true;

	switch (wp) {
		case VK_BACK:
			process_caret_keyd_wp_nl_back(data);
			break;
		case VK_RETURN:
			if ((GetKeyState(VK_CONTROL) & GKS_PRS_CHECK) != 0) {
				process_caret_keyd_wp_nl_cret(data);
			}
			else {
				process_caret_keyd_wp_nl_ret(data);
			}
			break;
		case VK_END:
		case VK_HOME:
		case VK_LEFT:
		case VK_UP:
		case VK_RIGHT:
		case VK_DOWN:
			process_caret_keyd_wp_nl_nav(data, wp);
			break;
		case VK_DELETE:
			process_caret_keyd_wp_nl_del(data);
			break;
		default:
			vis_update = false;
			break;
	}

	if (vis_update) {
		process_caret_keyd_wp_nl_vis(data);
	}
}
static void process_caret_keyd_wp(wnd_data_t * data, WPARAM wp) {
	EnterCriticalSection(&data->text.lock);

	process_caret_keyd_wp_nl(data, wp);

	LeaveCriticalSection(&data->text.lock);
}

static void process_caret_ch_wp_nl(wnd_data_t * data, WPARAM wp) {
	wchar_t ch = (wchar_t)wp;

	if (iswprint(ch) || ch == L'\t') {
		if (data->is_sel) {
			remove_text_sel_nl(data);
		}

		text_ch_pos_t pos = get_caret_ch_pos(data);

		insert_text_ch_nl(data, pos.line, pos.ch, ch, true);

		update_vis_to_caret(data);
	}
}
static void process_caret_ch_wp(wnd_data_t * data, WPARAM wp) {
	EnterCriticalSection(&data->text.lock);

	process_caret_ch_wp_nl(data, wp);

	LeaveCriticalSection(&data->text.lock);
}

static void process_caret_cur_pos_nl(wnd_data_t * data, int x, int y) {
	x = max(x, 0), y = max(y, 0);

	x += (int)(data->style.font.f_w / 2);

	ul_assert(data->text.lines_size > 0);

	text_ch_pos_t pos = get_ch_pos(data,
		data->vis_line + (size_t)y / data->style.font.f_h,
		data->vis_col + (size_t)x / data->style.font.f_w);

	set_caret_pos_ch(data, pos.line, pos.ch);
}
static void process_caret_cur_lp(wnd_data_t * data, LPARAM lp) {
	EnterCriticalSection(&data->text.lock);

	process_caret_cur_pos_nl(data, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));

	LeaveCriticalSection(&data->text.lock);
}


static size_t process_vis_wheel_c_wp(wnd_data_t * data, int * wheel_c, size_t vis_val, size_t vis_val_max) {
	int scroll_amount = *wheel_c / WHEEL_DELTA;
	*wheel_c %= WHEEL_DELTA;

	if (scroll_amount >= 0) {
		vis_val_max = max(1, vis_val_max) - 1;
		if (vis_val < vis_val_max) {
			vis_val = min(vis_val_max, vis_val + (size_t)scroll_amount);
		}
	}
	else {
		vis_val -= min(vis_val, (size_t)-scroll_amount);
	}

	return vis_val;
}
static void process_vis_wheel_y_nl(wnd_data_t * data, int wheel_delta) {
	data->wheel_y += wheel_delta * -VIS_WHEEL_MUL;

	size_t vis_line = process_vis_wheel_c_wp(data, &data->wheel_y, data->vis_line, data->text.lines_size);

	set_vis_pos(data, vis_line, data->vis_col);
}
static void process_vis_wheel_y(wnd_data_t * data, int wheel_delta) {
	EnterCriticalSection(&data->text.lock);

	process_vis_wheel_y_nl(data, wheel_delta);

	LeaveCriticalSection(&data->text.lock);
}
static void process_vis_wheel_x_nl(wnd_data_t * data, int wheel_delta) {
	data->wheel_x += wheel_delta * VIS_WHEEL_MUL;

	text_ch_pos_t pos = get_caret_ch_pos(data);

	text_line_t * line = &data->text.lines[pos.line];

	size_t vis_col = process_vis_wheel_c_wp(data, &data->wheel_x, data->vis_col, convert_line_ch_to_col(data, line, line->size));

	set_vis_pos(data, data->vis_line, vis_col);
}
static void process_vis_wheel_x(wnd_data_t * data, int wheel_delta) {
	EnterCriticalSection(&data->text.lock);

	process_vis_wheel_x_nl(data, wheel_delta);

	LeaveCriticalSection(&data->text.lock);
}


static void start_sel(wnd_data_t * data) {
	data->is_sel = true;
	data->sel_start_line = data->caret_line;
	data->sel_start_col = data->caret_col;

	SetTimer(data->hw, SEL_SCROLL_TIMER_ID, SEL_SCROLL_TIMER_ELPS, NULL);
}
static void end_sel(wnd_data_t * data) {
	if (data->is_sel
		&& data->sel_start_line == data->caret_line
		&& data->sel_start_col == data->caret_col) {
		data->is_sel = false;
	}

	KillTimer(data->hw, SEL_SCROLL_TIMER_ID);
}
static void process_sel_timer_nl(wnd_data_t * data) {
	POINT pos;

	GetCursorPos(&pos);

	ScreenToClient(data->hw, &pos);

	RECT rect;

	GetClientRect(data->hw, &rect);

	if (pos.x < 0) {
		process_vis_wheel_x_nl(data, pos.x);
	}
	else if (pos.x > rect.right) {
		process_vis_wheel_x_nl(data, pos.x - rect.right);
	}

	if (pos.y < 0) {
		process_vis_wheel_y_nl(data, -pos.y);
	}
	else if (pos.y > rect.bottom) {
		process_vis_wheel_y_nl(data, rect.bottom - pos.y);
	}

	process_caret_cur_pos_nl(data, pos.x, pos.y);
}
static void process_sel_timer(wnd_data_t * data) {
	EnterCriticalSection(&data->text.lock);

	process_sel_timer_nl(data);

	LeaveCriticalSection(&data->text.lock);
}


static void copy_text_nl(wnd_data_t * data) {
	if (!data->is_sel) {
		return;
	}

	text_ch_pos_t sel_start, sel_end;

	get_sel_pos(data, &sel_start, &sel_end);

	size_t str_size = 0;

	for (size_t line_i = sel_start.line; line_i <= sel_end.line; ++line_i) {
		text_line_t * line = &data->text.lines[line_i];

		size_t start_ch = line_i == sel_start.line ? sel_start.ch : 0;
		size_t end_ch = line_i == sel_end.line ? sel_end.ch : line->size;

		str_size += end_ch - start_ch;

		if (line_i != sel_end.line) {
			str_size += 2;
		}
	}

	++str_size;

	HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, str_size * sizeof(wchar_t));

	if (hg != NULL) {
		wchar_t * hg_str = GlobalLock(hg);

		if (hg_str != NULL) {
			wchar_t * hg_str_end = hg_str + str_size;

			wchar_t * cur = hg_str;

			static const wchar_t new_line_str[2] = { L'\r', L'\n' };

			for (size_t line_i = sel_start.line; line_i <= sel_end.line; ++line_i) {
				text_line_t * line = &data->text.lines[line_i];

				size_t start_ch = line_i == sel_start.line ? sel_start.ch : 0;
				size_t end_ch = line_i == sel_end.line ? sel_end.ch : line->size;

				wmemcpy_s(cur, hg_str_end - cur, line->str + start_ch, end_ch - start_ch);
				cur += end_ch - start_ch;

				if (line_i != sel_end.line) {
					wmemcpy_s(cur, hg_str_end - cur, new_line_str, _countof(new_line_str));
					cur += _countof(new_line_str);
				}
			}

			*cur++ = 0;

			ul_assert(cur == hg_str_end);

			GlobalUnlock(hg);

			EmptyClipboard();

			if (SetClipboardData(CF_UNICODETEXT, hg) == NULL) {
				GlobalFree(hg);
			}
		}
		else {
			GlobalFree(hg);
		}
	}
}
static void paste_text_nl(wnd_data_t * data) {
	HGLOBAL hg = GetClipboardData(CF_UNICODETEXT);

	if (hg != NULL) {
		wchar_t * hg_str = GlobalLock(hg);

		if (hg_str != NULL) {
			text_ch_pos_t pos = get_caret_ch_pos(data);

			insert_text_str_nl(data, pos.line, pos.ch, wcslen(hg_str), hg_str, true);

			GlobalUnlock(hg);
		}
	}
}

static void process_cmd_cut_sel_nl(wnd_data_t * data) {
	if (!data->is_sel) {
		return;
	}

	copy_text_nl(data);

	remove_text_sel_nl(data);
}
static void process_cmd_cut_sel(wnd_data_t * data) {
	if (OpenClipboard(data->hw) != 0) {
		EnterCriticalSection(&data->text.lock);

		process_cmd_cut_sel_nl(data);

		LeaveCriticalSection(&data->text.lock);
		CloseClipboard();
	}
}
static void process_cmd_copy_sel(wnd_data_t * data) {
	if (OpenClipboard(data->hw) != 0) {
		EnterCriticalSection(&data->text.lock);

		copy_text_nl(data);

		LeaveCriticalSection(&data->text.lock);
		CloseClipboard();
	}
}
static void process_cmd_paste_data_nl(wnd_data_t * data) {
	if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) {
		return;
	}

	if (data->is_sel) {
		remove_text_sel_nl(data);
	}

	paste_text_nl(data);
}
static void process_cmd_paste_data(wnd_data_t * data) {
	if (OpenClipboard(data->hw) != 0) {
		EnterCriticalSection(&data->text.lock);

		process_cmd_paste_data_nl(data);

		LeaveCriticalSection(&data->text.lock);
		CloseClipboard();
	}
}
static void process_cmd_select_all_nl(wnd_data_t * data) {
	data->is_sel = true;

	data->sel_start_line = 0;
	data->sel_start_col = 0;

	text_ch_pos_t pos = get_ch_pos(data, SIZE_MAX, SIZE_MAX);

	set_caret_pos_ch(data, pos.line, pos.ch);

	update_vis_to_caret(data);
}
static void process_cmd_select_all(wnd_data_t * data) {
	EnterCriticalSection(&data->text.lock);

	process_cmd_select_all_nl(data);

	LeaveCriticalSection(&data->text.lock);
}
static void process_cmd_wp(wnd_data_t * data, WPARAM wp) {
	switch (LOWORD(wp)) {
		case CmdExit:
			if (data->is_fcs) {
				SetFocus(data->prev_fcs_hw);
			}
			break;
		case CmdRunExe:
			if (data->exe_name != NULL) {
				SubmitThreadpoolWork(data->run_exe_work);
			}
			else {
				wprintf(L"no exe_name");
			}
			break;
		case CmdBuildExe:
			if (data->repo != NULL && data->tus != NULL && data->exe_name != NULL) {
				SubmitThreadpoolWork(data->build_work);
			}
			else {
				wprintf(L"no repo || no tus || no exe_name");
			}
			break;
		case CmdClearCmdWin:
			_wsystem(L"cls");
			break;
		case CmdSaveTus:
			if (data->tus != NULL) {
				write_text_to_tus(data);
				wprintf(L"saved text data to tus\n");
			}
			else {
				wprintf(L"no tus\n");
			}
			break;
		case CmdCutSel:
			process_cmd_cut_sel(data);
			break;
		case CmdCopySel:
			process_cmd_copy_sel(data);
			break;
		case CmdPasteData:
			process_cmd_paste_data(data);
			break;
		case CmdSelectAll:
			process_cmd_select_all(data);
			break;
		default:
			ul_assert_unreachable();
	}
}


static bool process_cd_args(wnd_data_t * data, wa_wnd_cd_t * cd) {
	HWND hw = data->hw;

	for (const wchar_t * arg = va_arg(cd->args, const wchar_t *); arg != NULL; arg = va_arg(cd->args, const wchar_t *)) {
		if (wcscmp(arg, gia_edit_prop_style_desc) == 0) {
			gia_edit_style_desc_t desc = va_arg(cd->args, gia_edit_style_desc_t);

			if (!init_style(&data->style, &desc)) {
				return false;
			}
		}
		else if (wcscmp(arg, gia_edit_prop_exe_name) == 0) {
			const wchar_t * exe_name = va_arg(cd->args, const wchar_t *);

			if (exe_name == NULL) {
				return false;
			}

			data->exe_name = exe_name;
		}
		else if (wcscmp(arg, gia_edit_prop_repo) == 0) {
			gia_repo_t * repo = va_arg(cd->args, gia_repo_t *);

			if (repo == NULL) {
				return false;
			}

			data->repo = repo;
		}
		else if (wcscmp(arg, gia_edit_prop_tus) == 0) {
			gia_tus_t * tus = va_arg(cd->args, gia_tus_t *);

			if (tus == NULL) {
				return false;
			}

			data->tus = tus;

			read_text_from_tus(data);
		}
		else {
			return false;
		}
	}

	return true;
}

static VOID run_exe_worker(PTP_CALLBACK_INSTANCE itnc, PVOID user_data, PTP_WORK work) {
	CallbackMayRunLong(itnc);

	wnd_data_t * data = user_data;

	wprintf(L"running exe file\n");

	int ret_code = (int)_wspawnl(_P_WAIT, data->exe_name, data->exe_name, NULL);

	wprintf(L"program exit code: %d 0x%X\n", ret_code, ret_code);
}
static VOID build_prog_worker(PTP_CALLBACK_INSTANCE itnc, PVOID user_data, PTP_WORK work) {
	CallbackMayRunLong(itnc);

	wnd_data_t * data = user_data;

	wprintf(L"awating build lock\n");

	EnterCriticalSection(&data->repo->lock);
	LeaveCriticalSectionWhenCallbackReturns(itnc, &data->repo->lock);

	wprintf(L"build started\n");

	bool res = gia_bs_build_nl(data->repo, data->tus->name, data->exe_name);

	wprintf(L"build status: %s\n", res ? L"success" : L"failure");
}

static LRESULT wnd_proc_data_core(wnd_data_t * data, UINT msg, WPARAM wp, LPARAM lp) {
	HWND hw = data->hw;
	wa_ctx_t * ctx = data->ctx;

	switch (msg) {
		case WM_CREATE:
			if (!process_cd_args(data, wa_wnd_get_cd(lp))) {
				return -1;
			}

			break;
		case WM_SETFOCUS:
			data->is_fcs = true;
			data->prev_fcs_hw = (HWND)wp;

			redraw_caret(data);
			break;
		case WM_KILLFOCUS:
			data->is_fcs = false;
			data->prev_fcs_hw = NULL;

			DestroyCaret();
			break;
		case WM_PAINT:
		{
			PAINTSTRUCT ps;

			HDC hdc = BeginPaint(hw, &ps);

			wa_wnd_paint_buf(hw, hdc, data, redraw_lines);

			EndPaint(hw, &ps);

			return 0;
		}
		case WM_ERASEBKGND:
			return TRUE;
		case WM_KEYDOWN:
			process_caret_keyd_wp(data, wp);
			break;
		case WM_CHAR:
			process_caret_ch_wp(data, wp);
			break;
		case WM_COMMAND:
			process_cmd_wp(data, wp);
			break;
		case WM_TIMER:
			if (wp == SEL_SCROLL_TIMER_ID) {
				process_sel_timer(data);
			}
			break;
		case WM_MOUSEMOVE:
			if (wp & MK_LBUTTON) {
				process_caret_cur_lp(data, lp);
			}
			break;
		case WM_LBUTTONDOWN:
			SetCapture(hw);
			data->is_cpt = true;

			process_caret_cur_lp(data, lp);

			if (!data->is_fcs) {
				SetFocus(hw);
			}

			start_sel(data);
			break;
		case WM_LBUTTONUP:
			if (data->is_cpt) {
				ReleaseCapture();
				data->is_cpt = false;
			}

			process_caret_cur_lp(data, lp);

			end_sel(data);
			break;
		case WM_MOUSEWHEEL:
			process_vis_wheel_y(data, GET_WHEEL_DELTA_WPARAM(wp));
			return 0;
		case WM_MOUSEHWHEEL:
			process_vis_wheel_x(data, GET_WHEEL_DELTA_WPARAM(wp));
			break;
	}

	return DefWindowProcW(hw, msg, wp, lp);
}
static LRESULT wnd_proc_data(wnd_data_t * data, UINT msg, WPARAM wp, LPARAM lp) {
	LRESULT res = wnd_proc_data_core(data, msg, wp, lp);

	if (data->is_redraw_req) {
		InvalidateRect(data->hw, NULL, FALSE);

		data->is_redraw_req = false;
	}

	if (data->is_caret_redraw_req) {
		redraw_caret(data);

		data->is_caret_redraw_req = false;
	}

	return res;
}
static LRESULT wnd_proc(HWND hw, UINT msg, WPARAM wp, LPARAM lp) {
	wnd_data_t * data = wa_wnd_get_fp(hw);

	switch (msg) {
		case WM_NCCREATE:
			if (data == NULL) {
				wa_wnd_cd_t * cd = wa_wnd_get_cd(lp);

				data = malloc(sizeof(*data));

				if (data == NULL) {
					return FALSE;
				}

				*data = (wnd_data_t){ .hw = hw, .ctx = cd->ctx, .ctl_data = { .ctl_ptr = data, .get_w_proc = wa_ctl_get_parent_w, .get_h_proc = wa_ctl_get_parent_h } };

				wa_wnd_set_fp(hw, data);

				if (!wa_ctl_set_data(hw, &data->ctl_data)) {
					return FALSE;
				}

				{
					gia_edit_style_desc_t desc = gia_edit_get_style_desc_dflt(data->ctx);

					if (!init_style(&data->style, &desc)) {
						return FALSE;
					}
				}

				text_init(&data->text, data->ctx->es_ctx);

				data->run_exe_work = CreateThreadpoolWork(run_exe_worker, data, NULL);

				if (data->run_exe_work == NULL) {
					return FALSE;
				}

				data->build_work = CreateThreadpoolWork(build_prog_worker, data, NULL);

				if (data->build_work == NULL) {
					return FALSE;
				}

				if (!wa_accel_load(hw, _countof(accel_table), accel_table)) {
					return FALSE;
				}
			}
			break;
		case WM_NCDESTROY:
			if (data != NULL) {
				wa_accel_unload(hw);
				
				if (data->run_exe_work != NULL) {
					CloseThreadpoolWork(data->run_exe_work);
				}

				if (data->build_work != NULL) {
					WaitForThreadpoolWorkCallbacks(data->build_work, FALSE);

					CloseThreadpoolWork(data->build_work);
				}

				text_cleanup(&data->text);

				cleanup_style(&data->style);

				wa_ctl_remove_data(hw);

				free(data);

				data = NULL;

				wa_wnd_set_fp(hw, data);
			}
			break;
		default:
			if (data != NULL) {
				return wnd_proc_data(data, msg, wp, lp);
			}
			break;
	}

	return DefWindowProcW(hw, msg, wp, lp);
}

WNDCLASSEXW gia_edit_get_wnd_cls_desc() {
	WNDCLASSEXW wnd_cls_desc = {
		.style = CS_VREDRAW | CS_HREDRAW | CS_PARENTDC,
		.lpfnWndProc = wnd_proc,
		.cbWndExtra = sizeof(wnd_data_t *),
		.hCursor = LoadCursorW(NULL, IDC_IBEAM),
		.lpszClassName = GIA_EDIT_WND_CLS_NAME
	};

	return wnd_cls_desc;
}

gia_edit_style_desc_t gia_edit_get_style_desc_dflt(wa_ctx_t * ctx) {
	gia_edit_style_desc_t desc = {
		.cols = {
			[GiaEditColPlain] = RGB(0, 0, 0),
			[GiaEditColSel] = RGB(176, 220, 255)
	},
	.font = ctx->style.fonts[WaStyleFontFxd].lf,
	.err_line_size = 0.125
	};

	desc.font.lfHeight = -16;

	return desc;
}
