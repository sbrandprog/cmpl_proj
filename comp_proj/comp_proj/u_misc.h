#pragma once
#include "u_assert.h"

#define U_SIZE_T_NUM_SIZE 20

inline size_t u_align_biased_to(size_t val, size_t align, size_t bias) {
	size_t val_align_1 = val + align - 1;
	return val_align_1 - (val_align_1 - bias) % align;
}

inline size_t u_align_to(size_t val, size_t align) {
	return u_align_biased_to(val, align, 0);
}

#ifdef _DEBUG
inline void u_grow_arr_d(size_t * arr_cap, void ** arr, size_t elem_size, size_t add_size, const char * file_name, int line_num) {
	size_t cap = *arr_cap;

	size_t new_cap = cap + max(cap / 2, add_size);

	void * new_arr = _realloc_dbg(*arr, new_cap * elem_size, _NORMAL_BLOCK, file_name, line_num);

	u_assert(new_arr != NULL);

	*arr_cap = new_cap;
	*arr = new_arr;
}
#define u_grow_arr(arr_cap, arr, elem_size, add_size) u_grow_arr_d((arr_cap), (arr), (elem_size), (add_size), __FILE__, __LINE__)
#else
inline void u_grow_arr(size_t * arr_cap, void ** arr, size_t elem_size, size_t add_size) {
	size_t cap = *arr_cap;

	size_t new_cap = cap + max(cap / 2, add_size);

	void * new_arr = realloc(*arr, new_cap * elem_size);

	u_assert(new_arr != NULL);

	*arr_cap = new_cap;
	*arr = new_arr;
}
#endif