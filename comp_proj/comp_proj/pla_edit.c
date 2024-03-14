#include "pch.h"
#include "wa_lib/wa_ctx.h"
#include "wa_lib/wa_wnd.h"
#include "wa_lib/wa_ctl.h"
#include "pla_punc.h"
#include "pla_keyw.h"
#include "pla_dclr.h"
#include "pla_edit.h"

typedef struct pla_edit_style {
	wa_style_col_t cols[PlaEditCol_Count];
	wa_style_font_t font;
} style_t;

typedef struct pla_edit_data {
	HWND hw;
	wa_ctx_t * ctx;
	wa_ctl_data_t ctl_data;
	style_t style;
} wnd_data_t;

typedef struct pla_edit_disp_ctx {
	wnd_data_t * data;
	HDC hdc;

	HWND hw;
	wa_ctx_t * wa_ctx;
	RECT rect;

	LONG font_w;
	LONG font_h;
} disp_ctx_t;

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
}

static void disp_spaces(disp_ctx_t * ctx, size_t spaces, size_t * x_ch, size_t * y_ch) {
	*x_ch += spaces;
}
static void disp_newlines(disp_ctx_t * ctx, size_t x_level, size_t lines, size_t * x_ch, size_t * y_ch) {
	*x_ch = x_level;
	*y_ch += lines;
}
static void disp_text(disp_ctx_t * ctx, pla_edit_col_type_t col, size_t text_size, const wchar_t * text, size_t * x_ch, size_t * y_ch) {
	SetTextColor(ctx->hdc, ctx->data->style.cols[col].cr);

	TextOutW(ctx->hdc, (int)(*x_ch * ctx->font_w), (int)(*y_ch * ctx->font_h), text, (int)text_size);
	
	*x_ch += text_size;
}
static void disp_keyw(disp_ctx_t * ctx, pla_keyw_t keyw, size_t * x_ch, size_t * y_ch) {
	const ul_ros_t * text = &pla_keyw_strs[keyw];

	disp_text(ctx, PlaEditColKeyw, text->size, text->str, x_ch, y_ch);
}
static void disp_punc(disp_ctx_t * ctx, pla_punc_t punc, size_t * x_ch, size_t * y_ch) {
	const ul_ros_t * text = &pla_punc_strs[punc];

	disp_text(ctx, PlaEditColPlain, text->size, text->str, x_ch, y_ch);
}
static void disp_ident(disp_ctx_t * ctx, ul_hs_t * ident, size_t * x_ch, size_t * y_ch) {
	disp_text(ctx, PlaEditColPlain, ident->size, ident->str, x_ch, y_ch);
}
static void disp_dclr_nspc(disp_ctx_t * ctx, pla_dclr_t * nspc, size_t * x_ch, size_t * y_ch) {
	size_t x_level = *x_ch;

	disp_keyw(ctx, PlaKeywNamespace, x_ch, y_ch);
	disp_spaces(ctx, 1, x_ch, y_ch);
	disp_ident(ctx, nspc->name, x_ch, y_ch);
	disp_spaces(ctx, 1, x_ch, y_ch);
	disp_punc(ctx, PlaPuncLeBrace, x_ch, y_ch);

	disp_newlines(ctx, x_level, 2, x_ch, y_ch);
	disp_punc(ctx, PlaPuncRiBrace, x_ch, y_ch);

	disp_newlines(ctx, x_level, 1, x_ch, y_ch);
}
static void disp_test(wnd_data_t * data, HDC hdc) {
	disp_ctx_t ctx = { .data = data, hdc = hdc };

	ctx.hw = data->hw;
	ctx.wa_ctx = data->ctx;
	GetClientRect(ctx.hw, &ctx.rect);

	FillRect(hdc, &ctx.rect, ctx.wa_ctx->style.cols[WaStyleColBg].hb);

	SelectObject(hdc, data->style.font.hf);

	{
		TEXTMETRICW tm;

		GetTextMetricsW(hdc, &tm);

		ctx.font_h = tm.tmAscent;
		ctx.font_w = tm.tmAveCharWidth;
	}
	
	{
		size_t x_ch = 0, y_ch = 0;

		pla_dclr_t nspc_example = { .type = PlaDclrNspc, .name = UL_HST_HASHADD_WS(data->ctx->hst, L"pla_lib") };

		disp_dclr_nspc(&ctx, &nspc_example, &x_ch, &y_ch);
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

static LRESULT wnd_proc_data(wnd_data_t * data, UINT msg, WPARAM wp, LPARAM lp) {
	HWND hw = data->hw;
	wa_ctx_t * ctx = data->ctx;

	switch (msg) {
		case WM_CREATE:
			if (!process_cd_args(data, wa_wnd_get_cd(lp))) {
				return -1;
			}
			break;
		case WM_PAINT:
		{
			PAINTSTRUCT ps;

			HDC hdc = BeginPaint(hw, &ps);

			disp_test(data, hdc);

			EndPaint(hw, &ps);

			return TRUE;
		}
		case WM_ERASEBKGND:
			return FALSE;
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
			}
			break;
		case WM_NCDESTROY:
			if (data != NULL) {
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

	desc.font.lfHeight = -14;

	return desc;
}