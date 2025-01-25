#pragma once
#include "pla_ec.h"

struct pla_ec_prntr
{
    pla_repo_t * repo;
    ul_ec_prntr_t prntr;
};

PLA_API void pla_ec_prntr_init(pla_ec_prntr_t * prntr, pla_repo_t * repo);
PLA_API void pla_ec_prntr_cleanup(pla_ec_prntr_t * prntr);
