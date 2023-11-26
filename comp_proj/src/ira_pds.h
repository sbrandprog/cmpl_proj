#pragma once
#include "ira.h"
#include "u_ros.h"

enum ira_pds {
#define IRA_PDS(name, str) IraPds##name,
#include "ira_pds.def"
#undef IRA_PDS
	IraPds_Count
};

const u_ros_t ira_pds_strs[IraPds_Count];