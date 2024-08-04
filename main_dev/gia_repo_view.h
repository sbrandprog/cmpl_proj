#pragma once
#include "gia.h"

#define GIA_REPO_VIEW_WND_CLS_NAME L"gia_repo_view"

WNDCLASSEX gia_repo_view_get_wnd_cls_desc();

pla_repo_lsnr_t * gia_repo_view_get_lsnr(HWND hw);
