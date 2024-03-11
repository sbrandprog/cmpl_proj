#include "pch.h"
#include "wa_ctx.h"

bool wa_ctx_init(wa_ctx_t * ctx, ul_hst_t * hst) {
	*ctx = (wa_ctx_t){ .hst = hst };

	ul_hsb_init(&ctx->hsb);

	ctx->itnc = GetModuleHandleW(NULL);

	if (ctx->itnc == NULL) {
		return false;
	}

	{
		STARTUPINFOW si;

		GetStartupInfoW(&si);

		ctx->show_cmd = si.dwFlags & STARTF_USESHOWWINDOW ? si.wShowWindow : SW_SHOWDEFAULT;
	}

	if (!wa_style_init_dflt(&ctx->style)) {
		return false;
	}

	return true;
}
void wa_ctx_cleanup(wa_ctx_t * ctx) {
	ul_hsb_cleanup(&ctx->hsb);

	memset(ctx, 0, sizeof(*ctx));
}

void wa_ctx_use_show_cmd(wa_ctx_t * ctx, HWND hw) {
	ShowWindow(hw, (int)ctx->show_cmd);
}

void wa_ctx_run_msg_loop(wa_ctx_t * ctx) {
	MSG msg;
	BOOL res;

	while ((res = GetMessageW(&msg, NULL, 0, 0)) != 0) {
		if (res < 0) {
			break;
		}

		TranslateMessage(&msg);

		DispatchMessageW(&msg);
	}
}

void wa_ctx_inc_twc(wa_ctx_t * ctx) {
	InterlockedIncrement64((long long *)&ctx->twc);
}
void wa_ctx_dec_twc(wa_ctx_t * ctx) {
	if (InterlockedDecrement64((long long *)&ctx->twc) == 0) {
		PostQuitMessage(0);
	}
}
