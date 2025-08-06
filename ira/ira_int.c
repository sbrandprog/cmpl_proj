#include "ira_int.h"

const ira_int_info_t ira_int_infos[IraInt_Count] = {
    [IraIntU8] = { .sign = false, .size = 1, .mc_size = McSize8, .mc_imm_type = McInstImm8, .max = UINT8_MAX },
    [IraIntU16] = { .sign = false, .size = 2, .mc_size = McSize16, .mc_imm_type = McInstImm16, .max = UINT16_MAX },
    [IraIntU32] = { .sign = false, .size = 4, .mc_size = McSize32, .mc_imm_type = McInstImm32, .max = UINT32_MAX },
    [IraIntU64] = { .sign = false, .size = 8, .mc_size = McSize64, .mc_imm_type = McInstImm64, .max = UINT64_MAX },
    [IraIntS8] = { .sign = true, .size = 1, .mc_size = McSize8, .mc_imm_type = McInstImm8, .max = INT8_MAX },
    [IraIntS16] = { .sign = true, .size = 2, .mc_size = McSize16, .mc_imm_type = McInstImm16, .max = INT16_MAX },
    [IraIntS32] = { .sign = true, .size = 4, .mc_size = McSize32, .mc_imm_type = McInstImm32, .max = INT32_MAX },
    [IraIntS64] = { .sign = true, .size = 8, .mc_size = McSize64, .mc_imm_type = McInstImm64, .max = INT64_MAX }
};
