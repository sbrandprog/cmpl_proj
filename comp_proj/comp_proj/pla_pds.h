#pragma once
#include "pla.h"

enum pla_pds {
#define PLA_PDS(name, str) PlaPds##name,
#include "pla_pds.def"
#undef PLA_PDS
	PlaPds_Count
};

const ul_ros_t pla_pds_strs[PlaPds_Count];
