#pragma once
#include "ira_pec.h"

IRA_API mc_frag_t * ira_pec_c_get_frag(ira_pec_c_ctx_t * ctx, mc_frag_type_t frag_type, ul_hs_t * frag_label);

IRA_API bool ira_pec_c_process_val_compl(ira_pec_c_ctx_t * ctx, ira_val_t * val);
IRA_API bool ira_pec_c_compile_val_frag(ira_pec_c_ctx_t * ctx, ira_val_t * val, ul_hs_t * hint_name, ul_hs_t ** out_label);

IRA_API ul_hsb_t * ira_pec_c_get_hsb(ira_pec_c_ctx_t * ctx);
IRA_API ul_hst_t * ira_pec_c_get_hst(ira_pec_c_ctx_t * ctx);
IRA_API ira_pec_t * ira_pec_c_get_pec(ira_pec_c_ctx_t * ctx);

IRA_API void ira_pec_c_push_cl_elem(ira_pec_c_ctx_t * ctx, ira_lo_t * lo);

IRA_API bool ira_pec_c_compile(ira_pec_t * pec, mc_pea_t * out);
