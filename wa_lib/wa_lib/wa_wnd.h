#pragma once
#include "wa.h"

#define wa_wnd_prop_size L"size"
#define wa_wnd_prop_pos L"size"
#define wa_wnd_prop_text L"text"

#define WA_WND_CHECK_CLS_NAME_SIZE_MAX 63

struct wa_wnd_size {
	int w, h;
};
struct wa_wnd_pos {
	int x, y;
};

struct wa_wnd_cd {
	wa_ctx_t * ctx;
	va_list args;
};

typedef void wa_wnd_paint_proc_t(void * user_data, HDC hdc, RECT * rect);

WA_SYMBOL HWND wa_wnd_create(wa_ctx_t * ctx, const wchar_t * wnd_cls_name, HWND parent_hw, DWORD style, DWORD style_ex, ...);

WA_SYMBOL bool wa_wnd_check_cls(HWND hw, const wchar_t * cls_name);

WA_SYMBOL wa_wnd_cd_t * wa_wnd_get_cd(LPARAM lp);

WA_SYMBOL void wa_wnd_set_fp(HWND hw, void * fp);
WA_SYMBOL void * wa_wnd_get_fp(HWND hw);

WA_SYMBOL void wa_wnd_paint_buf(HWND hw, HDC hdc, void * user_data, wa_wnd_paint_proc_t * paint_proc);
