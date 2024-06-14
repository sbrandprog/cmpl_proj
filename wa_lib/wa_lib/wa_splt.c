#include "pch.h"
#include "wa_ctx.h"
#include "wa_wnd.h"
#include "wa_ctl.h"
#include "wa_splt.h"

typedef struct wa_splt_data {
	HWND hw;
	wa_ctx_t * ctx;
	wa_ctl_data_t ctl_data;

	bool is_hors;

	HCURSOR rsz_cur_hors;
	HCURSOR rsz_cur_vert;

	HWND first;
	HWND second;
	LONG first_size;
	LONG second_size;

	bool is_rsz;
} wnd_data_t;

static void get_sep_rect(wnd_data_t * data, RECT * rect, RECT * out) {
	LONG ctls_size = data->first_size + data->second_size;

	LONG sep_size = (LONG)data->ctx->style.sep_size;

	LONG max_size = (data->is_hors ? rect->bottom : rect->right) - sep_size;
	
	LONG sep_pos = data->first_size * max_size / ctls_size;
	
	*out = *rect;

	if (data->is_hors) {
		out->top = sep_pos;
		out->bottom = sep_pos + sep_size;
	}
	else {
		out->left = sep_pos;
		out->right = sep_pos + sep_size;
	}
}
static void get_rects(wnd_data_t * data, RECT * out_rect, RECT * out_sep_rect) {
	GetClientRect(data->hw, out_rect);

	get_sep_rect(data, out_rect, out_sep_rect);
}
static void resize_ctls_rect(wnd_data_t * data, RECT * rect) {
	RECT sep_rect;

	get_sep_rect(data, rect, &sep_rect);

	if (data->first != NULL) {
		int w = (int)(data->is_hors ? rect->right : sep_rect.left);
		int h = (int)(data->is_hors ? sep_rect.top : rect->bottom);

		SetWindowPos(data->first, NULL, 0, 0, w, h, SWP_NOZORDER);
	}

	if (data->second != NULL) {
		int x = (int)(data->is_hors ? 0 : sep_rect.right);
		int y = (int)(data->is_hors ? sep_rect.bottom : 0);

		SetWindowPos(data->second, NULL, x, y, rect->right - x, rect->bottom - y, SWP_NOZORDER);
	}

	InvalidateRect(data->hw, NULL, FALSE);
}
static void resize_ctls(wnd_data_t * data) {
	RECT rect;

	GetClientRect(data->hw, &rect);

	resize_ctls_rect(data, &rect);
}
static void update_sep_drag_lp(wnd_data_t * data, LPARAM lp) {
	LONG x = (LONG)GET_X_LPARAM(lp), y = (LONG)GET_Y_LPARAM(lp);

	RECT rect;

	GetClientRect(data->hw, &rect);

	LONG max_size = rect.bottom - (LONG)data->ctx->style.sep_size;
	
	if (data->is_hors) {
		y = max(rect.top, min(y, rect.bottom));

		data->first_size = y;
		data->second_size = rect.bottom - y;
	}
	else {
		x = max(rect.left, min(x, rect.right));

		data->first_size = x;
		data->second_size = rect.right - x;
	}

	resize_ctls_rect(data, &rect);
}

static bool process_cd_args(wnd_data_t * data, wa_wnd_cd_t * cd) {
	for (const wchar_t * arg = va_arg(cd->args, const wchar_t *); arg != NULL; arg = va_arg(cd->args, const wchar_t *)) {
		if (wcscmp(arg, wa_splt_prop_is_hors) == 0) {
			data->is_hors = va_arg(cd->args, bool);
		}
		else {
			return false;
		}
	}

	return true;
}

static void redraw_split(void * user_data, HDC hdc, RECT * rect) {
	wnd_data_t * data = user_data;

	wa_ctx_t * ctx = data->ctx;

	RECT sep_rect;

	get_sep_rect(data, rect, &sep_rect);

	FillRect(hdc, rect, ctx->style.cols[WaStyleColBg].hb);

	FillRect(hdc, &sep_rect, ctx->style.cols[WaStyleColFg].hb);
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
		case WM_SIZE:
			resize_ctls(data);
			break;
		case WM_PAINT:
		{
			PAINTSTRUCT ps;

			HDC hdc = BeginPaint(data->hw, &ps);

			wa_wnd_paint_buf(hw, hdc, data, redraw_split);

			EndPaint(data->hw, &ps);
			
			return 0;
		}
		case WM_ERASEBKGND:
			return TRUE;
		case WM_SETCURSOR:
		{
			RECT rect, sep_rect;

			get_rects(data, &rect, &sep_rect);

			POINT cur_pos;

			GetCursorPos(&cur_pos);

			ScreenToClient(data->hw, &cur_pos);

			if (PtInRect(&sep_rect, cur_pos)) {
				SetCursor(data->is_hors ? data->rsz_cur_hors : data->rsz_cur_vert);
				return TRUE;
			}

			break;
		}
		case WM_MOUSEMOVE:
			if (data->is_rsz) {
				update_sep_drag_lp(data, lp);
			}
			break;
		case WM_LBUTTONDOWN:
		{
			SetFocus(NULL);

			RECT rect, sep_rect;

			get_rects(data, &rect, &sep_rect);

			POINT cur_pos = { .x = GET_X_LPARAM(lp), .y = GET_Y_LPARAM(lp) };

			if (PtInRect(&sep_rect, cur_pos) && DragDetect(data->hw, cur_pos)) {
				SetCapture(data->hw);
				data->is_rsz = true;
			}

			break;
		}
		case WM_LBUTTONUP:
			if (GetCapture() == data->hw && data->is_rsz) {
				ReleaseCapture();
				update_sep_drag_lp(data, lp);
				data->is_rsz = false;
			}
			break;
		case WM_PARENTNOTIFY:
			switch (LOWORD(wp)) {
				case WM_CREATE:
				{
					HWND child = (HWND)lp;

					if (data->first == NULL) {
						data->first = child;
					}
					else if (data->second == NULL) {
						data->second = child;
					}
					else {
						ul_assert_unreachable();
					}

					InvalidateRect(data->hw, NULL, FALSE);

					break;
				}
				case WM_DESTROY:
				{
					HWND child = (HWND)lp;

					if (data->first == child) {
						data->first = NULL;
					}
					else if (data->second == child) {
						data->second = NULL;
					}

					InvalidateRect(data->hw, NULL, FALSE);

					break;
				}
			}
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

				*data = (wnd_data_t) {.hw = hw, .ctx = cd->ctx, .ctl_data = { .ctl_ptr = data, .get_w_proc = wa_ctl_get_parent_w, .get_h_proc = wa_ctl_get_parent_h }, .first_size = 1, .second_size = 1 };

				wa_wnd_set_fp(hw, data);

				if (!wa_ctl_set_data(hw, &data->ctl_data)) {
					return false;
				}

				data->rsz_cur_hors = LoadCursorW(NULL, IDC_SIZENS);

				if (data->rsz_cur_hors == NULL) {
					return FALSE;
				}

				data->rsz_cur_vert = LoadCursorW(NULL, IDC_SIZEWE);

				if (data->rsz_cur_vert == NULL) {
					return FALSE;
				}
			}
			break;
		case WM_NCDESTROY:
			if (data != NULL) {
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

WNDCLASSEX wa_splt_get_wnd_cls_desc() {
	WNDCLASSEX wnd_cls_desc = {
		.lpfnWndProc = wnd_proc,
		.style = CS_VREDRAW | CS_HREDRAW | CS_PARENTDC,
		.cbWndExtra = sizeof(wnd_data_t *),
		.lpszClassName = WA_SPLT_WND_CLS_NAME
	};

	return wnd_cls_desc;
}
