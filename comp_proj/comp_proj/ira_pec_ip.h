#pragma once
#include "ira_pec.h"
#include "asm.h"

bool ira_pec_ip_compile(ira_pec_c_ctx_t * ctx, ira_lo_t * lo);
bool ira_pec_ip_interpret(ira_pec_t * pec, ira_func_t * func, ira_val_t ** out);
