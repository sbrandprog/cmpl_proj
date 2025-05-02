#pragma once
#include "ira.h"
#include "pla_bs.h"

#define PLA_BS_LPT_MOD_NAME "pla_bs_lpt"

PLA_API bool pla_bs_lpt_form_pec_nl(pla_repo_t * repo, ul_ec_fmtr_t * ec_fmtr, ul_hs_t * first_tus_name, ira_pec_t * out);
