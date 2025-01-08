#pragma once
#include "ul_assert.h"

inline void ul_arr_grow(size_t new_arr_size, size_t * arr_cap, void ** arr, size_t elem_size) {
	if (new_arr_size > *arr_cap) {
		size_t cap = *arr_cap, add_size = new_arr_size - cap;

		size_t new_cap = cap + max(cap / 2, add_size);

		void * new_arr = realloc(*arr, new_cap * elem_size);

		ul_assert(new_arr != NULL);

		*arr_cap = new_cap;
		*arr = new_arr;
	}
}
