#pragma once
#include "ira.h"

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

size_t ira_int_get_size(ira_int_type_t type);

uint64_t ira_int_get_max_pos(ira_int_type_t type);
