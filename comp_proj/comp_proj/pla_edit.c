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

	size_t cur_x;
	size_t cur_y;
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

static void disp_spaces(disp_ctx_t * ctx, size_t spaces) {
	ctx->cur_x += spaces;
}
static void disp_newlines(disp_ctx_t * ctx, size_t x_level, size_t lines) {
	ctx->cur_x = x_level;
	ctx->cur_y += lines;
}
static void disp_ch(disp_ctx_t * ctx, pla_edit_col_type_t col, wchar_t ch) {
	SetTextColor(ctx->hdc, ctx->data->style.cols[col].cr);

	TextOutW(ctx->hdc, (int)(ctx->cur_x * ctx->font_w), (int)(ctx->cur_y * ctx->font_h), &ch, 1);

	ctx->cur_x += 1;
}
static void disp_text(disp_ctx_t * ctx, pla_edit_col_type_t col, size_t text_size, const wchar_t * text) {
	SetTextColor(ctx->hdc, ctx->data->style.cols[col].cr);

	TextOutW(ctx->hdc, (int)(ctx->cur_x * ctx->font_w), (int)(ctx->cur_y * ctx->font_h), text, (int)text_size);
	
	ctx->cur_x += text_size;
}
static void disp_ros(disp_ctx_t * ctx, pla_edit_col_type_t col, const ul_ros_t * ros) {
	disp_text(ctx, col, ros->size, ros->str);
}
static void disp_hs(disp_ctx_t * ctx, pla_edit_col_type_t col, ul_hs_t * hs) {
	disp_text(ctx, col, hs->size, hs->str);
}

static void disp_keyw(disp_ctx_t * ctx, pla_keyw_t keyw) {
	disp_ros(ctx, PlaEditColKeyw, &pla_keyw_strs[keyw]);
}
static void disp_punc(disp_ctx_t * ctx, pla_punc_t punc) {
	disp_ros(ctx, PlaEditColPlain, &pla_punc_strs[punc]);
}
static void disp_ident(disp_ctx_t * ctx, ul_hs_t * ident) {
	disp_hs(ctx, PlaEditColPlain, ident);
}

static void disp_dclr_nspc(disp_ctx_t * ctx, pla_dclr_t * nspc) {
	size_t x_level = ctx->cur_x;

	disp_keyw(ctx, PlaKeywNamespace);
	disp_spaces(ctx, 1);
	disp_ident(ctx, nspc->name);
	disp_spaces(ctx, 1);
	disp_punc(ctx, PlaPuncLeBrace);

	disp_newlines(ctx, x_level, 2);
	disp_punc(ctx, PlaPuncRiBrace);

	disp_newlines(ctx, x_level, 1);
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

		ctx.font_h = tm.tmHeight;
		ctx.font_w = tm.tmAveCharWidth;
	}
	
	{
		pla_dclr_t nspc_example = { .type = PlaDclrNspc, .name = UL_HST_HASHADD_WS(data->ctx->hst, L"pla_lib") };

		disp_dclr_nspc(&ctx, &nspc_example);
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

	desc.font.lfHeight = -16;

	return desc;
}