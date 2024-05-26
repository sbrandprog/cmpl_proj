#include "pch.h"
#include "pla_ec_buf.h"

static void insert_err_sorted_nl(pla_ec_buf_t * buf, pla_ec_err_t * err) {
	if (buf->errs_size + 1 > buf->errs_cap) {
		ul_arr_grow(&buf->errs_cap, &buf->errs, sizeof(*buf->errs), 1);
	}

	size_t ins_pos = 0;

	while (ins_pos < buf->errs_size && !pla_ec_err_is_less(err, &buf->errs[ins_pos])) {
		++ins_pos;
	}

	memmove_s(buf->errs + ins_pos + 1, (buf->errs_cap - ins_pos) * sizeof(*buf->errs), buf->errs + ins_pos, (buf->errs_size - ins_pos) * sizeof(*buf->errs));

	buf->errs[ins_pos] = *err;

	++buf->errs_size;
}

static void post_err_nl(pla_ec_buf_t * buf, size_t group, pla_ec_pos_t pos_start, pla_ec_pos_t pos_end, ul_hs_t * msg) {
	if (group == PLA_EC_GROUP_ALL) {
		return;
	}
	
	pla_ec_err_t err = { .group = group, .pos_start = pos_start, .pos_end = pos_end, .msg = msg };

	insert_err_sorted_nl(buf, &err);
}
static void post_err_proc(void * user_data, size_t group, pla_ec_pos_t pos_start, pla_ec_pos_t pos_end, ul_hs_t * msg) {
	pla_ec_buf_t * buf = user_data;

	EnterCriticalSection(&buf->lock);

	__try {
		post_err_nl(buf, group, pos_start, pos_end, msg);
	}
	__finally {
		LeaveCriticalSection(&buf->lock);
	}
}

static void shift_lines_nl(pla_ec_buf_t * buf, size_t group, size_t start_line, size_t shift_size, bool shift_rev) {
	for (size_t err_i = 0; err_i < buf->errs_size; ++err_i) {
		pla_ec_err_t * err = &buf->errs[err_i];

		if ((group == PLA_EC_GROUP_ALL || err->group == group)
			&& err->pos_start.line_num >= start_line) {
			if (shift_rev) {
				err->pos_start.line_num -= min(err->pos_start.line_num, shift_size);
				err->pos_end.line_num -= min(err->pos_end.line_num, shift_size);
			}
			else {
				err->pos_start.line_num += shift_size;
				err->pos_end.line_num += shift_size;
			}
		}
	}
}
static void shift_lines_proc(void * user_data, size_t group, size_t start_line, size_t shift_size, bool shift_rev) {
	pla_ec_buf_t * buf = user_data;

	EnterCriticalSection(&buf->lock);

	__try {
		shift_lines_nl(buf, group, start_line, shift_size, shift_rev);
	}
	__finally {
		LeaveCriticalSection(&buf->lock);
	}
}

static void clear_errs_nl(pla_ec_buf_t * buf, size_t group, pla_ec_pos_t pos_start, pla_ec_pos_t pos_end) {
	for (size_t err_i = 0; err_i < buf->errs_size; ) {
		pla_ec_err_t * err = &buf->errs[err_i];

		if ((group == PLA_EC_GROUP_ALL || err->group == group)
			&& !pla_ec_pos_is_less(&err->pos_start, &pos_start)
			&& !pla_ec_pos_is_less(&pos_end, &err->pos_start)) {
			memmove_s(err, (buf->errs_cap - err_i) * sizeof(*buf->errs), err + 1, (buf->errs_size - err_i - 1) * sizeof(*buf->errs));

			--buf->errs_size;
		}
		else {
			++err_i;
		}
	}
}
static void clear_errs_proc(void * user_data, size_t group, pla_ec_pos_t pos_start, pla_ec_pos_t pos_end) {
	pla_ec_buf_t * buf = user_data;

	EnterCriticalSection(&buf->lock);

	__try {
		clear_errs_nl(buf, group, pos_start, pos_end);
	}
	__finally {
		LeaveCriticalSection(&buf->lock);
	}
}

void pla_ec_buf_init(pla_ec_buf_t * buf) {
	*buf = (pla_ec_buf_t){ 0 };

	pla_ec_init(&buf->ec, buf, post_err_proc, shift_lines_proc, clear_errs_proc);

	InitializeCriticalSection(&buf->lock);
}
void pla_ec_buf_cleanup(pla_ec_buf_t * buf) {
	DeleteCriticalSection(&buf->lock);

	pla_ec_cleanup(&buf->ec);

	free(buf->errs);

	memset(buf, 0, sizeof(*buf));
}

static void repost_nl(pla_ec_buf_t * buf, pla_ec_t * ec) {
	for (pla_ec_err_t * err = buf->errs, *err_end = err + buf->errs_size; err != err_end; ++err) {
		pla_ec_post(ec, err->group, err->pos_start, err->pos_end, err->msg);
	}
}
void pla_ec_buf_repost(pla_ec_buf_t * buf, pla_ec_t * ec) {
	ul_assert(&buf->ec != ec);

	EnterCriticalSection(&buf->lock);

	__try {
		repost_nl(buf, ec);
	}
	__finally {
		LeaveCriticalSection(&buf->lock);
	}
}
