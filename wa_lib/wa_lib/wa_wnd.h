#pragma once
#include "wa.h"

struct wa_wnd_sp {
	int x, y, w, h;
};

struct wa_wnd_cd {
	wa_ctx_t * ctx;
	wa_wnd_sp_t sp;
	ul_hs_t * text;
	HFONT hf;
};

WA_SYMBOL bool wa_wnd_cd_fill_va(wa_wnd_cd_t * cd, va_list args);
WA_SYMBOL bool wa_wnd_cd_fill(wa_wnd_cd_t * cd, ...);

WA_SYMBOL HWND wa_wnd_create(wa_ctx_t * ctx, const wchar_t * wnd_cls_name, HWND parent_hw, DWORD style, DWORD style_ex, ...);

WA_SYMBOL wa_wnd_cd_t * wa_wnd_get_cd(LPARAM lp);

WA_SYMBOL void wa_wnd_set_fp(HWND hw, void * fp);
WA_SYMBOL void * wa_wnd_get_fp(HWND hw);

