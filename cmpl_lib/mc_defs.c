#include "mc_defs.h"

#define SECT_INV_NAME ".inv"
#define SECT_TEXT_NAME ".text"
#define SECT_RDATA_NAME ".rdata"
#define SECT_DATA_NAME ".data"

const lnk_sect_desc_t mc_defs_sds[McDefsSd_Count] = {
	[McDefsSdText_Code] = { .name = SECT_INV_NAME, .ord_ind = SIZE_MAX, .data_align = 16, .data_align_byte = 0x00 },
	[McDefsSdText_Code] = { .name = SECT_TEXT_NAME, .ord_ind = 0, .data_align = 16, .data_align_byte = 0xCC, .mem_r = true, .mem_e = true },
	[McDefsSdRdata_ItAddr] = { .name = SECT_RDATA_NAME, .ord_ind = 0, .data_align = 16, .data_align_byte = 0x00, .mem_r = true },
	[McDefsSdRdata_Data] = { .name = SECT_RDATA_NAME, .ord_ind = 1, .data_align = 16, .data_align_byte = 0x00, .mem_r = true },
	[McDefsSdRdata_Unw] = { .name = SECT_RDATA_NAME, .ord_ind = 2, .data_align = 4, .data_align_byte = 0x00, .mem_r = true },
	[McDefsSdRdata_ItDir] = { .name = SECT_RDATA_NAME, .ord_ind = 3, .data_align = 16, .data_align_byte = 0x00, .mem_r = true },
	[McDefsSdRdata_ItHnt] = { .name = SECT_RDATA_NAME, .ord_ind = 4, .data_align = 16, .data_align_byte = 0x00, .mem_r = true },
	[McDefsSdData_Data] = { .name = SECT_DATA_NAME, .ord_ind = 0, .data_align = 16, .data_align_byte = 0x00, .mem_r = true, .mem_w = true }
};
