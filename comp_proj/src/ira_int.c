#include "pch.h"
#include "ira_int.h"
#include "asm_inst.h"

const ira_int_info_t ira_int_infos[IraInt_Count] = {
	[IraIntU8] = { .sign = false, .imm_type = AsmInstImm8, .size = 1, .max = UINT8_MAX },
	[IraIntU16] = { .sign = false, .imm_type = AsmInstImm16, .size = 2, .max = UINT16_MAX },
	[IraIntU32] = { .sign = false, .imm_type = AsmInstImm32, .size = 4, .max = UINT32_MAX },
	[IraIntU64] = { .sign = false, .imm_type = AsmInstImm64, .size = 8, .max = UINT64_MAX },
	[IraIntS8] = { .sign = true, .imm_type = AsmInstImm8, .size = 1, .max = INT8_MAX },
	[IraIntS16] = { .sign = true, .imm_type = AsmInstImm16, .size = 2, .max = INT16_MAX },
	[IraIntS32] = { .sign = true, .imm_type = AsmInstImm32, .size = 4, .max = INT32_MAX },
	[IraIntS64] = { .sign = true, .imm_type = AsmInstImm64, .size = 8, .max = INT64_MAX }
};
