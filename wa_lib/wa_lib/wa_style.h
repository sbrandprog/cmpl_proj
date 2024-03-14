#pragma once
#include "wa.h"

#define WA_STYLE_DFLT_FXD_FONT L"Cascadia Mono"

enum wa_style_col_type {
	WaStyleColBg,
	WaStyleColFg,
	WaStyleColHvrBg,
	WaStyleColHvrFg,
	WaStyleColPshBg,
	WaStyleColPshFg,
	WaStyleCol_Count
};
struct wa_style_col {
	COLORREF cr;
	HBRUSH hb;
};
enum wa_style_font_type {
	WaStyleFontCpt,
	WaStyleFontFxd,
	WaStyleFont_Count
};
struct wa_style_font {
	LOGFONTW lf;
	HFONT hf;
};
struct wa_style {
	wa_style_col_t cols[WaStyleCol_Count];
	wa_style_font_t fonts[WaStyleFont_Count];
};

WA_SYMBOL bool wa_style_col_set(wa_style_col_t * col, COLORREF cr);
WA_SYMBOL void wa_style_col_cleanup(wa_style_col_t * col);

WA_SYMBOL bool wa_style_font_set(wa_style_font_t * font, LOGFONTW * lf);
WA_SYMBOL void wa_style_font_cleanup(wa_style_font_t * font);

WA_SYMBOL void wa_style_cleanup(wa_style_t * style);

WA_SYMBOL bool wa_style_init_dflt(wa_style_t * style);

WA_SYMBOL TEXTMETRICW wa_style_get_font_metric(HFONT hf);
WA_SYMBOL LONG wa_style_get_font_str_w(HFONT hf, size_t str_size, wchar_t * str);
inline LONG wa_style_get_font_h(HFONT hf) {
	return wa_style_get_font_metric(hf).tmHeight;
}
