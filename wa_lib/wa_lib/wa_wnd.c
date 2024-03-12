#include "pch.h"
#include "wa_ctx.h"
#include "wa_wnd.h"

HWND wa_wnd_create(wa_ctx_t * ctx, const wchar_t * wnd_cls_name, HWND parent_hw, DWORD style, DWORD style_ex, ...) {
	wa_wnd_cd_t cd = { .ctx = ctx };

	va_start(cd.args, style_ex);

	HWND hw = CreateWindowExW(style_ex, wnd_cls_name, NULL, style, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, parent_hw, NULL, ctx->itnc, &cd);
	
	va_end(cd.args);

	return hw;
}

bool wa_wnd_check_cls(HWND hw, const wchar_t * cls_name) {
	wchar_t buf[WA_WND_CHECK_CLS_NAME_SIZE_MAX + 1];

	size_t cls_name_size = wcslen(cls_name);

	ul_raise_assert(_countof(buf) > cls_name_size);

	int res = GetClassNameW(hw, buf, _countof(buf));

	if (res == 0 || (size_t)res != cls_name_size) {
		return false;
	}

	return wcscmp(buf, cls_name) == 0 ? true : false;
}

wa_wnd_cd_t * wa_wnd_get_cd(LPARAM lp) {
	return ((LPCREATESTRUCTW)lp)->lpCreateParams;
}

void wa_wnd_set_fp(HWND hw, void * fp) {
	SetWindowLongPtrW(hw, 0, (LONG_PTR)fp);
}
void * wa_wnd_get_fp(HWND hw) {
	return (void *)GetWindowLongPtrW(hw, 0);
}
