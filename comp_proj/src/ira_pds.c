#include "pch.h"
#include "ira_pds.h"

const u_ros_t ira_pds_strs[IraPds_Count] = {
#define IRA_PDS(name, str_) [IraPds##name] = U_MAKE_ROS(str_),
#include "ira_pds.def"
#undef IRA_PDS
};