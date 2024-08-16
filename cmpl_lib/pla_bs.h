#pragma once
#include "pla.h"

PLA_API void pla_bs_print_ec_buf_errs(pla_repo_t * repo, pla_ec_buf_t * buf);

PLA_API bool pla_bs_build_nl(pla_repo_t * repo, ul_hs_t * first_tus_name, const wchar_t * file_name);
