#include "ul_bs.h"

size_t ul_bs_lower_bound(size_t elem_size, size_t elems_size, const void * elems, ul_bs_cmp_proc_t * cmp_proc, const void * trg) {
	size_t i = 0, j = elems_size;

	while (i < j) {
		size_t k = i + (j - i) / 2;

		if (cmp_proc((uint8_t *)elems + elem_size * k, trg)) {
			i = k + 1;
		}
		else {
			j = k;
		}
	}

	return i;
}
size_t ul_bs_upper_bound(size_t elem_size, size_t elems_size, const void * elems, ul_bs_cmp_proc_t * cmp_proc, const void * trg) {
	size_t i = 0, j = elems_size;

	while (i < j) {
		size_t k = i + (j - i) / 2;

		if (!cmp_proc(trg, (uint8_t *)elems + elem_size * k)) {
			i = k + 1;
		}
		else {
			j = k;
		}
	}

	return i;
}
