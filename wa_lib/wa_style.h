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

	size_t f_w;
	size_t f_h;
};
struct wa_style {
	wa_style_col_t cols[WaStyleCol_Count];
	wa_style_font_t fonts[WaStyleFont_Count];

	size_t sep_size;
};

WA_API bool wa_style_col_set(wa_style_col_t * col, COLORREF cr);
WA_API void wa_style_col_cleanup(wa_style_col_t * col);

WA_API bool wa_style_font_set(wa_style_font_t * font, LOGFONTW * lf);
WA_API void wa_style_font_cleanup(wa_style_font_t * font);

WA_API void wa_style_cleanup(wa_style_t * style);

WA_API bool wa_style_init_dflt(wa_style_t * style);

WA_API bool wa_style_get_font_metric(HFONT hf, TEXTMETRICW * out);
