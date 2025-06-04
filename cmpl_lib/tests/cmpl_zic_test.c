#include "cmpl_lib/cmpl_lib.h"

static lnk_pel_t pel;
static mc_inst_t mc_inst;
static mc_pea_t pea;
static ira_inst_t ira_inst;
static ira_pec_t pec;
static pla_tltr_t tltr;
static pla_prsr_src_t prsr_src;
static pla_prsr_t prsr;
static pla_lex_src_t lex_src;
static pla_lex_t lex;
static pla_tok_t tok;
static pla_repo_t repo;

int main()
{
    lnk_pel_cleanup(&pel);

	mc_inst_cleanup(&mc_inst);

    mc_pea_cleanup(&pea);

    ira_inst_cleanup(&ira_inst);

    ira_pec_cleanup(&pec);

    pla_tltr_cleanup(&tltr);

	pla_prsr_src_cleanup(&prsr_src);

    pla_prsr_cleanup(&prsr);

	pla_lex_src_cleanup(&lex_src);

    pla_lex_cleanup(&lex);

	pla_tok_cleanup(&tok);

    pla_repo_cleanup(&repo);

    return 0;
}
