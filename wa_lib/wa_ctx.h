#pragma once
#include "wa_style.h"

struct wa_ctx {
	ul_es_ctx_t * es_ctx;
	ul_hst_t * hst;
	ul_hsb_t hsb;

	HINSTANCE itnc;
	WORD show_cmd;

	wa_style_t style;

	uint64_t twc;
};

WA_API bool wa_ctx_init(wa_ctx_t * ctx, ul_es_ctx_t * es_ctx, ul_hst_t * hst);
WA_API void wa_ctx_cleanup(wa_ctx_t * ctx);

WA_API void wa_ctx_use_show_cmd(wa_ctx_t * ctx, HWND hw);

WA_API void wa_ctx_run_msg_loop(wa_ctx_t * ctx);

WA_API void wa_ctx_inc_twc(wa_ctx_t * ctx);
WA_API void wa_ctx_dec_twc(wa_ctx_t * ctx);

WA_API bool wa_ctx_init_buf_paint(wa_ctx_t * ctx);
WA_API void wa_ctx_cleanup_buf_paint(wa_ctx_t * ctx);
