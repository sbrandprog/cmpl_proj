#pragma once
#include "ira.h"
#include "mc.h"

enum ira_int_type {
	IraIntU8,
	IraIntU16,
	IraIntU32,
	IraIntU64,
	IraIntS8,
	IraIntS16,
	IraIntS32,
	IraIntS64,
	IraInt_Count,
	IraInt_First = IraIntU8
};

enum ira_int_cmp {
	IraIntCmpLess,
	IraIntCmpLessEq,
	IraIntCmpEq,
	IraIntCmpGrtrEq,
	IraIntCmpGrtr,
	IraIntCmpNeq,
	IraIntCmp_Count,
};

struct ira_int_info {
	bool sign;
	uint8_t size;
	mc_size_t mc_size;
	mc_inst_imm_type_t mc_imm_type;
	uint64_t max;
};

union ira_int {
	uint8_t ui8;
	uint16_t ui16;
	uint32_t ui32;
	uint64_t ui64;
	int8_t si8;
	int16_t si16;
	int32_t si32;
	int64_t si64;
};

extern IRA_API const ira_int_info_t ira_int_infos[IraInt_Count];
