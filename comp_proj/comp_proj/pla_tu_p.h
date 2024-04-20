#include "pla_tu.h"

typedef bool pla_tu_p_get_tok_proc_t(void * src_data, pla_tok_t * out);

bool pla_tu_p_parse(ul_hs_t * tu_name, pla_tu_p_get_tok_proc_t * get_tok_proc, void * src_data, pla_tu_t ** out);
