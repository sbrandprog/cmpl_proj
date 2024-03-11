#include "pch.h"
#include "wa_ctx.h"
#include "wa_wnd.h"
#include "wa_host.h"

typedef struct wnd_host_data {
	HWND hw;
	wa_ctx_t * ctx;
} wnd_data_t;

static LRESULT wnd_proc_data(wnd_data_t * data, UINT msg, WPARAM wp, LPARAM lp) {
	HWND hw = data->hw;
	wa_ctx_t * ctx = data->ctx;

	switch (msg) {
		case WM_CREATE:
			wa_ctx_inc_twc(data->ctx);
			break;
		case WM_DESTROY:
			wa_ctx_dec_twc(data->ctx);
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

const wa_wnd_sp_t wa_host_dflt_sp = { .x = CW_USEDEFAULT, .y = CW_USEDEFAULT, .w = 1024, .h = 768 };
