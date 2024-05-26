#pragma once
#include "pla.h"
#include "gia.h"

#define GIA_ECV_WND_CLS_NAME L"gia_ecv"

WNDCLASSEX gia_ecv_get_wnd_cls_desc();

pla_ec_rcvr_t * gia_ecv_get_ec_rcvr(HWND hw);
