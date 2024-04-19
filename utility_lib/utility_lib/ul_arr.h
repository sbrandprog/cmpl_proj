#pragma once
#include "ul_assert.h"

#ifdef _DEBUG
inline void ul_arr_grow_d(size_t * arr_cap, void ** arr, size_t elem_size, size_t add_size, const char * file_name, int line_num) {
	size_t cap = *arr_cap;

	size_t new_cap = cap + max(cap / 2, add_size);

	void * new_arr = _realloc_dbg(*arr, new_cap * elem_size, _NORMAL_BLOCK, file_name, line_num);

	ul_assert(new_arr != NULL);

	*arr_cap = new_cap;
	*arr = new_arr;
}
#define ul_arr_grow(arr_cap, arr, elem_size, add_size) ul_arr_grow_d((arr_cap), (arr), (elem_size), (add_size), __FILE__, __LINE__)
#else
inline void ul_arr_grow(size_t * arr_cap, void ** arr, size_t elem_size, size_t add_size) {
	size_t cap = *arr_cap;

	size_t new_cap = cap + max(cap / 2, add_size);

	void * new_arr = realloc(*arr, new_cap * elem_size);

	ul_assert(new_arr != NULL);

	*arr_cap = new_cap;
	*arr = new_arr;
}
#endif