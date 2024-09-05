#include "ul_lib/ul_lib.h"

static ul_ec_t ec;
static ul_ec_buf_t ec_buf;
static ul_ec_fmtr_t ec_fmtr;
static ul_ec_rn_t ec_rn;
static ul_ec_sn_t ec_sn;
static ul_es_ctx_t es_ctx;
static ul_hsb_t hsb;
static ul_hst_t hst;

int main() {
	ul_ec_cleanup(&ec);

	ul_ec_buf_cleanup(&ec_buf);

	ul_ec_fmtr_cleanup(&ec_fmtr);

	ul_ec_rn_cleanup(&ec_rn);

	ul_ec_sn_cleanup(&ec_sn);

	ul_es_cleanup_ctx(&es_ctx);

	ul_hsb_cleanup(&hsb);

	ul_hst_cleanup(&hst);

	return 0;
}
