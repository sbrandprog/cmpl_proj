#include "cmpl_lib/cmpl_lib.h"

static lnk_pel_t pel;
static mc_pea_t pea;
static ira_inst_t ira_inst;
static ira_pec_t pec;
static pla_tltr_t tltr;
static pla_prsr_t prsr;
static pla_lex_t lex;
static pla_repo_t repo;

int main() {
	lnk_pel_cleanup(&pel);

	mc_pea_cleanup(&pea);

	ira_inst_cleanup(&ira_inst);

	ira_pec_cleanup(&pec);

	pla_tltr_cleanup(&tltr);

	pla_prsr_cleanup(&prsr);

	pla_lex_cleanup(&lex);

	pla_repo_cleanup(&repo);

	return 0;
}
