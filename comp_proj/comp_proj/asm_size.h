#pragma once
#include "asm.h"

enum asm_size {
	AsmSizeNone,
	AsmSize8,
	AsmSize16,
	AsmSize32,
	AsmSize64,
	AsmSize_Count
};
struct asm_size_info {
	bool is_int;
	size_t bytes;
};

extern const asm_size_info_t asm_size_infos[AsmSize_Count];
