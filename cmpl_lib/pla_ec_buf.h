#pragma once
#include "pla_ec.h"

struct pla_ec_buf {
	CRITICAL_SECTION lock;

	size_t errs_size;
	pla_ec_err_t * errs;
	size_t errs_cap;

	pla_ec_t ec;
};

PLA_API void pla_ec_buf_init(pla_ec_buf_t * buf);
PLA_API void pla_ec_buf_cleanup(pla_ec_buf_t * buf);

PLA_API void pla_ec_buf_repost(pla_ec_buf_t * buf, pla_ec_t * ec);
