#include "pla_ec_buf.h"

static void insert_err_sorted_nl(pla_ec_buf_t * buf, pla_ec_err_t * err) {
	ul_arr_grow(buf->errs_size + 1, &buf->errs_cap, &buf->errs, sizeof(*buf->errs));

	size_t ins_pos = ul_bs_upper_bound(sizeof(*buf->errs), buf->errs_size, buf->errs, pla_ec_err_is_less, err);

	memmove_s(buf->errs + ins_pos + 1, (buf->errs_cap - ins_pos) * sizeof(*buf->errs), buf->errs + ins_pos, (buf->errs_size - ins_pos) * sizeof(*buf->errs));

	buf->errs[ins_pos] = *err;

	++buf->errs_size;
}

static void post_err_nl(pla_ec_buf_t * buf, pla_ec_actn_t * actn) {
	if (actn->group == PLA_EC_GROUP_ALL) {
		return;
	}
	
	pla_ec_err_t err = { .group = actn->group, .src_name = actn->post.src_name, .pos_start = actn->post.pos_start, .pos_end = actn->post.pos_end, .msg = actn->post.msg };

	insert_err_sorted_nl(buf, &err);
}
static void shift_lines_nl(pla_ec_buf_t * buf, pla_ec_actn_t * actn) {
	const size_t start_line = actn->shift.start_line, shift_size = actn->shift.size;
	const bool shift_rev = actn->shift.rev;

	for (size_t err_i = 0; err_i < buf->errs_size; ++err_i) {
		pla_ec_err_t * err = &buf->errs[err_i];

		if ((actn->group == PLA_EC_GROUP_ALL || err->group == actn->group)
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
static void clear_errs_nl(pla_ec_buf_t * buf, pla_ec_actn_t * actn) {
	const pla_ec_pos_t * const pos_start = &actn->clear.pos_start, * const pos_end = &actn->clear.pos_end;

	for (size_t err_i = 0; err_i < buf->errs_size; ) {
		pla_ec_err_t * err = &buf->errs[err_i];

		if ((actn->group == PLA_EC_GROUP_ALL || err->group == actn->group)
			&& !pla_ec_pos_is_less(&err->pos_start, pos_start)
			&& !pla_ec_pos_is_less(pos_end, &err->pos_start)) {
			memmove_s(err, (buf->errs_cap - err_i) * sizeof(*buf->errs), err + 1, (buf->errs_size - err_i - 1) * sizeof(*buf->errs));

			--buf->errs_size;
		}
		else {
			++err_i;
		}
	}
}

static void process_actn_nl(pla_ec_buf_t * buf, pla_ec_actn_t * actn) {
	switch (actn->type) {
		case PlaEcActnPost:
			post_err_nl(buf, actn);
			break;
		case PlaEcActnShift:
			shift_lines_nl(buf, actn);
			break;
		case PlaEcActnClear:
			clear_errs_nl(buf, actn);
			break;
		default:
			ul_assert_unreachable();
	}
}
static void process_actn_proc(void * user_data, pla_ec_actn_t * actn) {
	pla_ec_buf_t * buf = user_data;

	EnterCriticalSection(&buf->lock);

	process_actn_nl(buf, actn);

	LeaveCriticalSection(&buf->lock);
}

void pla_ec_buf_init(pla_ec_buf_t * buf) {
	*buf = (pla_ec_buf_t){ 0 };

	pla_ec_init(&buf->ec, buf, process_actn_proc);

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
		pla_ec_post(ec, err->group, err->src_name, err->pos_start, err->pos_end, err->msg);
	}
}
void pla_ec_buf_repost(pla_ec_buf_t * buf, pla_ec_t * ec) {
	ul_assert(&buf->ec != ec);

	EnterCriticalSection(&buf->lock);

	repost_nl(buf, ec);

	LeaveCriticalSection(&buf->lock);
}
