#pragma once
#include "gia.h"

#define GIA_EDIT_WND_CLS_NAME L"gia_edit"

#define gia_edit_prop_style_desc L"style_desc"
#define gia_edit_prop_exe_name L"exe_name"
#define gia_edit_prop_repo L"repo"
#define gia_edit_prop_tus L"tus"

enum gia_edit_col_type {
	GiaEditColPlain,
	GiaEditColSel,
	GiaEditCol_Count
};
struct gia_edit_style_desc {
	COLORREF cols[GiaEditCol_Count];
	LOGFONT font;
	float err_line_size;
};

WNDCLASSEXW gia_edit_get_wnd_cls_desc();

gia_edit_style_desc_t gia_edit_get_style_desc_dflt(wa_ctx_t * ctx);
