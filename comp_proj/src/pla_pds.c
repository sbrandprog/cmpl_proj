#include "pch.h"
#include "pla_pds.h"

const u_ros_t pla_pds_strs[PlaPds_Count] = {
#define PLA_PDS(name, str_) [PlaPds##name] = { .size = _countof(str_) - 1, .str = str_ },
#include "pla_pds.def"
#undef PLA_PDS
};