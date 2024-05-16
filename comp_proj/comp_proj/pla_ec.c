#include "pch.h"
#include "pla_ec.h"

bool pla_ec_pos_is_less(pla_ec_pos_t * first, pla_ec_pos_t * second) {
	if (first->line_num < second->line_num) {
		return true;
	}

	if (first->line_num == second->line_num
		&& first->line_ch < second->line_ch) {
		return true;
	}

	return false;
}
bool pla_ec_err_is_less(pla_ec_err_t * first, pla_ec_err_t * second) {
	return pla_ec_pos_is_less(&first->pos_start, &second->pos_start);
}

void pla_ec_init(pla_ec_t * ec, void * user_data, pla_ec_post_proc_t * post_proc, pla_ec_shift_proc_t * shift_proc, pla_ec_clear_proc_t * clear_proc) {
	*ec = (pla_ec_t){ .user_data = user_data, .post_proc = post_proc, .shift_proc = shift_proc, .clear_proc = clear_proc };
}
void pla_ec_cleanup(pla_ec_t * ec) {
	if (ec->sndr_data != NULL) {
		ec->sndr_detach_proc(ec->sndr_data, ec);
	}

	memset(ec, 0, sizeof(*ec));
}
