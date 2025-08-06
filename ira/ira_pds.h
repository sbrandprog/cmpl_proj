#pragma once
#include "ira.h"

enum ira_pds
{
#define IRA_PDS(name, str) IraPds##name,
#include "ira_pds.data"
#undef IRA_PDS
    IraPds_Count
};

extern IRA_API const ul_ros_t ira_pds_strs[IraPds_Count];
