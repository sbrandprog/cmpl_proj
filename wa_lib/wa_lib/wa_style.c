#include "pch.h"
#include "wa_style.h"

bool wa_style_col_set(wa_style_col_t * col, COLORREF cr) {
	HBRUSH new_hb = CreateSolidBrush(cr);

	if (new_hb == NULL) {
		return false;
	}

	wa_style_col_cleanup(col);

	col->cr = cr;
	col->hb = new_hb;

	return true;
}
void wa_style_col_cleanup(wa_style_col_t * col) {
	DeleteBrush(col->hb);

	memset(col, 0, sizeof(*col));
}

bool wa_style_font_set(wa_style_font_t * font, LOGFONTW * lf) {
	HFONT new_hf = CreateFontIndirectW(lf);

	if (new_hf == NULL) {
		return false;
	}

	wa_style_font_cleanup(font);

	font->lf = *lf;
	font->hf = new_hf;

	return true;
}
void wa_style_font_cleanup(wa_style_font_t * font) {
	DeleteFont(font->hf);

	memset(font, 0, sizeof(*font));
}

void wa_style_cleanup(wa_style_t * style) {
	for (wa_style_col_type_t col = 0; col < WaStyleCol_Count; ++col) {
		wa_style_col_cleanup(&style->cols[col]);
	}

	for (wa_style_font_type_t font = 0; font < WaStyleFont_Count; ++font) {
		wa_style_font_cleanup(&style->fonts[font]);
	}

	memset(style, 0, sizeof(*style));
}

static bool init_dflt_cols(wa_style_t * style) {
	COLORREF refs[WaStyleCol_Count] = {
		[WaStyleColBg] = RGB(255, 255, 255),
		[WaStyleColFg] = RGB(0, 0, 0),
		[WaStyleColHvrBg] = RGB(182, 225, 243),
		[WaStyleColHvrFg] = RGB(0, 0, 0),
		[WaStyleColPshBg] = RGB(95, 195, 238),
		[WaStyleColPshFg] = RGB(0, 0, 0),
	};

	for (wa_style_col_type_t col = 0; col < WaStyleCol_Count; ++col) {
		if (!wa_style_col_set(&style->cols[col], refs[col])) {
			return false;
		}
	}

	return true;
}
static bool init_dflt_fonts(wa_style_t * style) {
	NONCLIENTMETRICSW ncm = { .cbSize = sizeof(ncm) };

	if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, 0, &ncm, 0) == 0) {
		return false;
	}

	LOGFONT lf = ncm.lfCaptionFont;

	if (!wa_style_font_set(&style->fonts[WaStyleFontCpt], &lf)) {
		return false;
	}

	static const ul_ros_t dflt_fxd_font = UL_ROS_MAKE(WA_STYLE_DFLT_FXD_FONT);

	wmemcpy_s(lf.lfFaceName, LF_FACESIZE, dflt_fxd_font.str, dflt_fxd_font.size);

	if (!wa_style_font_set(&style->fonts[WaStyleFontFxd], &lf)) {
		return false;
	}

	return true;
}
bool wa_style_init_dflt(wa_style_t * style) {
	memset(style, 0, sizeof(*style));

	if (!init_dflt_cols(style)) {
		return false;
	}

	if (!init_dflt_fonts(style)) {
		return false;
	}

	return true;
}

TEXTMETRICW wa_style_get_font_metric(HFONT hf) {
	HDC hdc = GetDC(NULL);

	SelectFont(hdc, hf);

	TEXTMETRICW tm;

	GetTextMetricsW(hdc, &tm);

	ReleaseDC(NULL, hdc);

	return tm;
}
LONG wa_style_get_font_str_w(HFONT hf, size_t str_size, wchar_t * str) {
	HDC hdc = GetDC(NULL);

	SelectFont(hdc, hf);

	RECT text_rect = { 0 };

	DrawTextExW(hdc, str, (int)str_size, &text_rect, DT_SINGLELINE | DT_NOCLIP | DT_CALCRECT, NULL);

	ReleaseDC(NULL, hdc);

	return text_rect.right;
}
