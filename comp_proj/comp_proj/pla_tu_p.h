#include "pla_tu.h"

typedef bool pla_tu_p_get_tok_proc_t(void * src_data, pla_tok_t * out);

bool pla_tu_p_parse(pla_tu_t * tu, pla_tu_p_get_tok_proc_t * get_tok_proc, void * src_data);
