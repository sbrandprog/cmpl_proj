#include "pch.h"
#include "wa_ctx.h"
#include "wa_wnd.h"

#define prop_size_pos L"size_pos"
#define prop_text L"text"
#define prop_hf L"font"

bool wa_wnd_cd_fill_va(wa_wnd_cd_t * cd, va_list args) {
	for (const wchar_t * prop = va_arg(args, const wchar_t *); prop != NULL; prop = va_arg(args, const wchar_t *)) {
		if (wcscmp(prop, prop_size_pos) == 0) {
			cd->sp = va_arg(args, wa_wnd_sp_t);
		}
		else if (wcscmp(prop, prop_text) == 0) {
			cd->text = va_arg(args, ul_hs_t *);
		}
		else if (wcscmp(prop, prop_hf) == 0) {
			cd->hf = va_arg(args, HFONT);
		}
		else {
			return false;
		}
	}

	return true;
}
bool wa_wnd_cd_fill(wa_wnd_cd_t * cd, ...) {
	va_list args;

	va_start(args, cd);

	bool res = wa_wnd_cd_fill_va(cd, args);

	va_end(args);

	return res;
}

HWND wa_wnd_create(wa_ctx_t * ctx, const wchar_t * wnd_cls_name, HWND parent_hw, DWORD style, DWORD style_ex, ...) {
	wa_wnd_cd_t cd = { .ctx = ctx };

	{
		va_list args;

		va_start(args, style_ex);

		bool res = wa_wnd_cd_fill_va(&cd, args);

		va_end(args);

		if (!res) {
			return NULL;
		}
	}

	const wchar_t * wnd_name = cd.text != NULL ? cd.text->str : NULL;

	return CreateWindowExW(style_ex, wnd_cls_name, wnd_name, style, cd.sp.x, cd.sp.y, cd.sp.w, cd.sp.h, parent_hw, NULL, ctx->itnc, &cd);
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
