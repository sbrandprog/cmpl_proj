#include "pch.h"
#include "wa_lib/wa_ctx.h"
#include "wa_lib/wa_wnd.h"
#include "wa_lib/wa_ctl.h"
#include "pla_ec_buf.h"
#include "pla_lex.h"
#include "gia_ecv.h"

#define DISP_FONT WaStyleFontFxd
#define DISP_BUF_SIZE 256

typedef struct gia_ecv_data {
	HWND hw;
	wa_ctx_t * ctx;
	wa_ctl_data_t ctl_data;

	pla_ec_buf_t ec_buf;

	pla_ec_t ec;
} wnd_data_t;

static void post_err_proc(void * user_data, size_t group, pla_ec_pos_t pos_start, pla_ec_pos_t pos_end, ul_hs_t * msg) {
	wnd_data_t * data = user_data;

	pla_ec_post(&data->ec_buf.ec, group, pos_start, pos_end, msg);

	InvalidateRect(data->hw, NULL, FALSE);
}
static void clear_errs_proc(void * user_data, size_t group, pla_ec_pos_t pos_start, pla_ec_pos_t pos_end) {
	wnd_data_t * data = user_data;

	pla_ec_clear(&data->ec_buf.ec, group, pos_start, pos_end);

	InvalidateRect(data->hw, NULL, FALSE);
}

static wchar_t get_group_letter(size_t group) {
	switch (group) {
		case PLA_LEX_EC_GROUP:
			return L'L';
		default:
			return L'.';
	}
}
static size_t form_err_str(pla_ec_err_t * err, size_t buf_size, wchar_t * buf) {
	wchar_t group_letter = get_group_letter(err->group);

	int str_size = _snwprintf_s(buf, buf_size, _TRUNCATE, L"%c %4zi:%4zi %s", group_letter, err->pos_start.line_num, err->pos_start.line_ch, err->msg->str);

	if (str_size < 0) {
		str_size = 0;
	}

	return (size_t)str_size;
}

static void display_errs_nl(wnd_data_t * data, HDC hdc, RECT * rect) {
	wa_ctx_t * ctx = data->ctx;

	FillRect(hdc, rect, ctx->style.cols[WaStyleColBg].hb);

	wa_style_font_t * font = &ctx->style.fonts[DISP_FONT];
	
	SelectFont(hdc, font->hf);
	SetBkMode(hdc, TRANSPARENT);

	size_t err_i_max = min(data->ec_buf.errs_size, ((size_t)rect->bottom + font->f_h - 1) / font->f_h);

	for (size_t err_i = 0; err_i < err_i_max; ++err_i) {
		pla_ec_err_t * err = &data->ec_buf.errs[err_i];
		
		wchar_t buf[DISP_BUF_SIZE];

		size_t str_size = form_err_str(err, _countof(buf), buf);

		TextOutW(hdc, 0, (int)(err_i * font->f_h), buf, (int)str_size);
	}
}
static void display_errs(void * user_data, HDC hdc, RECT * rect) {
	wnd_data_t * data = user_data;

	EnterCriticalSection(&data->ec_buf.lock);

	__try {
		display_errs_nl(data, hdc, rect);
	}
	__finally {
		LeaveCriticalSection(&data->ec_buf.lock);
	}
}

static LRESULT wnd_proc_data(wnd_data_t * data, UINT msg, WPARAM wp, LPARAM lp) {
	HWND hw = data->hw;
	wa_ctx_t * ctx = data->ctx;

	switch (msg) {
		case WM_PAINT:
		{
			PAINTSTRUCT ps;

			HDC hdc = BeginPaint(hw, &ps);

			wa_wnd_paint_buf(hw, hdc, data, display_errs);

			EndPaint(hw, &ps);

			return 0;
		}
		case WM_ERASEBKGND:
			return TRUE;
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

				pla_ec_buf_init(&data->ec_buf);

				pla_ec_init(&data->ec, data, post_err_proc, clear_errs_proc);
			}
			break;
		case WM_NCDESTROY:
			if (data != NULL) {
				pla_ec_cleanup(&data->ec);

				pla_ec_buf_cleanup(&data->ec_buf);

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

WNDCLASSEX gia_ecv_get_wnd_cls_desc() {
	WNDCLASSEX wnd_cls_desc = {
		.style = CS_VREDRAW | CS_HREDRAW | CS_PARENTDC,
		.lpfnWndProc = wnd_proc,
		.cbWndExtra = sizeof(wnd_data_t *),
		.lpszClassName = GIA_ECV_WND_CLS_NAME
	};

	return wnd_cls_desc;
}

pla_ec_t * gia_ecv_get_ec(HWND hw) {
	ul_assert(wa_wnd_check_cls(hw, GIA_ECV_WND_CLS_NAME));

	wnd_data_t * data = wa_wnd_get_fp(hw);

	return &data->ec;
}
