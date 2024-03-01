#include "pch.h"
#include "ira_int.h"
#include "asm_size.h"
#include "asm_inst.h"

const ira_int_info_t ira_int_infos[IraInt_Count] = {
	[IraIntU8] = { .sign = false, .size = 1, .asm_size = AsmSize8, .asm_imm_type = AsmInstImm8, .max = UINT8_MAX },
	[IraIntU16] = { .sign = false, .size = 2, .asm_size = AsmSize16, .asm_imm_type = AsmInstImm16, .max = UINT16_MAX },
	[IraIntU32] = { .sign = false, .size = 4, .asm_size = AsmSize32, .asm_imm_type = AsmInstImm32, .max = UINT32_MAX },
	[IraIntU64] = { .sign = false, .size = 8, .asm_size = AsmSize64, .asm_imm_type = AsmInstImm64, .max = UINT64_MAX },
	[IraIntS8] = { .sign = true, .size = 1, .asm_size = AsmSize8, .asm_imm_type = AsmInstImm8, .max = INT8_MAX },
	[IraIntS16] = { .sign = true, .size = 2, .asm_size = AsmSize16, .asm_imm_type = AsmInstImm16, .max = INT16_MAX },
	[IraIntS32] = { .sign = true, .size = 4, .asm_size = AsmSize32, .asm_imm_type = AsmInstImm32, .max = INT32_MAX },
	[IraIntS64] = { .sign = true, .size = 8, .asm_size = AsmSize64, .asm_imm_type = AsmInstImm64, .max = INT64_MAX }
};
