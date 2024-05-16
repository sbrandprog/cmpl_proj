#pragma once
#include "pla.h"

#define PLA_EC_GROUP_ALL 0

typedef void pla_ec_post_proc_t(void * user_data, size_t group, pla_ec_pos_t pos_start, pla_ec_pos_t pos_end, ul_hs_t * msg);
typedef void pla_ec_shift_proc_t(void * user_data, size_t group, size_t start_line, size_t shift_size, bool shift_rev);
typedef void pla_ec_clear_proc_t(void * user_data, size_t group, pla_ec_pos_t pos_start, pla_ec_pos_t pos_end);
typedef void pla_ec_sndr_detach_proc_t(void * sndr_data, pla_ec_t * ec);

struct pla_ec_pos {
	size_t line_num;
	size_t line_ch;
};
struct pla_ec_err {
	size_t group;

	pla_ec_pos_t pos_start;
	pla_ec_pos_t pos_end;

	ul_hs_t * msg;
};
struct pla_ec {
	void * user_data;

	pla_ec_post_proc_t * post_proc;
	pla_ec_shift_proc_t * shift_proc;
	pla_ec_clear_proc_t * clear_proc;

	void * sndr_data;
	pla_ec_sndr_detach_proc_t * sndr_detach_proc;
};

bool pla_ec_pos_is_less(pla_ec_pos_t * first, pla_ec_pos_t * second);
bool pla_ec_err_is_less(pla_ec_err_t * first, pla_ec_err_t * second);

void pla_ec_init(pla_ec_t * ec, void * user_data, pla_ec_post_proc_t * post_proc, pla_ec_shift_proc_t * shift_proc, pla_ec_clear_proc_t * clear_proc);
void pla_ec_cleanup(pla_ec_t * ec);

inline void pla_ec_post(pla_ec_t * ec, size_t group, pla_ec_pos_t pos_start, pla_ec_pos_t pos_end, ul_hs_t * msg) {
	ec->post_proc(ec->user_data, group, pos_start, pos_end, msg);
}
inline void pla_ec_shift(pla_ec_t * ec, size_t group, size_t start_line, size_t shift_size, bool shift_rev) {
	ec->shift_proc(ec->user_data, group, start_line, shift_size, shift_rev);
}
inline void pla_ec_clear(pla_ec_t * ec, size_t group, pla_ec_pos_t pos_start, pla_ec_pos_t pos_end) {
	ec->clear_proc(ec->user_data, group, pos_start, pos_end);
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
