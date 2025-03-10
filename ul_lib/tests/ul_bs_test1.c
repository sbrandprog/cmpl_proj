#include "ul_lib/ul_lib.h"

static bool int_cmp_proc(const void * first_ptr, const void * second_ptr)
{
    const int * first = first_ptr;
    const int * second = second_ptr;

    return *first < *second;
}

int main()
{
    int arr[] = { -5, -4, -4, -4, -3, -3, -3, -1, 0, 0, 0, 2, 2, 2, 3, 3, 4, 4, 4, 6, 6 };

    for (size_t i = 0; i < ul_arr_count(arr); ++i)
    {
        printf("%3i ", arr[i]);
    }

    putchar('\n');

    for (size_t i = 0; i < ul_arr_count(arr); ++i)
    {
        printf("%3zi ", i);
    }

    putchar('\n');

    int trgs[] = { -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8 };

    for (size_t i = 0; i < ul_arr_count(trgs); ++i)
    {
        int trg = trgs[i];

        size_t lower_i = ul_bs_lower_bound(sizeof(*arr), ul_arr_count(arr), arr, int_cmp_proc, &trg),
               upper_i = ul_bs_upper_bound(sizeof(*arr), ul_arr_count(arr), arr, int_cmp_proc, &trg);

        if (lower_i > 0 && arr[lower_i - 1] > trg)
        {
            return -1;
        }
        else if (upper_i < ul_arr_count(arr) && arr[upper_i] <= trg)
        {
            return -1;
        }

        for (int *first = arr + lower_i, *last = arr + upper_i; first != last; ++first)
        {
            if (*first != trg)
            {
                return -1;
            }
        }

        printf("%3i : %3zi %3zi\n", trg, lower_i, upper_i);
    }

    return 0;
}
