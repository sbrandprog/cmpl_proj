#include "ira_pds.h"

const ul_ros_t ira_pds_strs[IraPds_Count] = {
#define IRA_PDS(name, str_) [IraPds##name] = UL_ROS_MAKE(str_),
#include "ira_pds.data"
#undef IRA_PDS
};