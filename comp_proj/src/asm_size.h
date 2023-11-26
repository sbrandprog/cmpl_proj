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

bool asm_size_is_int(asm_size_t size);

size_t asm_size_get_bytes(asm_size_t size);
