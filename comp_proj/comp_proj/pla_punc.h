#pragma once
#include "pla.h"
#include "u_ros.h"

enum pla_punc {
	PlaPuncNone,
#define PLA_PUNC(name, str) PlaPunc##name,
#include "pla_punc.def"
#undef PLA_PUNC
	PlaPunc_Count
};

pla_punc_t pla_punc_fetch_best(size_t str_size, wchar_t * str, size_t * punc_len);

const u_ros_t pla_punc_strs[PlaPunc_Count];

