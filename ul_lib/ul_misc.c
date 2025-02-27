#include "ul_misc.h"

#if defined __linux__
extern inline size_t ul_align_biased_to(size_t val, size_t align, size_t bias);
extern inline size_t ul_align_to(size_t val, size_t align);
extern inline bool ul_is_pwr_of_2(size_t val);
#endif
