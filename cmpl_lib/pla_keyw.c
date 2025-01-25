#include "pla_keyw.h"

pla_keyw_t pla_keyw_fetch_exact(size_t str_size, wchar_t * str)
{
    for (pla_keyw_t k = PlaKeywNone + 1; k < PlaKeyw_Count; ++k)
    {
        const ul_ros_t * k_str = &pla_keyw_strs[k];

        if (k_str->size == str_size
            && wmemcmp(k_str->str, str, k_str->size) == 0)
        {
            return k;
        }
    }

    return PlaKeywNone;
}

const ul_ros_t pla_keyw_strs[PlaKeyw_Count] = {
    [PlaKeywNone] = { .size = 0, .str = NULL },
#define PLA_KEYW(name, str_) [PlaKeyw##name] = UL_ROS_MAKE(str_),
#include "pla_keyw.data"
#undef PLA_PUNC
};
