#pragma once
#include "wa.h"

struct wa_wcr {
	wa_ctx_t * ctx;
	CRITICAL_SECTION lock;
	size_t wnd_clss_cap;
	ATOM * wnd_clss;
	size_t wnd_clss_size;
};

WA_SYMBOL bool wa_wcr_init(wa_wcr_t * wcr, wa_ctx_t * ctx);
WA_SYMBOL void wa_wcr_cleanup(wa_wcr_t * wcr);

WA_SYMBOL bool wa_wcr_register_d(wa_wcr_t * wcr, WNDCLASSEXW * wnd_cls_desc);
WA_SYMBOL bool wa_wcr_register_p(wa_wcr_t * wcr, wa_wcr_wnd_cls_desc_proc_t * proc);
