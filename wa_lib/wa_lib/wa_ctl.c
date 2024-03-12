#include "pch.h"
#include "wa_ctl.h"

bool wa_ctl_set_data(HWND hw, wa_ctl_data_t * ctl_data) {
	if (SetPropW(hw, WA_CTL_DATA_PROP_NAME, (HANDLE)ctl_data) == 0) {
		return false;
	}

	return true;
}
wa_ctl_data_t * wa_ctl_get_data(HWND hw) {
	return (wa_ctl_data_t *)GetPropW(hw, WA_CTL_DATA_PROP_NAME);
}
void wa_ctl_remove_data(HWND hw) {
	RemovePropW(hw, WA_CTL_DATA_PROP_NAME);
}

LONG wa_ctl_get_parent_w(void * ctl_ptr, RECT * parent_rect) {
	return parent_rect->right - parent_rect->left;
}
LONG wa_ctl_get_parent_h(void * ctl_ptr, RECT * parent_rect) {
	return parent_rect->bottom - parent_rect->top;
}
