#include "pch.h"
#include "pla_keyw.h"

pla_keyw_t pla_keyw_fetch_exact(size_t str_size, wchar_t * str) {
	for (pla_keyw_t k = PlaKeywNone + 1; k < PlaKeyw_Count; ++k) {
		const u_ros_t * k_str = &pla_keyw_strs[k];

		if (k_str->size == str_size
			&& wmemcmp(k_str->str, str, k_str->size) == 0) {
			return k;
		}
	}

	return PlaKeywNone;
}

const u_ros_t pla_keyw_strs[PlaKeyw_Count] = {
#define PLA_KEYW(name, str_) [PlaKeyw##name] = { .size = _countof(str_) - 1, .str = str_ },
#include "pla_keyw.def"
#undef PLA_PUNC
};
