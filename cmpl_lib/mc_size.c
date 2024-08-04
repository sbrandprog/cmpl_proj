#include "mc_size.h"

extern const mc_size_info_t mc_size_infos[McSize_Count] = {
	[McSizeNone] = { .is_int = false, .bytes = 0 },
	[McSize8] = { .is_int = true, .bytes = 1 },
	[McSize16] = { .is_int = true, .bytes = 2 },
	[McSize32] = { .is_int = true, .bytes = 4 },
	[McSize64] = { .is_int = true, .bytes = 8 }
};
