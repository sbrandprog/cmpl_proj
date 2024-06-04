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
	pla_ec_pos_t pos_start;
	pla_ec_pos_t pos_end;
	ul_hs_t * msg;
	size_t start_line;
	size_t shift_size;
	bool shift_rev;
};
struct pla_ec_err {
	size_t group;

	pla_ec_pos_t pos_start;
	pla_ec_pos_t pos_end;

	ul_hs_t * msg;
};
struct pla_ec {
	void * user_data;

	pla_ec_do_actn_proc_t * actn_proc;
};

bool pla_ec_pos_is_less(pla_ec_pos_t * first, pla_ec_pos_t * second);
bool pla_ec_err_is_less(pla_ec_err_t * first, pla_ec_err_t * second);

void pla_ec_init(pla_ec_t * ec, void * user_data, pla_ec_do_actn_proc_t * actn_proc);
void pla_ec_cleanup(pla_ec_t * ec);

inline void pla_ec_do_actn(pla_ec_t * ec, pla_ec_actn_t * actn) {
	ec->actn_proc(ec->user_data, actn);
}
inline void pla_ec_post(pla_ec_t * ec, size_t group, pla_ec_pos_t pos_start, pla_ec_pos_t pos_end, ul_hs_t * msg) {
	pla_ec_actn_t actn = { .type = PlaEcActnPost, .group = group, .pos_start = pos_start, .pos_end = pos_end, .msg = msg };
	
	pla_ec_do_actn(ec, &actn);
}
inline void pla_ec_shift(pla_ec_t * ec, size_t group, size_t start_line, size_t shift_size, bool shift_rev) {
	pla_ec_actn_t actn = { .type = PlaEcActnShift, .group = group, .start_line = start_line, .shift_size = shift_size, .shift_rev = shift_rev };

	pla_ec_do_actn(ec, &actn);
}
inline void pla_ec_clear(pla_ec_t * ec, size_t group, pla_ec_pos_t pos_start, pla_ec_pos_t pos_end) {
	pla_ec_actn_t actn = { .type = PlaEcActnClear, .group = group, .pos_start = pos_start, .pos_end = pos_end };

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
