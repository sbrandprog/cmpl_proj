#include "cmpl_lib/cmpl_lib.h"

static lnk_pel_t pel;
static mc_pea_t pea;
static ira_inst_t ira_inst;
static ira_pec_t pec;
static pla_ec_t ec;
static pla_ec_buf_t ec_buf;
static pla_ec_fmtr_t ec_fmtr;
static pla_ec_rcvr_t ec_rcvr;
static pla_ec_sndr_t ec_sndr;
static pla_tltr_t tltr;
static pla_prsr_t prsr;
static pla_lex_t lex;
static pla_repo_t repo;

int main() {
	lnk_pel_cleanup(&pel);

	mc_pea_cleanup(&pea);

	ira_inst_cleanup(&ira_inst);

	ira_pec_cleanup(&pec);

	pla_ec_cleanup(&ec);

	pla_ec_buf_cleanup(&ec_buf);

	pla_ec_fmtr_cleanup(&ec_fmtr);

	pla_ec_rcvr_cleanup(&ec_rcvr);

	pla_ec_sndr_cleanup(&ec_sndr);

	pla_tltr_cleanup(&tltr);

	pla_prsr_cleanup(&prsr);

	pla_lex_cleanup(&lex);

	pla_repo_cleanup(&repo);

	return 0;
}
