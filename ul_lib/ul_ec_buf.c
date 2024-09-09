#include "ul_assert.h"
#include "ul_ec_prntr.h"
#include "ul_ec_buf.h"

static void process_actn_post(ul_ec_buf_t * buf, const ul_ec_actn_t * actn) {
	if (actn->post.rec->mod_name != NULL) {
		*buf->rec_ins = ul_ec_rec_copy(actn->post.rec);
		buf->rec_ins = &(*buf->rec_ins)->next;
	}
}
static void process_actn_clear(ul_ec_buf_t * buf, const ul_ec_actn_t * actn) {
	ul_ec_rec_t ** rec_ins = &buf->rec;

	while (*rec_ins != NULL) {
		ul_ec_rec_t * rec = *rec_ins;

		if ((actn->clear.type == NULL || wcscmp(rec->type, actn->clear.type) == 0)
			&& (actn->clear.mod_name == NULL || wcscmp(rec->mod_name, actn->clear.mod_name) == 0)) {
			*rec_ins = rec->next;
			ul_ec_rec_destroy(rec);
		}
		else {
			rec_ins = &rec->next;
		}
	}
}
static void process_actn_proc(ul_ec_buf_t * buf, const ul_ec_actn_t * actn) {
	switch (actn->type) {
		case UlEcActnPost:
			process_actn_post(buf, actn);
			break;
		case UlEcActnClear:
			process_actn_clear(buf, actn);
			break;
		default:
			ul_assert_unreachable();
	}
}

void ul_ec_buf_init(ul_ec_buf_t * buf) {
	*buf = (ul_ec_buf_t){ 0 };

	buf->rec_ins = &buf->rec;

	ul_ec_init(&buf->ec, buf, process_actn_proc);
}
void ul_ec_buf_cleanup(ul_ec_buf_t * buf) {
	ul_ec_rec_destroy_chain(buf->rec);

	ul_ec_cleanup(&buf->ec);

	memset(buf, 0, sizeof(*buf));
}

void ul_ec_buf_repost(ul_ec_buf_t * buf, ul_ec_t * ec) {
	for (ul_ec_rec_t * rec = buf->rec; rec != NULL; rec = rec->next) {
		ul_ec_post(ec, rec);
	}
}

void ul_ec_buf_print(ul_ec_buf_t * buf, size_t prntrs_size, ul_ec_prntr_t ** prntrs) {
	for (ul_ec_rec_t * rec = buf->rec; rec != NULL; rec = rec->next) {
		ul_ec_prntr_t ** prntr = prntrs, ** prntr_end = prntr + prntrs_size;

		for (; prntr != prntr_end; ++prntr) {
			if (ul_ec_prntr_print(*prntr, rec)) {
				break;
			}
		}

		if (prntr == prntr_end) {
			wprintf(L"print error: no printer assigned for type [%s]\n", rec->type);
			ul_ec_rec_dump(rec);
		}

		putwchar(L'\n');
	}
}
