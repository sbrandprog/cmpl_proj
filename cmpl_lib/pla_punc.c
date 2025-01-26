#include "pla_punc.h"

pla_punc_t pla_punc_fetch_best(size_t str_size, char * str, size_t * punc_len)
{
    pla_punc_t best = PlaPuncNone;
    size_t best_len = 0;

    for (pla_punc_t p = PlaPuncNone + 1; p < PlaPunc_Count; ++p)
    {
        const ul_ros_t * p_str = &pla_punc_strs[p];

        if (best_len < p_str->size && p_str->size <= str_size
            && strncmp(p_str->str, str, p_str->size) == 0)
        {
            best = p;
            best_len = p_str->size;
        }
    }

    *punc_len = best_len;

    return best;
}

const ul_ros_t pla_punc_strs[PlaPunc_Count] = {
    [PlaPuncNone] = { .size = 0, .str = NULL },
#define PLA_PUNC(name, str_) [PlaPunc##name] = UL_ROS_MAKE(str_),
#include "pla_punc.data"
#undef PLA_PUNC
};
