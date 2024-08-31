#include "ul_lib/ul_lib.h"

static ul_es_ctx_t es_ctx;
static ul_hsb_t hsb;
static ul_hst_t hst;

int main() {
	ul_es_cleanup_ctx(&es_ctx);

	ul_hsb_cleanup(&hsb);

	ul_hst_cleanup(&hst);

	return 0;
}
