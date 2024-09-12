#include "mc_pea.h"
#include "mc_frag.h"

void mc_pea_init(mc_pea_t * pea, ul_hst_t * hst, ul_ec_fmtr_t * ec_fmtr) {
	*pea = (mc_pea_t){ .hst = hst, .ec_fmtr = ec_fmtr };

	mc_it_init(&pea->it, hst, ec_fmtr);
}
void mc_pea_cleanup(mc_pea_t * pea) {
	mc_it_cleanup(&pea->it);

	mc_frag_destroy_chain(pea->frag);

	memset(pea, 0, sizeof(*pea));
}
