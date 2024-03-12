#include "pch.h"
#include "asm_size.h"

extern const asm_size_info_t asm_size_infos[AsmSize_Count] = {
	[AsmSizeNone] = { .is_int = false, .bytes = 0 },
	[AsmSize8] = { .is_int = true, .bytes = 1 },
	[AsmSize16] = { .is_int = true, .bytes = 2 },
	[AsmSize32] = { .is_int = true, .bytes = 4 },
	[AsmSize64] = { .is_int = true, .bytes = 8 }
};
