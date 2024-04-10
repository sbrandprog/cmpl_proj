#include "pch.h"
#include "wa_lib/wa_ctx.h"
#include "wa_lib/wa_wnd.h"
#include "wa_lib/wa_ctl.h"
#include "pla_punc.h"
#include "pla_keyw.h"
#include "pla_dclr.h"
#include "pla_edit.h"

#define TAB_SIZE 4

#define LINE_INIT_STR_SIZE 64

#define GKS_PRS_CHECK 0x8000

typedef struct pla_edit_style {
	wa_style_col_t cols[PlaEditCol_Count];
	wa_style_font_t font;
} style_t;

typedef struct pla_edit_line {
	size_t cap;
	wchar_t * str;
	size_t size;
} line_t;

typedef struct pla_edit_data {
	HWND hw;
	wa_ctx_t * ctx;
	wa_ctl_data_t ctl_data;
	style_t style;

	bool is_fcs;
	HWND prev_fcs_hw;

	size_t lines_cap;
	line_t * lines;
	size_t lines_size;

	size_t vis_line;
	size_t vis_ch;

	size_t caret_line;
	size_t caret_col;
	size_t caret_ch;
} wnd_data_t;


static bool init_style(style_t * style, pla_edit_style_desc_t * desc) {
	for (pla_edit_col_type_t col = 0; col < PlaEditCol_Count; ++col) {
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
	for (pla_edit_col_type_t col = 0; col < PlaEditCol_Count; ++col) {
		wa_style_col_cleanup(&style->cols[col]);
	}

	wa_style_font_cleanup(&style->font);
}


static void init_line(line_t * line) {
	*line = (line_t){ 0 };

	line->str = malloc(LINE_INIT_STR_SIZE * sizeof(*line->str));

	ul_raise_check_mem_alloc(line->str);

	line->cap = LINE_INIT_STR_SIZE;
}
static void cleanup_line(line_t * line) {
	free(line->str);

	memset(line, 0, sizeof(*line));
}

static void insert_line_ch(wnd_data_t * data, line_t * line, size_t ins_pos, wchar_t ch) {
	ul_raise_assert(ins_pos <= line->size);

	if (line->size + 1 > line->cap) {
		ul_arr_grow(&line->cap, &line->str, sizeof(*line->str), 1);
	}

	wmemmove_s(line->str + ins_pos + 1, line->cap - ins_pos, line->str + ins_pos, line->size - ins_pos);
	line->str[ins_pos] = ch;

	++line->size;
}
static void remove_line_ch(wnd_data_t * data, line_t * line, size_t rem_pos) {
	ul_raise_assert(line->size > 0 && rem_pos < line->size);

	wmemmove_s(line->str + rem_pos, line->cap - rem_pos, line->str + rem_pos + 1, line->size - rem_pos);

	--line->size;
}

static size_t get_ch_col_size(wnd_data_t * data, size_t line_col, wchar_t ch) {
	if (ch == L'\t') {
		return TAB_SIZE - line_col % TAB_SIZE;
	}

	return 1;
}
static size_t convert_line_ch_to_col(wnd_data_t * data, line_t * line, size_t line_ch) {
	ul_raise_assert(line_ch <= line->size);

	size_t line_col = 0;

	for (wchar_t * ch = line->str, *ch_end = ch + line_ch; ch != ch_end; ++ch) {
		line_col += get_ch_col_size(data, line_col, *ch);
	}

	return line_col;
}
static size_t convert_line_col_to_ch(wnd_data_t * data, line_t * line, size_t line_col) {
	size_t line_ch = 0, line_col_calc = 0;

	for (; line_ch < line->size; ++line_ch) {
		if (line_col_calc >= line_col) {
			return line_ch;
		}

		line_col_calc += get_ch_col_size(data, line_col_calc, line->str[line_ch]);
	}

	return line_ch;
}


static line_t * insert_line(wnd_data_t * data, size_t ins_pos) {
	ul_raise_assert(ins_pos <= data->lines_size);

	if (data->lines_size + 1 > data->lines_cap) {
		ul_arr_grow(&data->lines_cap, &data->lines, sizeof(*data->lines), 1);
	}

	line_t * ins_it = data->lines + ins_pos;

	memmove_s(ins_it + 1, (data->lines_cap - ins_pos) * sizeof(*ins_it), ins_it, (data->lines_size - ins_pos) * sizeof(*ins_it));

	++data->lines_size;

	init_line(ins_it);

	return ins_it;
}
static void remove_line(wnd_data_t * data, size_t rem_pos) {
	ul_raise_assert(rem_pos < data->lines_size);

	cleanup_line(&data->lines[rem_pos]);

	line_t * rem_it = data->lines + rem_pos;

	memmove_s(rem_it, (data->lines_cap - rem_pos) * sizeof(*rem_it), rem_it + 1, (data->lines_size - rem_pos) * sizeof(*rem_it));

	--data->lines_size;
}


static bool is_caret_vis(wnd_data_t * data) {
	if (data->vis_line > data->caret_line
		|| data->vis_ch > data->caret_ch) {
		return false;
	}

	return true;
}

static void redraw_caret(wnd_data_t * data) {
	if (!data->is_fcs) {
		return;
	}

	if (is_caret_vis(data)) {
		int caret_x = (int)(convert_line_ch_to_col(data, &data->lines[data->caret_line], data->caret_ch) * data->style.font.f_w);
		int caret_y = (int)((data->caret_line - data->vis_line) * data->style.font.f_h);

		SetCaretPos(caret_x, caret_y);

		ShowCaret(data->hw);
	}
	else {
		HideCaret(data->hw);
	}
}
static void draw_line(wnd_data_t * data, HDC hdc, line_t * line, size_t line_pos) {
	for (size_t line_ch = 0, ch_pos = 0; line_ch < line->size;) {
		wchar_t ch = line->str[line_ch];

		if (ch == L'\t') {
			ch_pos += TAB_SIZE - ch_pos % TAB_SIZE;
			++line_ch;
		}
		else {
			size_t line_batch = line_ch + 1;

			while (line_batch < line->size && line->str[line_batch] != L'\t') {
				++line_batch;
			}

			size_t batch_size = line_batch - line_ch;

			TextOutW(hdc, (int)(ch_pos * data->style.font.f_w), (int)(line_pos * data->style.font.f_h), line->str + line_ch, (int)batch_size);

			ch_pos += batch_size;
			line_ch = line_batch;
		}
	}
}
static void redraw_lines(wnd_data_t * data, HDC hdc, RECT * rect) {
	HWND hw = data->hw;
	wa_ctx_t * ctx = data->ctx;

	FillRect(hdc, rect, ctx->style.cols[WaStyleColBg].hb);

	SelectFont(hdc, data->style.font.hf);
	SetBkColor(hdc, ctx->style.cols[WaStyleColBg].cr);

	{
		size_t max_w = ((size_t)rect->right + data->style.font.f_w - 1) / data->style.font.f_w;
		size_t max_h = ((size_t)rect->bottom + data->style.font.f_h - 1) / data->style.font.f_h;

		LONG line_h_step = (LONG)data->style.font.f_h;

		for (size_t line_i = 0; line_i + data->vis_line < data->lines_size && line_i < max_h; ++line_i) {
			line_t * line = &data->lines[line_i + data->vis_line];

			draw_line(data, hdc, line, line_i);
		}
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

static void set_caret_pos_col(wnd_data_t * data, size_t caret_line, size_t caret_col) {
	caret_line = min(caret_line, data->lines_size);

	size_t caret_ch = convert_line_col_to_ch(data, &data->lines[caret_line], caret_col);

	if (data->caret_line != caret_line
		|| data->caret_col != caret_col
		|| data->caret_ch != caret_ch) {
		data->caret_line = caret_line;
		data->caret_col = caret_col;
		data->caret_ch = caret_ch;

		redraw_caret(data);
	}
}
static void set_caret_pos_ch(wnd_data_t * data, size_t caret_line, size_t caret_ch) {
	caret_line = min(caret_line, data->lines_size);

	caret_ch = min(caret_ch, data->lines[caret_line].size);

	size_t caret_col = convert_line_ch_to_col(data, &data->lines[caret_line], caret_ch);

	if (data->caret_line != caret_line
		|| data->caret_col != caret_col
		|| data->caret_ch != caret_ch) {
		data->caret_line = caret_line;
		data->caret_col = caret_col;
		data->caret_ch = caret_ch;

		redraw_caret(data);
	}
}
static void set_caret_pos_by_cur_lp(wnd_data_t * data, LPARAM lp) {
	int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);

	if (x < 0 || y < 0) {
		return;
	}

	x += (int)(data->style.font.f_w / 2);

	ul_raise_assert(data->lines_size > 0);

	size_t caret_line = min(data->lines_size - 1, data->vis_line + (size_t)y / data->style.font.f_h);

	line_t * line = &data->lines[caret_line];

	size_t caret_col = (size_t)x / data->style.font.f_w;

	set_caret_pos_col(data, caret_line, caret_col);
}
static void set_caret_pos_by_nav_wp(wnd_data_t * data, WPARAM wp) {
	size_t caret_line = data->caret_line, caret_col = data->caret_col, caret_ch = data->caret_ch;

	switch (wp) {
		case VK_END:
			set_caret_pos_ch(data, caret_line, data->lines[caret_line].size);
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
				caret_ch = data->lines[caret_line].size;
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
			if (caret_ch < data->lines[caret_line].size) {
				++caret_ch;
			}
			else if (caret_line < data->lines_size - 1) {
				++caret_line;
				caret_ch = 0;
			}

			set_caret_pos_ch(data, caret_line, caret_ch);
			break;
		case VK_DOWN:
			if (caret_line < data->lines_size - 1) {
				++caret_line;
			}
			else if (caret_col < data->lines[caret_line].size) {
				caret_col = data->lines[caret_line].size;
			}

			set_caret_pos_col(data, caret_line, caret_col);
			break;
		default:
			ul_raise_unreachable();
	}
}

static void process_caret_back(wnd_data_t * data) {
	if (data->caret_ch > 0) {
		remove_line_ch(data, &data->lines[data->caret_line], data->caret_ch - 1);

		InvalidateRect(data->hw, NULL, FALSE);

		set_caret_pos_ch(data, data->caret_line, data->caret_ch - 1);
	}
	else if (data->caret_line > 0) {
		line_t * ins_line = &data->lines[data->caret_line - 1];
		line_t * rem_line = &data->lines[data->caret_line];

		if (ins_line->size + rem_line->size > ins_line->cap) {
			ul_arr_grow(&ins_line->cap, &ins_line->str, sizeof(*ins_line->str), ins_line->size + rem_line->size - ins_line->cap);
		}

		wmemcpy_s(ins_line->str + ins_line->size, ins_line->cap - ins_line->size, rem_line->str, rem_line->size);

		size_t caret_ch = ins_line->size;

		ins_line->size += rem_line->size;

		remove_line(data, data->caret_line);

		InvalidateRect(data->hw, NULL, FALSE);

		set_caret_pos_ch(data, data->caret_line - 1, caret_ch);
	}
}
static void process_caret_ret(wnd_data_t * data) {
	line_t * ins_line = insert_line(data, data->caret_line + 1);
	line_t * src_line = &data->lines[data->caret_line];

	if (src_line->size - data->caret_ch > ins_line->cap) {
		ul_arr_grow(&ins_line->cap, &ins_line->str, sizeof(*ins_line->str), src_line->size - data->caret_ch - ins_line->cap);
	}

	wmemcpy_s(ins_line->str, ins_line->cap, src_line->str + data->caret_ch, src_line->size - data->caret_ch);
	ins_line->size = src_line->size - data->caret_ch;

	src_line->size = data->caret_ch;

	InvalidateRect(data->hw, NULL, FALSE);

	set_caret_pos_ch(data, data->caret_line + 1, 0);
}
static void process_caret_cret(wnd_data_t * data) {
	line_t * line = insert_line(data, data->caret_line);

	InvalidateRect(data->hw, NULL, FALSE);

	set_caret_pos_col(data, data->caret_line, data->caret_col);
}
static void process_caret_del(wnd_data_t * data) {
	line_t * line = &data->lines[data->caret_line];

	if (data->caret_ch < line->size) {
		remove_line_ch(data, line, data->caret_ch);

		InvalidateRect(data->hw, NULL, FALSE);
	}
	else if (data->caret_line + 1 < data->lines_size) {
		line_t * src_line = &data->lines[data->caret_line + 1];

		if (line->size + src_line->size > line->cap) {
			ul_arr_grow(&line->cap, &line->str, sizeof(*line->str), line->size + src_line->size - line->cap);
		}

		wmemcpy_s(line->str + line->size, line->cap - line->size, src_line->str, src_line->size);
		line->size += src_line->size;

		remove_line(data, data->caret_line + 1);

		InvalidateRect(data->hw, NULL, FALSE);
	}
}
static void process_caret_ch(wnd_data_t * data, WPARAM wp) {
	wchar_t ch = (wchar_t)wp;

	if (iswprint(ch) || ch == L'\t') {
		insert_line_ch(data, &data->lines[data->caret_line], data->caret_ch, ch);

		InvalidateRect(data->hw, NULL, FALSE);

		set_caret_pos_ch(data, data->caret_line, data->caret_ch + 1);
	}
}


static bool process_cd_args(wnd_data_t * data, wa_wnd_cd_t * cd) {
	HWND hw = data->hw;

	for (const wchar_t * arg = va_arg(cd->args, const wchar_t *); arg != NULL; arg = va_arg(cd->args, const wchar_t *)) {
		if (wcscmp(arg, pla_edit_prop_style_desc) == 0) {
			pla_edit_style_desc_t desc = va_arg(cd->args, pla_edit_style_desc_t);

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

			insert_line(data, line_num);
		}
		else {
			insert_line_ch(data, &data->lines[line_num], line_ch++, (wchar_t)ch);
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
			switch (wp) {
				case VK_BACK:
					process_caret_back(data);
					break;
				case VK_RETURN:
					if ((GetKeyState(VK_CONTROL) & GKS_PRS_CHECK) != 0) {
						process_caret_cret(data);
					}
					else {
						process_caret_ret(data);
					}
					break;
				case VK_END:
				case VK_HOME:
				case VK_LEFT:
				case VK_UP:
				case VK_RIGHT:
				case VK_DOWN:
					set_caret_pos_by_nav_wp(data, wp);
					break;
				case VK_DELETE:
					process_caret_del(data);
					break;
			}
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
			process_caret_ch(data, wp);
			break;
		case WM_MOUSEMOVE:
			if (wp & MK_LBUTTON) {
				set_caret_pos_by_cur_lp(data, lp);
			}
			break;
		case WM_LBUTTONDOWN:
			set_caret_pos_by_cur_lp(data, lp);

			if (!data->is_fcs) {
				SetFocus(hw);
			}
			break;
		case WM_LBUTTONUP:
			set_caret_pos_by_cur_lp(data, lp);
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
					pla_edit_style_desc_t desc = pla_edit_get_style_desc_dflt(data->ctx);

					if (!init_style(&data->style, &desc)) {
						return FALSE;
					}
				}

				insert_line(data, 0);
			}
			break;
		case WM_NCDESTROY:
			if (data != NULL) {
				for (line_t * line = data->lines, *line_end = line + data->lines_size; line != line_end; ++line) {
					cleanup_line(line);
				}

				free(data->lines);

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

WNDCLASSEXW pla_edit_get_wnd_cls_desc() {
	WNDCLASSEXW wnd_cls_desc = {
		.style = CS_VREDRAW | CS_HREDRAW | CS_PARENTDC,
		.lpfnWndProc = wnd_proc,
		.cbWndExtra = sizeof(wnd_data_t *),
		.hCursor = LoadCursorW(NULL, IDC_IBEAM),
		.lpszClassName = PLA_EDIT_WND_CLS_NAME
	};

	return wnd_cls_desc;
}

pla_edit_style_desc_t pla_edit_get_style_desc_dflt(wa_ctx_t * ctx) {
	pla_edit_style_desc_t desc = {
		.cols = {
			[PlaEditColPlain] = RGB(0, 0, 0),
			[PlaEditColKeyw] = RGB(0, 0, 255)
	},
		.font = ctx->style.fonts[WaStyleFontFxd].lf
	};

	desc.font.lfHeight = -16;

	return desc;
}
