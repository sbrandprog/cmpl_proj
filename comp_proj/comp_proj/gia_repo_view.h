#pragma once
#include "gia_repo.h"

#define GIA_REPO_VIEW_WND_CLS_NAME L"gia_repo_view"

WNDCLASSEX gia_repo_view_get_wnd_cls_desc();

gia_repo_lsnr_t * gia_repo_view_get_lsnr(HWND hw);
