#pragma once
#include "pla.h"

#define PLA_EDIT_WND_CLS_NAME L"pla_edit"

#define pla_edit_prop_style_desc L"style_desc"

enum pla_edit_col {
	PlaEditColPlain,
	PlaEditColKeyw,
	PlaEditCol_Count
};
struct pla_edit_style_desc {
	COLORREF cols[PlaEditCol_Count];
	LOGFONT font;
};

WNDCLASSEXW pla_edit_get_wnd_cls_desc();

pla_edit_style_desc_t pla_edit_get_style_desc_dflt(wa_ctx_t * ctx);
