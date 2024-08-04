#pragma once
#include "wa.h"

#define WA_CTL_DATA_PROP_NAME L"wa_ctl_data"

struct wa_ctl_data {
	void * ctl_ptr;
	wa_ctl_get_w_proc_t * get_w_proc;
	wa_ctl_get_h_proc_t * get_h_proc;
};

WA_SYMBOL bool wa_ctl_set_data(HWND hw, wa_ctl_data_t * ctl_data);
WA_SYMBOL wa_ctl_data_t * wa_ctl_get_data(HWND hw);
WA_SYMBOL void wa_ctl_remove_data(HWND hw);

WA_SYMBOL LONG wa_ctl_get_parent_w(void * ctl_ptr, RECT * parent_rect);
WA_SYMBOL LONG wa_ctl_get_parent_h(void * ctl_ptr, RECT * parent_rect);
