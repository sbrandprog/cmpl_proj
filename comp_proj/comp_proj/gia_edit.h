#pragma once
#include "gia.h"

#define GIA_EDIT_WND_CLS_NAME L"gia_edit"

#define gia_edit_prop_style_desc L"style_desc"
#define gia_edit_prop_exe_name L"exe_name"
#define gia_edit_prop_prog L"prog"
#define gia_edit_prop_tus L"tus"

enum gia_edit_col {
	GiaEditColPlain,
	GiaEditColKeyw,
	GiaEditCol_Count
};
struct gia_edit_style_desc {
	COLORREF cols[GiaEditCol_Count];
	LOGFONT font;
};

WNDCLASSEXW gia_edit_get_wnd_cls_desc();

gia_edit_style_desc_t gia_edit_get_style_desc_dflt(wa_ctx_t * ctx);
