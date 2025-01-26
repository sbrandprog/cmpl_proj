#pragma once
#include "pla.h"

enum pla_punc
{
    PlaPuncNone,
#define PLA_PUNC(name, str) PlaPunc##name,
#include "pla_punc.data"
#undef PLA_PUNC
    PlaPunc_Count
};

PLA_API pla_punc_t pla_punc_fetch_best(size_t str_size, char * str, size_t * punc_len);

extern PLA_API const ul_ros_t pla_punc_strs[PlaPunc_Count];
