#pragma once
#include "wa.h"

#define WA_HOST_WND_CLS_NAME L"wa_host"

WA_SYMBOL WNDCLASSEXW wa_host_get_wnd_cls_desc();

WA_SYMBOL extern const wa_wnd_sp_t wa_host_dflt_sp;
