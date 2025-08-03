#pragma once
#include "ul.h"

UL_API size_t ul_bs_lower_bound(size_t elem_size, size_t elems_size, const void * elems, ul_bs_cmp_proc_t * cmp_proc, const void * trg);
UL_API size_t ul_bs_upper_bound(size_t elem_size, size_t elems_size, const void * elems, ul_bs_cmp_proc_t * cmp_proc, const void * trg);
