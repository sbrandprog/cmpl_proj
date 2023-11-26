#include "pch.h"
#include "ira_int.h"
#include "u_assert.h"

size_t ira_int_get_size(ira_int_type_t type) {
	switch (type) {
		case IraIntU8:
		case IraIntS8:
			return 1;
		case IraIntU16:
		case IraIntS16:
			return 2;
		case IraIntU32:
		case IraIntS32:
			return 4;
		case IraIntU64:
		case IraIntS64:
			return 8;
		default:
			return 0;
	}
}

uint64_t ira_int_get_max_pos(ira_int_type_t type) {
	switch (type) {
		case IraIntU8:
			return UINT8_MAX;
		case IraIntS8:
			return INT8_MAX;
		case IraIntU16:
			return UINT16_MAX;
		case IraIntS16:
			return INT16_MAX;
		case IraIntU32:
			return UINT32_MAX;
		case IraIntS32:
			return INT32_MAX;
		case IraIntU64:
			return UINT64_MAX;
		case IraIntS64:
			return INT64_MAX;
		default:
			return 0;
	}
}