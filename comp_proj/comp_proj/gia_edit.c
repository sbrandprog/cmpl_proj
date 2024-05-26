#include "pch.h"
#include "wa_lib/wa_ctx.h"
#include "wa_lib/wa_wnd.h"
#include "wa_lib/wa_ctl.h"
#include "gia_tus.h"
#include "gia_repo.h"
#include "gia_bs.h"
#include "gia_text.h"
#include "gia_edit.h"

#define TAB_SIZE 4
#define VIS_SPACE 3
#define VIS_WHEEL_MUL 2

#define SEL_SCROLL_TIMER_ID 1
#define SEL_SCROLL_TIMER_ELPS 10

#define GKS_PRS_CHECK 0x8000

typedef struct gia_edit_style {
	wa_style_col_t cols[GiaEditCol_Count];
	wa_style_font_t font;
	float err_line_size;
} style_t;

typedef struct gia_edit_data {
	HWND hw;
	wa_ctx_t * ctx;
	wa_ctl_data_t ctl_data;
	style_t style;

	const wchar_t * exe_name;
	gia_repo_t * repo;
	gia_tus_t * tus;

	gia_text_t text;

	size_t vis_line;
	size_t vis_col;

	size_t caret_line;
	size_t caret_col;

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

static void load_tus_data(wnd_data_t * data) {
	EnterCriticalSection(&data->text.lock);
	EnterCriticalSection(&data->tus->lock);

	__try {
		gia_text_from_tus_nl(&data->text, data->tus);
	}
	__finally {
		LeaveCriticalSection(&data->text.lock);
		LeaveCriticalSection(&data->tus->lock);
	}
}
static void save_tus_data(wnd_data_t * data) {
	EnterCriticalSection(&data->text.lock);
	EnterCriticalSection(&data->tus->lock);

	__try {
		gia_text_to_tus_nl(&data->text, data->tus);
	}
	__finally {
		LeaveCriticalSection(&data->text.lock);
		LeaveCriticalSection(&data->tus->lock);
	}
}


static size_t get_ch_col_size(wnd_data_t * data, size_t line_col, wchar_t ch) {
	if (ch == L'\t') {
		return TAB_SIZE - line_col % TAB_SIZE;
	}

	return 1;
}
static size_t convert_line_ch_to_col(wnd_data_t * data, gia_text_line_t * line, size_t line_ch) {
	ul_assert(line_ch <= line->size);

	size_t line_col = 0;

	for (wchar_t * ch = line->str, *ch_end = ch + line_ch; ch != ch_end; ++ch) {
		line_col += get_ch_col_size(data, line_col, *ch);
	}

	return line_col;
}
static size_t convert_line_col_to_ch(wnd_data_t * data, gia_text_line_t * line, size_t line_col) {
	size_t line_ch = 0, line_col_calc = 0;

	for (; line_ch < line->size; ++line_ch) {
		if (line_col_calc >= line_col) {
			return line_ch;
		}

		line_col_calc += get_ch_col_size(data, line_col_calc, line->str[line_ch]);
	}

	return line_ch;
}

static gia_text_ch_pos_t get_ch_pos(wnd_data_t * data, size_t line, size_t col) {
	ul_assert(data->text.lines_size > 0);

	line = min(line, data->text.lines_size - 1);

	size_t ch = convert_line_col_to_ch(data, &data->text.lines[line], col);

	return (gia_text_ch_pos_t) {
		.line = line, .ch = ch
	};
}
static gia_text_ch_pos_t get_caret_ch_pos(wnd_data_t * data) {
	return get_ch_pos(data, data->caret_line, data->caret_col);
}

static void get_sel_pos(wnd_data_t * data, gia_text_ch_pos_t * out_start, gia_text_ch_pos_t * out_end) {
	gia_text_ch_pos_t sel_pos0 = get_ch_pos(data, data->sel_start_line, data->sel_start_col),
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

	gia_text_ch_pos_t pos = get_caret_ch_pos(data);

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

	__try {
		redraw_caret_nl(data);
	}
	__finally {
		LeaveCriticalSection(&data->text.lock);
	}
}
static size_t clamp_line_ch_to_vis_col(wnd_data_t * data, gia_text_line_t * line, size_t line_ch) {
	size_t line_col = convert_line_ch_to_col(data, line, line_ch);
	return line_col - min(line_col, data->vis_col);
}
static void draw_line(wnd_data_t * data, HDC hdc, size_t line_pos, size_t line_max_w) {
	gia_text_line_t * line = &data->text.lines[line_pos];

	int line_h = (int)((line_pos - data->vis_line) * data->style.font.f_h);
	size_t line_col_max = line_max_w + data->vis_col;

	for (size_t line_ch = 0, line_col = 0, line_tok = 0; line_ch < line->size && line_col < line_col_max; ) {
		wchar_t ch = line->str[line_ch];

		if (ch == L'\t') {
			line_col += TAB_SIZE - line_col % TAB_SIZE;
			++line_ch;
		}
		else {
			size_t batch_size = 1;
			gia_edit_col_type_t text_col_type = GiaEditColPlain;

			if (line_tok >= line->toks_size || line_ch < line->toks[line_tok].pos_start.line_ch) {
				size_t line_ch_max = line->size;

				if (line_tok < line->toks_size) {
					line_ch_max = line->toks[line_tok].pos_start.line_ch;
				}

				while (line_ch + batch_size < line_ch_max
					&& line->str[line_ch + batch_size] != L'\t'
					&& line_col + batch_size < line_col_max) {
					++batch_size;
				}
			}
			else {
				pla_tok_t * tok = &line->toks[line_tok++];

				batch_size = tok->pos_end.line_ch - tok->pos_start.line_ch;

				switch (tok->type) {
					case PlaTokKeyw:
						text_col_type = GiaEditColKeyw;
						break;
					case PlaTokChStr:
						text_col_type = GiaEditColChStr;
						break;
					case PlaTokNumStr:
						text_col_type = GiaEditColNumStr;
						break;
				}
			}

			SetTextColor(hdc, data->style.cols[text_col_type].cr);

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
		gia_text_ch_pos_t sel_start, sel_end;
		get_sel_pos(data, &sel_start, &sel_end);

		RECT line_rect = { .top = 0, .bottom = (LONG)line_h };

		for (size_t line_i = data->vis_line; line_i < line_i_max; ++line_i) {
			gia_text_line_t * line = &data->text.lines[line_i];

			if (data->is_sel && line_i >= sel_start.line && line_i <= sel_end.line) {
				RECT sel_rect = {
					.left = (LONG)(clamp_line_ch_to_vis_col(data, line, line_i == sel_start.line ? sel_start.ch : 0) * ch_w),
					.top = line_rect.top,
					.right = (LONG)(clamp_line_ch_to_vis_col(data, line, line_i == sel_end.line ? sel_end.ch : line->size) * ch_w),
					.bottom = line_rect.bottom
				};

				FillRect(hdc, &sel_rect, data->style.cols[GiaEditColSel].hb);
			}

			draw_line(data, hdc, line_i, max_w);

			line_rect.top += (LONG)line_h;
			line_rect.bottom += (LONG)line_h;
		}
	}

	{
		LONG err_line_size = (LONG)(line_h * data->style.err_line_size);

		for (pla_ec_err_t * err = data->text.ec_sndr.buf.errs, *err_end = err + data->text.ec_sndr.buf.errs_size; err != err_end; ++err) {
			if (err->pos_end.line_num < data->vis_line) {
				continue;
			}

			if (err->pos_start.line_num > data->vis_line + max_h) {
				break;
			}

			size_t line_start = max(err->pos_start.line_num, data->vis_line);

			LONG err_line_off_h = (LONG)((line_start - data->vis_line + 1) * line_h);

			RECT err_rect = { .top = err_line_off_h - err_line_size, .bottom = err_line_off_h };

			for (size_t line_i = line_start; line_i <= err->pos_end.line_num; ++line_i) {
				gia_text_line_t * line = &data->text.lines[line_i];

				if (line_i >= err->pos_start.line_num) {
					err_rect.left = (LONG)(clamp_line_ch_to_vis_col(data, line, line_i == err->pos_start.line_num ? err->pos_start.line_ch : 0) * ch_w);
					err_rect.right = (LONG)(clamp_line_ch_to_vis_col(data, line, line_i == err->pos_end.line_num ? err->pos_end.line_ch : line->size) * ch_w);

					FillRect(hdc, &err_rect, data->style.cols[GiaEditColErr].hb);

					err_rect.top += (LONG)line_h;
					err_rect.bottom += (LONG)line_h;
				}
			}
		}
	}
}
static void redraw_lines(void * user_data, HDC hdc, RECT * rect) {
	wnd_data_t * data = user_data;

	EnterCriticalSection(&data->text.lock);
	EnterCriticalSection(&data->text.ec_sndr.buf.lock);

	__try {
		redraw_lines_nl(data, hdc, rect);
	}
	__finally {
		LeaveCriticalSection(&data->text.lock);
		LeaveCriticalSection(&data->text.ec_sndr.buf.lock);
	}
}

static void set_vis_pos(wnd_data_t * data, size_t vis_line, size_t vis_col) {
	if (data->vis_line != vis_line
		|| data->vis_col != vis_col) {
		data->vis_line = vis_line;
		data->vis_col = vis_col;

		InvalidateRect(data->hw, NULL, FALSE);

		redraw_caret_nl(data);
	}
}

static void set_caret_pos_col(wnd_data_t * data, size_t caret_line, size_t caret_col) {
	if (data->caret_line != caret_line
		|| data->caret_col != caret_col) {
		data->caret_line = caret_line;
		data->caret_col = caret_col;

		redraw_caret_nl(data);
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
static void update_vis_to_caret(wnd_data_t * data, size_t caret_line, size_t caret_col) {
	size_t vis_line = data->vis_line, vis_col = data->vis_col;

	RECT rect;
	GetClientRect(data->hw, &rect);

	size_t max_w = (size_t)rect.right / data->style.font.f_w;
	size_t max_h = (size_t)rect.bottom / data->style.font.f_h;

	vis_line = calc_vis_val(data, caret_line, vis_line, max_h);
	vis_col = calc_vis_val(data, caret_col, vis_col, max_w);

	set_vis_pos(data, vis_line, vis_col);
}

static void remove_sel_text_nl(wnd_data_t * data) {
	gia_text_ch_pos_t sel_start, sel_end;

	get_sel_pos(data, &sel_start, &sel_end);

	if (sel_start.line == sel_end.line) {
		gia_text_remove_str_nl(&data->text, sel_start.line, sel_start.ch, sel_end.ch);
	}
	else {
		gia_text_line_t * sel_end_line = &data->text.lines[sel_end.line];

		gia_text_remove_str_nl(&data->text, sel_start.line, sel_start.ch, data->text.lines[sel_start.line].size);
		gia_text_insert_str_nl(&data->text, sel_start.line, sel_start.ch, sel_end_line->size - sel_end.ch, sel_end_line->str + sel_end.ch);

		for (size_t line_i = sel_start.line + 1; line_i <= sel_end.line; ++line_i) {
			gia_text_remove_line_nl(&data->text, sel_start.line + 1);
		}
	}

	set_caret_pos_ch(data, sel_start.line, sel_start.ch);
}

static void process_caret_keyd_wp_nl_back(wnd_data_t * data) {
	gia_text_ch_pos_t pos = get_caret_ch_pos(data);

	if (data->is_sel) {
		remove_sel_text_nl(data);

		data->is_sel = false;

		InvalidateRect(data->hw, NULL, FALSE);
	}
	else {
		if (pos.ch > 0) {
			gia_text_remove_ch_nl(&data->text, pos.line, pos.ch - 1);

			set_caret_pos_ch(data, pos.line, pos.ch - 1);

			InvalidateRect(data->hw, NULL, FALSE);
		}
		else if (pos.line > 0) {
			gia_text_line_t * ins_line = &data->text.lines[pos.line - 1];
			gia_text_line_t * rem_line = &data->text.lines[pos.line];

			size_t new_caret_ch = ins_line->size;

			gia_text_insert_str_nl(&data->text, pos.line - 1, ins_line->size, rem_line->size, rem_line->str);

			gia_text_remove_line_nl(&data->text, pos.line);

			set_caret_pos_ch(data, pos.line - 1, new_caret_ch);

			InvalidateRect(data->hw, NULL, FALSE);
		}
	}
}
static void process_caret_keyd_wp_nl_ret(wnd_data_t * data) {
	if (data->is_sel) {
		remove_sel_text_nl(data);

		data->is_sel = false;
	}

	gia_text_ch_pos_t pos = get_caret_ch_pos(data);

	gia_text_insert_line_nl(&data->text, pos.line + 1);

	gia_text_line_t * ins_line = &data->text.lines[pos.line + 1];
	gia_text_line_t * src_line = &data->text.lines[pos.line];

	gia_text_insert_str_nl(&data->text, pos.line + 1, 0, src_line->size - pos.ch, src_line->str + pos.ch);
	gia_text_remove_str_nl(&data->text, pos.line, pos.ch, src_line->size);

	set_caret_pos_ch(data, pos.line + 1, 0);

	InvalidateRect(data->hw, NULL, FALSE);
}
static void process_caret_keyd_wp_nl_cret(wnd_data_t * data) {
	if (data->is_sel) {
		remove_sel_text_nl(data);

		data->is_sel = false;
	}

	gia_text_ch_pos_t pos = get_caret_ch_pos(data);

	gia_text_insert_line_nl(&data->text, pos.line);

	set_caret_pos_ch(data, pos.line, pos.ch);

	InvalidateRect(data->hw, NULL, FALSE);
}
static void process_caret_keyd_wp_nl_nav(wnd_data_t * data, WPARAM wp) {
	ul_assert(data->text.lines_size > 0);

	if (data->is_sel) {
		gia_text_ch_pos_t sel_start, sel_end;

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

		InvalidateRect(data->hw, NULL, FALSE);
	}
	else {
		gia_text_ch_pos_t caret_pos = get_caret_ch_pos(data);

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
		remove_sel_text_nl(data);

		data->is_sel = false;

		InvalidateRect(data->hw, NULL, FALSE);
	}
	else {
		gia_text_ch_pos_t pos = get_caret_ch_pos(data);

		gia_text_line_t * line = &data->text.lines[pos.line];

		if (pos.ch < line->size) {
			gia_text_remove_ch_nl(&data->text, pos.line, pos.ch);

			InvalidateRect(data->hw, NULL, FALSE);
		}
		else if (pos.line + 1 < data->text.lines_size) {
			gia_text_line_t * src_line = &data->text.lines[pos.line + 1];

			gia_text_insert_str_nl(&data->text, pos.line, line->size, src_line->size, src_line->str);

			gia_text_remove_line_nl(&data->text, pos.line + 1);

			InvalidateRect(data->hw, NULL, FALSE);
		}
	}
}
static void process_caret_keyd_wp_nl_vis(wnd_data_t * data) {
	gia_text_ch_pos_t pos = get_caret_ch_pos(data);

	size_t caret_col = convert_line_ch_to_col(data, &data->text.lines[pos.line], pos.ch);

	update_vis_to_caret(data, pos.line, caret_col);
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

	__try {
		process_caret_keyd_wp_nl(data, wp);
	}
	__finally {
		LeaveCriticalSection(&data->text.lock);
	}
}

static void process_caret_ch_wp_nl(wnd_data_t * data, WPARAM wp) {
	wchar_t ch = (wchar_t)wp;

	if (iswprint(ch) || ch == L'\t') {
		if (data->is_sel) {
			remove_sel_text_nl(data);

			data->is_sel = false;
		}

		gia_text_ch_pos_t pos = get_caret_ch_pos(data);

		gia_text_insert_ch_nl(&data->text, pos.line, pos.ch, ch);

		InvalidateRect(data->hw, NULL, FALSE);

		set_caret_pos_ch(data, pos.line, pos.ch + 1);

		size_t caret_col = convert_line_ch_to_col(data, &data->text.lines[pos.line], pos.ch + 1);

		update_vis_to_caret(data, pos.line, caret_col);
	}
}
static void process_caret_ch_wp(wnd_data_t * data, WPARAM wp) {
	EnterCriticalSection(&data->text.lock);

	__try {
		process_caret_ch_wp_nl(data, wp);
	}
	__finally {
		LeaveCriticalSection(&data->text.lock);
	}
}

static void process_caret_cur_lp_nl(wnd_data_t * data, LPARAM lp) {
	int x = max(GET_X_LPARAM(lp), 0), y = max(GET_Y_LPARAM(lp), 0);

	x += (int)(data->style.font.f_w / 2);

	ul_assert(data->text.lines_size > 0);

	gia_text_ch_pos_t pos = get_ch_pos(data,
		data->vis_line + (size_t)y / data->style.font.f_h,
		data->vis_col + (size_t)x / data->style.font.f_w);

	set_caret_pos_ch(data, pos.line, pos.ch);

	if (data->is_sel) {
		InvalidateRect(data->hw, NULL, FALSE);
	}
}
static void process_caret_cur_lp(wnd_data_t * data, LPARAM lp) {
	EnterCriticalSection(&data->text.lock);

	__try {
		process_caret_cur_lp_nl(data, lp);
	}
	__finally {
		LeaveCriticalSection(&data->text.lock);
	}
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

	__try {
		process_vis_wheel_y_nl(data, wheel_delta);
	}
	__finally {
		LeaveCriticalSection(&data->text.lock);
	}
}
static void process_vis_wheel_x_nl(wnd_data_t * data, int wheel_delta) {
	data->wheel_x += wheel_delta * VIS_WHEEL_MUL;

	gia_text_ch_pos_t pos = get_caret_ch_pos(data);

	gia_text_line_t * line = &data->text.lines[pos.line];

	size_t vis_col = process_vis_wheel_c_wp(data, &data->wheel_x, data->vis_col, convert_line_ch_to_col(data, line, line->size));

	set_vis_pos(data, data->vis_line, vis_col);
}
static void process_vis_wheel_x(wnd_data_t * data, int wheel_delta) {
	EnterCriticalSection(&data->text.lock);

	__try {
		process_vis_wheel_x_nl(data, wheel_delta);
	}
	__finally {
		LeaveCriticalSection(&data->text.lock);
	}
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
}
static void process_sel_timer(wnd_data_t * data) {
	EnterCriticalSection(&data->text.lock);

	__try {
		process_sel_timer_nl(data);
	}
	__finally {
		LeaveCriticalSection(&data->text.lock);
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

			load_tus_data(data);
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

static LRESULT wnd_proc_data(wnd_data_t * data, UINT msg, WPARAM wp, LPARAM lp) {
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
		case WM_KEYUP:
			switch (wp) {
				case VK_ESCAPE:
					if (data->is_fcs) {
						SetFocus(data->prev_fcs_hw);
					}
					break;
				case VK_F5:
					if (data->exe_name != NULL) {
						SubmitThreadpoolWork(data->run_exe_work);
					}
					else {
						wprintf(L"no exe_name");
					}
					break;
				case VK_F6:
					if (data->repo != NULL && data->tus != NULL && data->exe_name != NULL) {
						SubmitThreadpoolWork(data->build_work);
					}
					else {
						wprintf(L"no repo || no tus || no exe_name");
					}
					break;
				case VK_F7:
					if (data->tus != NULL) {
						save_tus_data(data);
						wprintf(L"saved text data to tus\n");
					}
					else {
						wprintf(L"no tus\n");
					}
					break;
				case VK_F8:
					_wsystem(L"cls");
					break;
			}
			break;
		case WM_CHAR:
			process_caret_ch_wp(data, wp);
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

				gia_text_init(&data->text, data->ctx->es_ctx);

				data->run_exe_work = CreateThreadpoolWork(run_exe_worker, data, NULL);

				if (data->run_exe_work == NULL) {
					return FALSE;
				}

				data->build_work = CreateThreadpoolWork(build_prog_worker, data, NULL);

				if (data->build_work == NULL) {
					return FALSE;
				}
			}
			break;
		case WM_NCDESTROY:
			if (data != NULL) {
				if (data->run_exe_work != NULL) {
					CloseThreadpoolWork(data->run_exe_work);
				}

				if (data->build_work != NULL) {
					WaitForThreadpoolWorkCallbacks(data->build_work, FALSE);

					CloseThreadpoolWork(data->build_work);
				}

				gia_text_cleanup(&data->text);

				cleanup_style(&data->style);

				wa_ctl_remove_data(hw);

				free(data);

				data = NULL;

				wa_wnd_set_fp(hw, data);
			}
			break;
		default:
			if (data != NULL) {
				wnd_proc_data(data, msg, wp, lp);
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
			[GiaEditColSel] = RGB(176, 220, 255),
			[GiaEditColErr] = RGB(255, 0, 0),
			[GiaEditColKeyw] = RGB(0, 0, 255),
			[GiaEditColChStr] = RGB(163, 21, 21),
			[GiaEditColNumStr] = RGB(47, 79, 79),
	},
	.font = ctx->style.fonts[WaStyleFontFxd].lf,
	.err_line_size = 0.125
	};

	desc.font.lfHeight = -16;

	return desc;
}

void gia_edit_attach_ec_rcvr(HWND hw, pla_ec_rcvr_t * ec_rcvr) {
	ul_assert(wa_wnd_check_cls(hw, GIA_EDIT_WND_CLS_NAME));

	wnd_data_t * data = wa_wnd_get_fp(hw);

	pla_ec_sndr_attach_rcvr(&data->text.ec_sndr, ec_rcvr);
}
