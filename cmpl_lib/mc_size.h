#pragma once
#include "mc.h"

enum mc_size {
	McSizeNone,
	McSize8,
	McSize16,
	McSize32,
	McSize64,
	McSize_Count
};
struct mc_size_info {
	bool is_int;
	size_t bytes;
};

extern MC_API const mc_size_info_t mc_size_infos[McSize_Count];
