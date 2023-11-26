#pragma once
#include "pla.h"
#include "u_ros.h"

enum pla_pds {
#define PLA_PDS(name, str) PlaPds##name,
#include "pla_pds.def"
#undef PLA_PDS
	PlaPds_Count
};

const u_ros_t pla_pds_strs[PlaPds_Count];
