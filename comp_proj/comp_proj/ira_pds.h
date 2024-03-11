#pragma once
#include "ira.h"

enum ira_pds {
#define IRA_PDS(name, str) IraPds##name,
#include "ira_pds.def"
#undef IRA_PDS
	IraPds_Count
};

const ul_ros_t ira_pds_strs[IraPds_Count];