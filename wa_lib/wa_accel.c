#include "wa_accel.h"

bool wa_accel_set(HWND hw, HACCEL ha) {
	if (SetPropW(hw, WA_ACCEL_PROP_NAME, (HANDLE)ha) == 0) {
		return false;
	}

	return true;
}
HACCEL wa_accel_get(HWND hw) {
	return (HACCEL)GetPropW(hw, WA_ACCEL_PROP_NAME);
}
void wa_accel_remove(HWND hw) {
	RemovePropW(hw, WA_ACCEL_PROP_NAME);
}

bool wa_accel_load(HWND hw, size_t accels_size, const ACCEL * accels) {
	HACCEL ha = CreateAcceleratorTableW((LPACCEL)accels, (int)accels_size);

	if (ha == NULL) {
		return false;
	}

	if (!wa_accel_set(hw, ha)) {
		DestroyAcceleratorTable(ha);
		return false;
	}

	return true;
}
void wa_accel_unload(HWND hw) {
	HACCEL ha = wa_accel_get(hw);

	if (ha == NULL) {
		return;
	}

	wa_accel_remove(hw);

	DestroyAcceleratorTable(ha);
}
