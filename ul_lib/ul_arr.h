#pragma once
#include "ul_assert.h"

inline void ul_arr_grow(size_t new_arr_size, size_t * arr_cap, void ** arr, size_t elem_size)
{
    size_t cap = *arr_cap;

    if (new_arr_size > cap)
    {
        size_t new_cap = cap + cap / 2;

        if (new_cap < new_arr_size)
        {
            new_cap = new_arr_size;
        }

        void * new_arr = realloc(*arr, new_cap * elem_size);

        ul_assert(new_arr != NULL);

        *arr_cap = new_cap;
        *arr = new_arr;
    }
}
