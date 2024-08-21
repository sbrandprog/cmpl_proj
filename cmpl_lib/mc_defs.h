#pragma once
#include "lnk_sect.h"
#include "mc.h"

enum mc_defs_sd_type {
	McDefsSdText_Code,
	McDefsSdRdata_ItAddr,
	McDefsSdRdata_Data,
	McDefsSdRdata_Unw,
	McDefsSdRdata_ItDir,
	McDefsSdRdata_ItHnt,
	McDefsSdData_Data,
	McDefsSd_Count
};

MC_API extern const lnk_sect_desc_t mc_defs_sds[McDefsSd_Count];
