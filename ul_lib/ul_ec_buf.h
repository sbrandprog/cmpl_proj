#pragma once
#include "ul_ec.h"

struct ul_ec_buf {
	ul_ec_rec_t * rec;
	ul_ec_rec_t ** rec_ins;

	ul_ec_t ec;
};

UL_API void ul_ec_buf_init(ul_ec_buf_t * buf);
UL_API void ul_ec_buf_cleanup(ul_ec_buf_t * buf);

UL_API void ul_ec_buf_repost(ul_ec_buf_t * buf, ul_ec_t * ec);

UL_API void ul_ec_buf_print(ul_ec_buf_t * buf, size_t prntrs_size, ul_ec_prntr_t ** prntrs);
