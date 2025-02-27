#include "ul_arr.h"

#if defined __linux__
extern inline void ul_arr_grow(size_t new_arr_size, size_t * arr_cap, void ** arr, size_t elem_size);
#endif
