#pragma once
#include "pla.h"

#define PLA_EC_GROUP_ALL 0

struct pla_ec_pos {
	size_t line_num;
	size_t line_ch;
};
enum pla_ec_actn_type {
	PlaEcActnPost,
	PlaEcActnShift,
	PlaEcActnClear,
	PlaEcActn_Count
};
struct pla_ec_actn {
	pla_ec_actn_type_t type;
	size_t group;
	union {
		struct {
			ul_hs_t * src_name;
			pla_ec_pos_t pos_start;
			pla_ec_pos_t pos_end;
			ul_hs_t * msg;
		} post;
		struct {
			size_t start_line;
			size_t size;
			bool rev;
		} shift;
		struct {
			pla_ec_pos_t pos_start;
			pla_ec_pos_t pos_end;
		} clear;
	};
};
struct pla_ec_err {
	size_t group;

	ul_hs_t * src_name;

	pla_ec_pos_t pos_start;
	pla_ec_pos_t pos_end;

	ul_hs_t * msg;
};
struct pla_ec {
	void * user_data;

	pla_ec_do_actn_proc_t * actn_proc;
};

PLA_API bool pla_ec_pos_is_less(const pla_ec_pos_t * first, const pla_ec_pos_t * second);
PLA_API bool pla_ec_err_is_less(const pla_ec_err_t * first, const pla_ec_err_t * second);

PLA_API void pla_ec_init(pla_ec_t * ec, void * user_data, pla_ec_do_actn_proc_t * actn_proc);
PLA_API void pla_ec_cleanup(pla_ec_t * ec);

inline void pla_ec_do_actn(pla_ec_t * ec, pla_ec_actn_t * actn) {
	ec->actn_proc(ec->user_data, actn);
}
inline void pla_ec_post(pla_ec_t * ec, size_t group, ul_hs_t * src_name, pla_ec_pos_t pos_start, pla_ec_pos_t pos_end, ul_hs_t * msg) {
	pla_ec_actn_t actn = { .type = PlaEcActnPost, .group = group, .post = { .src_name = src_name, .pos_start = pos_start, .pos_end = pos_end, .msg = msg } };
	
	pla_ec_do_actn(ec, &actn);
}
inline void pla_ec_shift(pla_ec_t * ec, size_t group, size_t start_line, size_t shift_size, bool shift_rev) {
	pla_ec_actn_t actn = { .type = PlaEcActnShift, .group = group, .shift = { .start_line = start_line, .size = shift_size, .rev = shift_rev } };

	pla_ec_do_actn(ec, &actn);
}
inline void pla_ec_clear(pla_ec_t * ec, size_t group, pla_ec_pos_t pos_start, pla_ec_pos_t pos_end) {
	pla_ec_actn_t actn = { .type = PlaEcActnClear, .group = group, .clear = { .pos_start = pos_start, .pos_end = pos_end } };

	pla_ec_do_actn(ec, &actn);
}
inline void pla_ec_clear_group(pla_ec_t * ec, size_t group) {
	pla_ec_pos_t pos_start = { .line_num = 0, .line_ch = 0 }, pos_end = { .line_num = SIZE_MAX, .line_ch = SIZE_MAX };

	pla_ec_clear(ec, group, pos_start, pos_end);
}
inline void pla_ec_clear_all(pla_ec_t * ec) {
	pla_ec_clear_group(ec, PLA_EC_GROUP_ALL);
}
inline void pla_ec_clear_line(pla_ec_t * ec, size_t group, size_t line_num) {
	pla_ec_pos_t pos_start = { .line_num = line_num, .line_ch = 0 }, pos_end = { .line_num = line_num, .line_ch = SIZE_MAX };

	pla_ec_clear(ec, group, pos_start, pos_end);
}
