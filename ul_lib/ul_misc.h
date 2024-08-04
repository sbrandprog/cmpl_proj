#pragma once

#define UL_SIZE_T_NUM_SIZE 20

inline size_t ul_align_biased_to(size_t val, size_t align, size_t bias) {
	size_t val_align_1 = val + align - 1;
	return val_align_1 - (val_align_1 - bias) % align;
}

inline size_t ul_align_to(size_t val, size_t align) {
	return ul_align_biased_to(val, align, 0);
}
