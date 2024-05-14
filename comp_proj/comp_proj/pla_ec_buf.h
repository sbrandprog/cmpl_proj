#pragma once
#include "pla_ec.h"

struct pla_ec_buf_rcvr {
	pla_ec_t * ec;
};
struct pla_ec_buf {
	CRITICAL_SECTION lock;

	size_t errs_cap;
	pla_ec_err_t * errs;
	size_t errs_size;

	pla_ec_t ec;

	size_t rcvrs_cap;
	pla_ec_buf_rcvr_t * rcvrs;
	size_t rcvrs_size;
};

void pla_ec_buf_init(pla_ec_buf_t * buf);
void pla_ec_buf_cleanup(pla_ec_buf_t * buf);

void pla_ec_buf_attach_rcvr(pla_ec_buf_t * buf, pla_ec_t * ec);
