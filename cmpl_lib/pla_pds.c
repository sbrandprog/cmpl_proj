#include "pla_pds.h"

const ul_ros_t pla_pds_strs[PlaPds_Count] = {
#define PLA_PDS(name, str_) [PlaPds##name] = UL_ROS_MAKE(str_),
#include "pla_pds.data"
#undef PLA_PDS
};