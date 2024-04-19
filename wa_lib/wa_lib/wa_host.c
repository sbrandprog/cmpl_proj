#include "pch.h"
#include "wa_ctx.h"
#include "wa_wnd.h"
#include "wa_ctl.h"
#include "wa_host.h"

typedef struct wnd_host_data {
	HWND hw;
	wa_ctx_t * ctx;
	HWND child;
} wnd_data_t;

static bool process_cd_args(wnd_data_t * data, wa_wnd_cd_t * cd) {
	HWND hw = data->hw;

	for (const wchar_t * prop_name = va_arg(cd->args, const wchar_t *); prop_name != NULL; prop_name = va_arg(cd->args, const wchar_t *)) {
		if (wcscmp(prop_name, wa_wnd_prop_size) == 0) {
			wa_wnd_size_t new_size = va_arg(cd->args, wa_wnd_size_t);

			SetWindowPos(hw, NULL, 0, 0, new_size.w, new_size.h, SWP_NOMOVE | SWP_NOZORDER);
		}
		else if (wcscmp(prop_name, wa_wnd_prop_pos) == 0) {
			wa_wnd_pos_t new_pos = va_arg(cd->args, wa_wnd_pos_t);

			SetWindowPos(hw, NULL, new_pos.x, new_pos.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		}
		else if (wcscmp(prop_name, wa_wnd_prop_text) == 0) {
			ul_hs_t * text = va_arg(cd->args, ul_hs_t *);

			BOOL res = SetWindowTextW(hw, text->str);
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

			wa_ctx_inc_twc(data->ctx);
			break;
		case WM_DESTROY:
			wa_ctx_dec_twc(data->ctx);
			break;
		case WM_SIZE:
			if (data->child != NULL) {
				int w = LOWORD(lp), h = HIWORD(lp);

				SetWindowPos(data->child, NULL, 0, 0, w, h, SWP_NOZORDER);
			}
			break;
		case WM_PAINT:
		{
			PAINTSTRUCT ps;

			HDC hdc = BeginPaint(hw, &ps);

			FillRect(hdc, &ps.rcPaint, ctx->style.cols[WaStyleColBg].hb);

			EndPaint(hw, &ps);

			return TRUE;
		}
		case WM_ERASEBKGND:
			return TRUE;
		case WM_PARENTNOTIFY:
			switch (LOWORD(wp)) {
				case WM_CREATE:
				{
					HWND child = (HWND)lp;

					if (wa_ctl_get_data(child) != NULL) {
						ul_assert(data->child == NULL);

						data->child = child;
					}

					break;
				}
				case WM_DESTROY:
				{
					HWND child = (HWND)lp;

					if (data->child == child && wa_ctl_get_data(child) != NULL) {
						data->child = NULL;
					}

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

				*data = (wnd_data_t){ .hw = hw, .ctx = cd->ctx };

				wa_wnd_set_fp(hw, data);
			}

			break;
		case WM_NCDESTROY:
			if (data != NULL) {
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

WNDCLASSEXW wa_host_get_wnd_cls_desc() {
	WNDCLASSEXW wnd_cls_desc = {
		.lpfnWndProc = wnd_proc,
		.cbWndExtra = sizeof(wnd_data_t *),
		.lpszClassName = WA_HOST_WND_CLS_NAME
	};

	return wnd_cls_desc;
}

const wa_wnd_size_t wa_host_dflt_size = { .w = 1024, .h = 768 };
