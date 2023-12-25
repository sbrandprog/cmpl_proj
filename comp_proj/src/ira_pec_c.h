#pragma once
#include "ira_pec.h"
#include "asm.h"
#include "u_hsb.h"

bool ira_pec_c_compile_int_arr(ira_pec_c_ctx_t * ctx, ira_val_t * arr, u_hs_t ** out_label);

u_hsb_t * ira_pec_c_get_hsb(ira_pec_c_ctx_t * ctx);
u_hst_t * ira_pec_c_get_hst(ira_pec_c_ctx_t * ctx);
ira_pec_t * ira_pec_c_get_pec(ira_pec_c_ctx_t * ctx);
asm_pea_t * ira_pec_c_get_pea(ira_pec_c_ctx_t * ctx);

bool ira_pec_c_compile(ira_pec_t * pec, asm_pea_t * out);
