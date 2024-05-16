#include "pla_tu.h"

#define PLA_TU_P_EC_GROUP 2

typedef bool pla_tu_p_get_tok_proc_t(void * src_data, pla_tok_t * out);

bool pla_tu_p_parse(pla_ec_fmtr_t * ec_fmtr, pla_tu_t * tu, pla_tu_p_get_tok_proc_t * get_tok_proc, void * src_data);
