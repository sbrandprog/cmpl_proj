#include "pch.h"
#include "asm_size.h"
#include "u_assert.h"

bool asm_size_is_int(asm_size_t size) {
	switch (size) {
		case AsmSize8:
		case AsmSize16:
		case AsmSize32:
		case AsmSize64:
			return true;
	}

	return false;
}

size_t asm_size_get_bytes(asm_size_t size) {
	switch (size) {
		case AsmSizeNone:
			return 0;
		case AsmSize8:
			return 1;
		case AsmSize16:
			return 2;
		case AsmSize32:
			return 4;
		case AsmSize64:
			return 8;
		default:
			u_assert_switch(size);
	}
}