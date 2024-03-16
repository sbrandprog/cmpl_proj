#include "pch.h"
#include "wa_ctx.h"
#include "wa_host.h"
#include "wa_wcr.h"

static wa_wcr_wnd_cls_desc_proc_t * const wnd_cls_desc_procs[] = {
	wa_host_get_wnd_cls_desc
};

static bool register_wnd_cls_nl(wa_wcr_t * wcr, WNDCLASSEXW * wnd_cls_desc) {
	ATOM wnd_cls = RegisterClassExW(wnd_cls_desc);

	if (wnd_cls == 0) {
		return false;
	}

	if (wcr->wnd_clss_size + 1 > wcr->wnd_clss_cap) {
		ul_arr_grow(&wcr->wnd_clss_cap, &wcr->wnd_clss, sizeof(*wcr->wnd_clss), 1);
	}

	wcr->wnd_clss[wcr->wnd_clss_size++] = wnd_cls;

	return true;
}
static bool register_wnd_cls_p_nl(wa_wcr_t * wcr, wa_wcr_wnd_cls_desc_proc_t * proc) {
	WNDCLASSEXW wnd_cls_desc = proc();

	wnd_cls_desc.cbSize = sizeof(wnd_cls_desc);

	if (wnd_cls_desc.hInstance == NULL) {
		wnd_cls_desc.hInstance = wcr->ctx->itnc;
	}

	if (wnd_cls_desc.hCursor == NULL) {
		wnd_cls_desc.hCursor = LoadCursorW(NULL, IDC_ARROW);
	}

	if (!register_wnd_cls_nl(wcr, &wnd_cls_desc)) {
		return false;
	}

	return true;
}

static bool register_dflt_clss(wa_wcr_t * wcr) {
	for (wa_wcr_wnd_cls_desc_proc_t * const * proc = wnd_cls_desc_procs, * const * proc_end = proc + _countof(wnd_cls_desc_procs); proc != proc_end; ++proc) {
		if (!register_wnd_cls_p_nl(wcr, *proc)) {
			return false;
		}
	}

	return true;
}
bool wa_wcr_init(wa_wcr_t * wcr, wa_ctx_t * ctx) {
	*wcr = (wa_wcr_t){ .ctx = ctx };

	InitializeCriticalSection(&wcr->lock);

	if (!register_dflt_clss(wcr)) {
		return false;
	}

	return true;
}
void wa_wcr_cleanup(wa_wcr_t * wcr) {
	DeleteCriticalSection(&wcr->lock);

	for (ATOM * wnd_cls = wcr->wnd_clss, *wnd_cls_end = wnd_cls + wcr->wnd_clss_size; wnd_cls != wnd_cls_end; ++wnd_cls) {
		UnregisterClassW(wnd_cls, wcr->ctx->itnc);
	}

	free(wcr->wnd_clss);

	wcr->wnd_clss_cap = 0;
	wcr->wnd_clss = NULL;
	wcr->wnd_clss_size = 0;

	memset(wcr, 0, sizeof(*wcr));
}

bool wa_wcr_register_d(wa_wcr_t * wcr, WNDCLASSEXW * wnd_cls_desc) {
	EnterCriticalSection(&wcr->lock);

	bool res;

	__try {
		res = register_wnd_cls_nl(wcr, wnd_cls_desc);
	}
	__finally {
		LeaveCriticalSection(&wcr->lock);
	}

	return res;
}
bool wa_wcr_register_p(wa_wcr_t * wcr, wa_wcr_wnd_cls_desc_proc_t * proc) {
	EnterCriticalSection(&wcr->lock);

	bool res;

	__try {
		res = register_wnd_cls_p_nl(wcr, proc);
	}
	__finally {
		LeaveCriticalSection(&wcr->lock);
	}

	return res;
}
