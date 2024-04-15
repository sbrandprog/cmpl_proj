#include "pch.h"
#include "wa_lib/wa_ctx.h"
#include "wa_lib/wa_wnd.h"
#include "wa_lib/wa_ctl.h"
#include "pla_punc.h"
#include "pla_keyw.h"
#include "pla_dclr.h"
#include "gia_text.h"
#include "gia_edit.h"

#define TAB_SIZE 4
#define VIS_SPACE 3
#define VIS_WHEEL_MUL 2

#define GKS_PRS_CHECK 0x8000

typedef struct gia_edit_style {
	wa_style_col_t cols[GiaEditCol_Count];
	wa_style_font_t font;
} style_t;

typedef struct gia_edit_data {
	HWND hw;
	wa_ctx_t * ctx;
	wa_ctl_data_t ctl_data;
	style_t style;

	gia_text_t text;

	size_t vis_line;
	size_t vis_col;

	size_t caret_line;
	size_t caret_col;

	bool is_fcs;
	HWND prev_fcs_hw;

	int wheel_x;
	int wheel_y;
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

	return true;
}
static void cleanup_style(style_t * style) {
	for (gia_edit_col_type_t col = 0; col < GiaEditCol_Count; ++col) {
		wa_style_col_cleanup(&style->cols[col]);
	}

	wa_style_font_cleanup(&style->font);
}


static size_t get_ch_col_size(wnd_data_t * data, size_t line_col, wchar_t ch) {
	if (ch == L'\t') {
		return TAB_SIZE - line_col % TAB_SIZE;
	}

	return 1;
}
static size_t convert_line_ch_to_col(wnd_data_t * data, gia_text_line_t * line, size_t line_ch) {
	ul_raise_assert(line_ch <= line->size);

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
	ul_raise_assert(data->text.lines_size > 0);

	line = min(line, data->text.lines_size - 1);

	size_t ch = convert_line_col_to_ch(data, &data->text.lines[line], col);

	return (gia_text_ch_pos_t){ .line = line, .ch = ch };
}


static gia_text_ch_pos_t get_caret_ch_pos(wnd_data_t * data) {
	return get_ch_pos(data, data->caret_line, data->caret_col);
}

static void redraw_caret_nl(wnd_data_t * data) {
	if (!data->is_fcs) {
		return;
	}

	gia_text_ch_pos_t pos = get_caret_ch_pos(data);

	size_t col = convert_line_ch_to_col(data, &data->text.lines[data->caret_line], pos.ch);

	int x, y;

	if (data->vis_line <= pos.line && data->vis_col <= col) {
		x = (int)((col - data->vis_col) * data->style.font.f_w);
		y = (int)((pos.line - data->vis_line) * data->style.font.f_h);
	}
	else {
		x = -(int)data->style.font.f_w;
		y = -(int)data->style.font.f_h;
	}

	SetCaretPos(x, y);
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
static void draw_line(wnd_data_t * data, HDC hdc, size_t line_pos, size_t line_max_w) {
	gia_text_line_t * line = &data->text.lines[data->vis_line + line_pos];

	int line_h = (int)(line_pos * data->style.font.f_h);
	size_t line_col_max = line_max_w + data->vis_col;

	for (size_t line_ch = 0, line_col = 0; line_ch < line->size && line_col < line_col_max; ) {
		wchar_t ch = line->str[line_ch];

		if (ch == L'\t') {
			line_col += TAB_SIZE - line_col % TAB_SIZE;
			++line_ch;
		}
		else {
			if (line_col >= data->vis_col) {
				TextOutW(hdc, (int)((line_col - data->vis_col) * data->style.font.f_w), line_h, line->str + line_ch, 1);
			}

			++line_col;
			++line_ch;
		}
	}
}
static void redraw_lines_nl(wnd_data_t * data, HDC hdc, RECT * rect) {
	HWND hw = data->hw;
	wa_ctx_t * ctx = data->ctx;

	FillRect(hdc, rect, ctx->style.cols[WaStyleColBg].hb);

	SelectFont(hdc, data->style.font.hf);
	SetBkColor(hdc, ctx->style.cols[WaStyleColBg].cr);

	if (data->vis_line < data->text.lines_size) {
		size_t lines_to_draw = data->text.lines_size - data->vis_line;

		size_t max_w = ((size_t)rect->right + data->style.font.f_w - 1) / data->style.font.f_w;
		size_t max_h = ((size_t)rect->bottom + data->style.font.f_h - 1) / data->style.font.f_h;

		lines_to_draw = min(lines_to_draw, max_h);

		for (size_t line_i = 0; line_i < lines_to_draw; ++line_i) {
			draw_line(data, hdc, line_i, max_w);
		}
	}
}
static void redraw_lines(wnd_data_t * data, HDC hdc, RECT * rect) {
	EnterCriticalSection(&data->text.lock);

	__try {
		redraw_lines_nl(data, hdc, rect);
	}
	__finally {
		LeaveCriticalSection(&data->text.lock);
	}
}
static void redraw_lines_buf(wnd_data_t * data, HDC hdc) {
	HWND hw = data->hw;
	wa_ctx_t * ctx = data->ctx;
	RECT rect;

	GetClientRect(hw, &rect);

	HDC hdc_buf;

	HPAINTBUFFER buf = BeginBufferedPaint(hdc, &rect, BPBF_COMPATIBLEBITMAP, NULL, &hdc_buf);

	if (buf != NULL) {
		redraw_lines(data, hdc_buf, &rect);

		EndBufferedPaint(buf, TRUE);
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
	caret_line = min(caret_line, data->text.lines_size);

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

static void process_caret_keyd_wp_nl_back(wnd_data_t * data) {
	gia_text_ch_pos_t pos = get_caret_ch_pos(data);

	if (pos.ch > 0) {
		gia_text_remove_ch_nl(&data->text, pos.line, pos.ch - 1);

		InvalidateRect(data->hw, NULL, FALSE);

		set_caret_pos_ch(data, pos.line, pos.ch - 1);
	}
	else if (pos.line > 0) {
		gia_text_line_t * ins_line = &data->text.lines[pos.line - 1];
		gia_text_line_t * rem_line = &data->text.lines[pos.line];

		if (ins_line->size + rem_line->size > ins_line->cap) {
			ul_arr_grow(&ins_line->cap, &ins_line->str, sizeof(*ins_line->str), ins_line->size + rem_line->size - ins_line->cap);
		}

		wmemcpy_s(ins_line->str + ins_line->size, ins_line->cap - ins_line->size, rem_line->str, rem_line->size);

		size_t caret_ch = ins_line->size;

		ins_line->size += rem_line->size;

		gia_text_remove_line_nl(&data->text, pos.line);

		InvalidateRect(data->hw, NULL, FALSE);

		set_caret_pos_ch(data, pos.line - 1, caret_ch);
	}
}
static void process_caret_keyd_wp_nl_ret(wnd_data_t * data) {
	gia_text_ch_pos_t pos = get_caret_ch_pos(data);

	gia_text_insert_line_nl(&data->text, pos.line + 1);

	gia_text_line_t * ins_line = &data->text.lines[pos.line + 1];
	gia_text_line_t * src_line = &data->text.lines[pos.line];

	if (src_line->size - pos.ch > ins_line->cap) {
		ul_arr_grow(&ins_line->cap, &ins_line->str, sizeof(*ins_line->str), src_line->size - pos.ch - ins_line->cap);
	}

	wmemcpy_s(ins_line->str, ins_line->cap, src_line->str + pos.ch, src_line->size - pos.ch);
	ins_line->size = src_line->size - pos.ch;

	src_line->size = pos.ch;

	InvalidateRect(data->hw, NULL, FALSE);

	set_caret_pos_ch(data, pos.line + 1, 0);
}
static void process_caret_keyd_wp_nl_cret(wnd_data_t * data) {
	gia_text_ch_pos_t pos = get_caret_ch_pos(data);

	gia_text_insert_line_nl(&data->text, pos.line);

	InvalidateRect(data->hw, NULL, FALSE);
}
static void process_caret_keyd_wp_nl_nav(wnd_data_t * data, WPARAM wp) {
	gia_text_ch_pos_t pos = get_caret_ch_pos(data);

	size_t caret_line = pos.line, caret_col = data->caret_col, caret_ch = pos.ch;

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
			ul_raise_unreachable();
	}
}
static void process_caret_keyd_wp_nl_del(wnd_data_t * data) {
	gia_text_ch_pos_t pos = get_caret_ch_pos(data);

	gia_text_line_t * line = &data->text.lines[pos.line];

	if (pos.ch < line->size) {
		gia_text_remove_ch_nl(&data->text, pos.line, pos.ch);

		InvalidateRect(data->hw, NULL, FALSE);
	}
	else if (pos.line + 1 < data->text.lines_size) {
		gia_text_line_t * src_line = &data->text.lines[pos.line + 1];

		if (line->size + src_line->size > line->cap) {
			ul_arr_grow(&line->cap, &line->str, sizeof(*line->str), line->size + src_line->size - line->cap);
		}

		wmemcpy_s(line->str + line->size, line->cap - line->size, src_line->str, src_line->size);
		line->size += src_line->size;

		gia_text_remove_line_nl(&data->text, pos.line + 1);

		InvalidateRect(data->hw, NULL, FALSE);
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
	int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);

	if (x < 0 || y < 0) {
		return;
	}

	x += (int)(data->style.font.f_w / 2);

	ul_raise_assert(data->text.lines_size > 0);

	gia_text_ch_pos_t pos = get_ch_pos(data,
		data->vis_line + (size_t)y / data->style.font.f_h,
		data->vis_col + (size_t)x / data->style.font.f_w);

	set_caret_pos_ch(data, pos.line, pos.ch);
}
static void process_caret_cur_lp(wnd_data_t * data, WPARAM wp) {
	EnterCriticalSection(&data->text.lock);

	__try {
		process_caret_cur_lp_nl(data, wp);
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
		vis_val = min(vis_val_max, vis_val + (size_t)scroll_amount);
	}
	else {
		vis_val -= min(vis_val, (size_t)-scroll_amount);
	}

	return vis_val;
}
static void process_vis_wheel_y_wp_nl(wnd_data_t * data, WPARAM wp) {
	data->wheel_y += GET_WHEEL_DELTA_WPARAM(wp) * -VIS_WHEEL_MUL;

	size_t vis_line = process_vis_wheel_c_wp(data, &data->wheel_y, data->vis_line, data->text.lines_size);

	set_vis_pos(data, vis_line, data->vis_col);
}
static void process_vis_wheel_y_wp(wnd_data_t * data, WPARAM wp) {
	EnterCriticalSection(&data->text.lock);

	__try {
		process_vis_wheel_y_wp_nl(data, wp);
	}
	__finally {
		LeaveCriticalSection(&data->text.lock);
	}
}
static void process_vis_wheel_x_wp_nl(wnd_data_t * data, WPARAM wp) {
	data->wheel_x += GET_WHEEL_DELTA_WPARAM(wp) * VIS_WHEEL_MUL;

	gia_text_ch_pos_t pos = get_caret_ch_pos(data);

	gia_text_line_t * line = &data->text.lines[pos.line];

	size_t vis_col = process_vis_wheel_c_wp(data, &data->wheel_x, data->vis_col, line->size);

	set_vis_pos(data, data->vis_line, vis_col);
}
static void process_vis_wheel_x_wp(wnd_data_t * data, WPARAM wp) {
	EnterCriticalSection(&data->text.lock);

	__try {
		process_vis_wheel_x_wp_nl(data, wp);
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
		else {
			return false;
		}
	}

	return true;
}

static void fill_test_data(wnd_data_t * data) {
	FILE * file;

	(void)_wfopen_s(&file, L"test.pla", L"r");

	ul_raise_assert(file != NULL);

	size_t line_num = 0, line_ch = 0;

	wint_t ch;

	while ((ch = fgetwc(file)) != WEOF) {
		if (ch == L'\n') {
			++line_num;
			line_ch = 0;

			gia_text_insert_line(&data->text, line_num);
		}
		else {
			gia_text_insert_ch(&data->text, line_num, line_ch++, (wchar_t)ch);
		}
	}

	fclose(file);
}

static LRESULT wnd_proc_data(wnd_data_t * data, UINT msg, WPARAM wp, LPARAM lp) {
	HWND hw = data->hw;
	wa_ctx_t * ctx = data->ctx;

	switch (msg) {
		case WM_CREATE:
			if (!process_cd_args(data, wa_wnd_get_cd(lp))) {
				return -1;
			}

			fill_test_data(data);

			break;
		case WM_SETFOCUS:
			data->is_fcs = true;
			data->prev_fcs_hw = (HWND)wp;

			CreateCaret(hw, NULL, 1, (int)data->style.font.f_h);
			ShowCaret(hw);

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

			redraw_lines_buf(data, hdc);

			EndPaint(hw, &ps);

			return TRUE;
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
			}
			break;
		case WM_CHAR:
			process_caret_ch_wp(data, wp);
			break;
		case WM_MOUSEMOVE:
			if (wp & MK_LBUTTON) {
				process_caret_cur_lp(data, lp);
			}
			break;
		case WM_LBUTTONDOWN:
			process_caret_cur_lp(data, lp);

			if (!data->is_fcs) {
				SetFocus(hw);
			}
			break;
		case WM_LBUTTONUP:
			process_caret_cur_lp(data, lp);
			break;
		case WM_MOUSEWHEEL:
			process_vis_wheel_y_wp(data, wp);
			return 0;
		case WM_MOUSEHWHEEL:
			process_vis_wheel_x_wp(data, wp);
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

				gia_text_init(&data->text);
			}
			break;
		case WM_NCDESTROY:
			if (data != NULL) {
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
			[GiaEditColKeyw] = RGB(0, 0, 255)
	},
		.font = ctx->style.fonts[WaStyleFontFxd].lf
	};

	desc.font.lfHeight = -16;

	return desc;
}
