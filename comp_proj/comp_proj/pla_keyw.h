#pragma once
#include "pla.h"
#include "u_ros.h"

enum pla_keyw {
	PlaKeywNone,
#define PLA_KEYW(name, str) PlaKeyw##name,
#include "pla_keyw.def"
#undef PLA_KEYW
	PlaKeyw_Count
};

pla_keyw_t pla_keyw_fetch_exact(size_t str_size, wchar_t * str);

const u_ros_t pla_keyw_strs[PlaKeyw_Count];
